#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <mysql.h>
#include <setjmp.h>
#include "protocole.h" // contient la cle et la structure d'un message
#include <mysql.h>
int idQ,idShm,idSem;
TAB_CONNEXIONS *tab;
int user=0;

void afficheTab();
void handlerSIGINT(int i);
MYSQL* connexion;

int main()
{
  // Connection à la BD
  connexion = mysql_init(NULL);
  if (mysql_real_connect(connexion,"localhost","Student","PassStudent1_","PourStudent",0,0,0) == NULL)
  {
    fprintf(stderr,"(SERVEUR) Erreur de connexion à la base de données...\n");
    exit(1);  
  }

  // Armement des signaux
  struct sigaction A;
  A.sa_handler = handlerSIGINT;
  sigemptyset(&A.sa_mask);
  A.sa_flags = 0;
  if (sigaction(SIGINT,&A,NULL) == -1)
  {
    perror("Erreur de SIGINT");
    exit(1);
  }

  // creation des ressources
  fprintf(stderr,"(SERVEUR %d) Creation de la file de messages\n",getpid());
  if ((idQ = msgget(CLE,IPC_CREAT | IPC_EXCL | 0600)) == -1)  // CLE definie dans protocole.h
  {
    perror("(SERVEUR) Erreur de msgget");
    exit(1);
  }

  // Initilisation de la table de connexions
  tab = (TAB_CONNEXIONS*) malloc(sizeof(TAB_CONNEXIONS)); 

  for (int i=0 ; i<6 ; i++)
  {
    tab->connexions[i].pidFenetre = 0;
    strcpy(tab->connexions[i].nom,"");
    for (int j=0 ; j<5 ; j++) tab->connexions[i].autres[j] = 0;
    tab->connexions[i].pidModification = 0;
  }
  tab->pidServeur1 = getpid();
  tab->pidServeur2 = 0;
  tab->pidAdmin = 0;
  tab->pidPublicite = 0;

  afficheTab();

  // Creation du processus Publicite

  int i,k,j;
  MESSAGE m;
  MESSAGE reponse;
  char requete[200];
  MYSQL_RES  *resultat;
  MYSQL_ROW  tuple;
  PUBLICITE pub;

  while(1)
  {
  	fprintf(stderr,"(SERVEUR %d) Attente d'une requete...\n",getpid());
    if (msgrcv(idQ,&m,sizeof(MESSAGE)-sizeof(long),1,0) == -1)
    {
      perror("(SERVEUR) Erreur de msgrcv");
      msgctl(idQ,IPC_RMID,NULL);
      exit(1);
    }

    switch(m.requete)
    {
      case CONNECT :  {
                      fprintf(stderr,"(SERVEUR %d) Requete CONNECT reçue de %d\n",getpid(),m.expediteur);
                      for(i=0 ; i<6 ; i++)
                      {
                        if (tab->connexions[i].pidFenetre == 0)
                        {
                          tab->connexions[i].pidFenetre = m.expediteur; 
                          break;   
                        }
                      }
                      break; }

      case DECONNECT :  {
                      fprintf(stderr,"(SERVEUR %d) Requete DECONNECT reçue de %d\n",getpid(),m.expediteur);
                      for (int i=0 ; i<6 ; i++)
                      {
                        if(tab->connexions[i].pidFenetre == m.expediteur)
                        {
                          tab->connexions[i].pidFenetre = 0;
                          strcpy(tab->connexions[i].nom,"");
                          for (int j=0 ; j<5 ; j++) tab->connexions[i].autres[j] = 0;
                          tab->connexions[i].pidModification = 0;
                        }
                      }
                      break; }

      case LOGIN :  {
                      fprintf(stderr,"(SERVEUR %d) Requete LOGIN reçue de %d : --%s--%s--\n",getpid(),m.expediteur,m.data1,m.data2);
                      MYSQL *connexion = mysql_init(NULL);
                      if (mysql_real_connect(connexion,"localhost","Student","PassStudent1_","PourStudent",0,0,0) == NULL)
                      {
                        fprintf(stderr,"(SERVER) Erreur de connexion à la base de données...\n");
                        exit(1);  
                      }
                      sprintf(requete,"SELECT * FROM UNIX_FINAL WHERE nom like '%s' AND motdepasse like '%s'",m.data1,m.data2);
                      if (mysql_query(connexion,requete))
                      {
                        fprintf(stderr,"(SERVEUR) Erreur de requete SQL...\n");
                        exit(1);
                      }
                      resultat = mysql_store_result(connexion);
                      if (mysql_num_rows(resultat) == 0)
                      {
                        fprintf(stderr,"(SERVEUR) Erreur de login...\n");
                        reponse.type=m.expediteur;
                        reponse.requete = LOGIN;
                        reponse.expediteur = getpid();
                        strcpy(reponse.data1, "KO");
                        if (msgsnd(idQ,&reponse,sizeof(MESSAGE)-sizeof(long),0) == -1)
                        {
                          perror("SERVER -- Erreur de msgsnd");
                          exit(1);
                        }
                        kill(m.expediteur,SIGUSR1);
                      } 
                      else
                      {
                        fprintf(stderr,"(SERVEUR) Login OK...\n");
                        tuple = mysql_fetch_row(resultat);
                        reponse.type=m.expediteur;
                        reponse.requete = LOGIN;
                        reponse.expediteur = getpid();
                        strcpy(reponse.data1, "OK");
                        if (msgsnd(idQ,&reponse,sizeof(MESSAGE)-sizeof(long),0) == -1)
                        {
                          perror("SERVER -- Erreur de msgsnd");
                          exit(1);
                        }
                        for(int j=0;j<6;j++)
                        {
                          if(tab->connexions[j].pidFenetre == m.expediteur)
                          {
                            strcpy(tab->connexions[j].nom,m.data1);
                          }
                        }
                        fprintf(stderr,"(SERVEUR) Envoie SIGUSR1 à %d\n",m.expediteur);
                        kill(m.expediteur,SIGUSR1);

                        // SI le LOGIN est bon, on doit ajouter les USER deja connectés aux fenêtres

                        for(int i =0;i<6;i++)
                        {
                          if(tab->connexions[i].pidFenetre !=0)
                          {
                            if(strcmp(tab->connexions[i].nom,"")!=0)
                            {                          
                              if(tab->connexions[i].pidFenetre != m.expediteur)
                              {
                                fprintf(stderr,"(SERVEUR) Envoie ADDUSR à %d\n",tab->connexions[i].pidFenetre);
                                MESSAGE addusr;
                                addusr.type = tab->connexions[i].pidFenetre;
                                addusr.requete = ADD_USER;
                                addusr.expediteur = getpid();
                                strcpy(addusr.data1,m.data1);
                                if (msgsnd(idQ,&addusr,sizeof(MESSAGE)-sizeof(long),0) == -1)
                                {
                                  perror("SERVER -- Erreur de msgsnd");
                                  exit(1);
                                }
                                kill(tab->connexions[i].pidFenetre,SIGUSR1);
                                sleep(1);
                                
                                MESSAGE addusr2;
                                addusr2.type = m.expediteur;
                                addusr2.requete = ADD_USER;
                                addusr2.expediteur = getpid();
                                strcpy(addusr2.data1,tab->connexions[i].nom);
                                if (msgsnd(idQ,&addusr2,sizeof(MESSAGE)-sizeof(long),0) == -1)
                                {
                                  perror("SERVER -- Erreur de msgsnd");
                                  exit(1);
                                }
                                fprintf(stderr,"(SERVEUR) Envoie SIGUSR1 à %d\n",m.expediteur);
                                kill(m.expediteur,SIGUSR1);
                              }
                            }
                          }
                        }
                      }
                      
                      mysql_close(connexion);

                      

                                           
                      break; 
                    }
      case LOGOUT :  {
                      char nom[20];
                      
                      fprintf(stderr,"(SERVEUR %d) Requete LOGOUT reçue de %d\n",getpid(),m.expediteur);
                      for (int i=0 ; i<6 ; i++)
                      {
                        if(tab->connexions[i].pidFenetre == m.expediteur)
                        {
                          strcpy(nom,tab->connexions[i].nom);
                          strcpy(tab->connexions[i].nom,"");
                          tab->connexions[i].autres[0] = 0;
                          tab->connexions[i].autres[1] = 0;
                          tab->connexions[i].autres[2] = 0;
                          tab->connexions[i].autres[3] = 0;
                          tab->connexions[i].autres[4] = 0;
                        }
                      }

                      for(int i=0;i<6;i++)
                      {
                        if(tab->connexions[i].pidFenetre != m.expediteur )
                        {
                          if(tab->connexions[i].pidFenetre != 0)
                          {             
                            for(int j=0;j<5;j++)
                            {
                              if(tab->connexions[i].autres[j] == m.expediteur)
                              {
                                tab->connexions[i].autres[j] = 0;
                              }
                            }
                            sleep(1);
                            MESSAGE rmusr;
                            rmusr.type = tab->connexions[i].pidFenetre;
                            rmusr.requete = REMOVE_USER;
                            rmusr.expediteur = getpid();
                            strcpy(rmusr.data1,nom);
                            if (msgsnd(idQ,&rmusr,sizeof(MESSAGE)-sizeof(long),0) == -1)
                            {
                              perror("SERVER -- Erreur de msgsnd");
                              exit(1);
                            }
                            fprintf(stderr,"(SERVEUR) Envoie SIGUSR1 à %d\n",tab->connexions[i].pidFenetre);
                            kill(tab->connexions[i].pidFenetre,SIGUSR1);
                          }
                        }
                      }
                        
                      

                      break;}

      case ACCEPT_USER :
                      fprintf(stderr,"(SERVEUR %d) Requete ACCEPT_USER reçue de %d\n",getpid(),m.expediteur);
                      int tempAccept;
                      for(int i=0;i<6;i++)
                      {
                        if(strcmp(tab->connexions[i].nom,m.data1)==0)
                        {
                          fprintf(stderr,"Trouvé %s en pos %d\n",tab->connexions[i].nom,i);
                          tempAccept=tab->connexions[i].pidFenetre;
                        }
                      }

                      for(int i=0;i<6;i++)
                      {
                        if(tab->connexions[i].pidFenetre == m.expediteur)
                        {
                          for(int j=0;j<5;j++)
                          {
                            if(tab->connexions[i].autres[j] == 0)
                            {
                              tab->connexions[i].autres[j] = tempAccept;
                              break;
                            }
                          }
                        }
                      }
                      break;

      case REFUSE_USER :
                      fprintf(stderr,"(SERVEUR %d) Requete REFUSE_USER reçue de %d\n",getpid(),m.expediteur);
                      int tempRef;
                      if(strcmp(m.data1,"")==0)
                      {
                        break;
                      }
                      for(int i=0;i<6;i++)
                      {
                        if(strcmp(tab->connexions[i].nom,m.data1)==0)
                        {
                          fprintf(stderr,"Trouvé %s en pos %d\n",tab->connexions[i].nom,i);
                          tempRef=tab->connexions[i].pidFenetre;
                        }
                      }

                      for(int i=0;i<6;i++)
                      {
                        if(tab->connexions[i].pidFenetre == m.expediteur)
                        {
                          for(int j=0;j<5;j++)
                          {
                            if(tab->connexions[i].autres[j] == tempRef)
                            {
                              tab->connexions[i].autres[j] = 0;
                              break;
                            }
                          }
                        }
                      }
                      break;

      case SEND :  
                      fprintf(stderr,"(SERVEUR %d) Requete SEND reçue de %d\n",getpid(),m.expediteur);
                      for(int i=0;i<6;i++)
                      {
                        if(tab->connexions[i].pidFenetre == m.expediteur)
                        {
                          char nom[20];
                          strcpy(nom,tab->connexions[i].nom);
                          for(int j=0;j<5;j++)
                          {
                            if(tab->connexions[i].autres[j] != 0)
                            {
                              MESSAGE send;
                              send.type = tab->connexions[i].autres[j];
                              send.requete = SEND;
                              send.expediteur = getpid();
                              strcpy(send.texte,m.texte);
                              strcpy(send.data1,nom);
                              
                              if (msgsnd(idQ,&send,sizeof(MESSAGE)-sizeof(long),0) == -1)
                              {
                                perror("SERVER -- Erreur de msgsnd");
                                exit(1);
                              }
                              fprintf(stderr,"(SERVEUR) Envoie SIGUSR1 à %d\n",tab->connexions[i].autres[j]);
                              kill(tab->connexions[i].autres[j],SIGUSR1);
                            }
                          }
                        }
                      }
                          
                        
                      
                      break; 
  

      case UPDATE_PUB :
      {
                      fprintf(stderr,"(SERVEUR %d) Requete UPDATE_PUB reçue de %d\n",getpid(),m.expediteur);
                      break;}

      case CONSULT :
                      fprintf(stderr,"(SERVEUR %d) Requete CONSULT reçue de %d\n",getpid(),m.expediteur);
                      break;

      case MODIF1 :
                      fprintf(stderr,"(SERVEUR %d) Requete MODIF1 reçue de %d\n",getpid(),m.expediteur);
                      break;

      case MODIF2 :
                      fprintf(stderr,"(SERVEUR %d) Requete MODIF2 reçue de %d\n",getpid(),m.expediteur);
                      break;

      case LOGIN_ADMIN :
                      fprintf(stderr,"(SERVEUR %d) Requete LOGIN_ADMIN reçue de %d\n",getpid(),m.expediteur);
                      break;

      case LOGOUT_ADMIN :
                      fprintf(stderr,"(SERVEUR %d) Requete LOGOUT_ADMIN reçue de %d\n",getpid(),m.expediteur);
                      break;

      case NEW_USER :
                      fprintf(stderr,"(SERVEUR %d) Requete NEW_USER reçue de %d : --%s--%s--\n",getpid(),m.expediteur,m.data1,m.data2);
                      break;

      case DELETE_USER :
                      fprintf(stderr,"(SERVEUR %d) Requete DELETE_USER reçue de %d : --%s--\n",getpid(),m.expediteur,m.data1);
                      break;

      case NEW_PUB :
                      fprintf(stderr,"(SERVEUR %d) Requete NEW_PUB reçue de %d\n",getpid(),m.expediteur);
                      break;
    }
    afficheTab();
  }
  exit(0);
}
void afficheTab()
{
  fprintf(stderr,"Pid Serveur 1 : %d\n",tab->pidServeur1);
  fprintf(stderr,"Pid Serveur 2 : %d\n",tab->pidServeur2);
  fprintf(stderr,"Pid Publicite : %d\n",tab->pidPublicite);
  fprintf(stderr,"Pid Admin     : %d\n",tab->pidAdmin);
  for (int i=0 ; i<6 ; i++)
    fprintf(stderr,"%6d -%20s- %6d %6d %6d %6d %6d - %6d\n",tab->connexions[i].pidFenetre,
                                                      tab->connexions[i].nom,
                                                      tab->connexions[i].autres[0],
                                                      tab->connexions[i].autres[1],
                                                      tab->connexions[i].autres[2],
                                                      tab->connexions[i].autres[3],
                                                      tab->connexions[i].autres[4],
                                                      tab->connexions[i].pidModification);
  fprintf(stderr,"\n");
}

void handlerSIGINT(int sig)
{
  fprintf(stderr,"(SERVEUR %d) SIGINT reçu\n",getpid());
  if (msgctl(idQ,IPC_RMID,NULL) == -1)
  {
  perror("Erreur de msgctl(2)");
  exit(1);
  }
  fprintf(stderr,"(SERVEUR %d) Fermeture de la file de messages\n",getpid());
  exit(0);
}
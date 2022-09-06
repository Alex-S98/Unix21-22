#include "windowclient.h"
#include "ui_windowclient.h"
#include <QMessageBox>
#include "dialogmodification.h"
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extern WindowClient *w;

#include "protocole.h"

int idQ, idShm;
#define TIME_OUT 120
int timeOut = TIME_OUT;
int logged =0;

void handlerSIGUSR1(int sig);
void handlerSIGALRM(int sig);


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
WindowClient::WindowClient(QWidget *parent):QMainWindow(parent),ui(new Ui::WindowClient)
{
    ui->setupUi(this);
    logoutOK();

    // Recuperation de l'identifiant de la file de messages
    fprintf(stderr,"(CLIENT %d) Recuperation de l'id de la file de messages\n",getpid());
    if ((idQ = msgget(CLE,0)) == -1)
    {
      perror("Erreur de msgget");
      exit(1);
    }

    // Recuperation de l'identifiant de la mémoire partagée
    fprintf(stderr,"(CLIENT %d) Recuperation de l'id de la mémoire partagée\n",getpid());

    // Attachement à la mémoire partagée

    // Armement des signaux
    struct sigaction A;
    A.sa_handler = handlerSIGUSR1;
    sigemptyset(&A.sa_mask);
    A.sa_flags = 0;
    if (sigaction(SIGUSR1,&A,NULL) == -1)
    {
      perror("Erreur de SIGUSR1");
      exit(1);
    }

    struct sigaction B;
    B.sa_handler = handlerSIGALRM;
    sigemptyset(&B.sa_mask);
    B.sa_flags = 0;
    if (sigaction(SIGALRM,&B,NULL) == -1)
    {
      perror("Erreur de SIGALRM");
      exit(1);
    }

    // Envoi d'une requete de connexion au serveur
    MESSAGE requete;
    requete.expediteur = getpid();
    requete.requete = CONNECT;
    requete.type=1;
    if (msgsnd(idQ,&requete,sizeof(MESSAGE)-sizeof(long),0) == -1)
    {
      perror("SERVER -- Erreur de msgsnd");
      exit(1);
    }

}

WindowClient::~WindowClient()
{
    delete ui;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Fonctions utiles : ne pas modifier /////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setNom(const char* Text)
{
  if (strlen(Text) == 0 )
  {
    ui->lineEditNom->clear();
    return;
  }
  ui->lineEditNom->setText(Text);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* WindowClient::getNom()
{
  strcpy(connectes[0],ui->lineEditNom->text().toStdString().c_str());
  return connectes[0];
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setMotDePasse(const char* Text)
{
  if (strlen(Text) == 0 )
  {
    ui->lineEditMotDePasse->clear();
    return;
  }
  ui->lineEditMotDePasse->setText(Text);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* WindowClient::getMotDePasse()
{
  strcpy(motDePasse,ui->lineEditMotDePasse->text().toStdString().c_str());
  return motDePasse;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setPublicite(const char* Text)
{
  if (strlen(Text) == 0 )
  {
    ui->lineEditPublicite->clear();
    return;
  }
  ui->lineEditPublicite->setText(Text);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setTimeOut(int nb)
{
  ui->lcdNumberTimeOut->display(nb);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setAEnvoyer(const char* Text)
{
  //fprintf(stderr,"---%s---\n",Text);
  if (strlen(Text) == 0 )
  {
    ui->lineEditAEnvoyer->clear();
    return;
  }
  ui->lineEditAEnvoyer->setText(Text);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* WindowClient::getAEnvoyer()
{
  strcpy(aEnvoyer,ui->lineEditAEnvoyer->text().toStdString().c_str());
  return aEnvoyer;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setPersonneConnectee(int i,const char* Text)
{
  if (strlen(Text) == 0 )
  {
    switch(i)
    {
        case 1 : ui->lineEditConnecte1->clear(); break;
        case 2 : ui->lineEditConnecte2->clear(); break;
        case 3 : ui->lineEditConnecte3->clear(); break;
        case 4 : ui->lineEditConnecte4->clear(); break;
        case 5 : ui->lineEditConnecte5->clear(); break;
    }
    return;
  }
  switch(i)
  {
      case 1 : ui->lineEditConnecte1->setText(Text); break;
      case 2 : ui->lineEditConnecte2->setText(Text); break;
      case 3 : ui->lineEditConnecte3->setText(Text); break;
      case 4 : ui->lineEditConnecte4->setText(Text); break;
      case 5 : ui->lineEditConnecte5->setText(Text); break;
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* WindowClient::getPersonneConnectee(int i)
{
  QLineEdit *tmp;
  switch(i)
  {
    case 1 : tmp = ui->lineEditConnecte1; break;
    case 2 : tmp = ui->lineEditConnecte2; break;
    case 3 : tmp = ui->lineEditConnecte3; break;
    case 4 : tmp = ui->lineEditConnecte4; break;
    case 5 : tmp = ui->lineEditConnecte5; break;
    default : return NULL;
  }

  strcpy(connectes[i],tmp->text().toStdString().c_str());
  return connectes[i];
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::ajouteMessage(const char* personne,const char* message)
{
  // Choix de la couleur en fonction de la position
  int i=1;
  bool trouve=false;
  while (i<=5 && !trouve)
  {
      if (getPersonneConnectee(i) != NULL && strcmp(getPersonneConnectee(i),personne) == 0) trouve = true;
      else i++;
  }
  char couleur[40];
  if (trouve)
  {
      switch(i)
      {
        case 1 : strcpy(couleur,"<font color=\"red\">"); break;
        case 2 : strcpy(couleur,"<font color=\"blue\">"); break;
        case 3 : strcpy(couleur,"<font color=\"green\">"); break;
        case 4 : strcpy(couleur,"<font color=\"darkcyan\">"); break;
        case 5 : strcpy(couleur,"<font color=\"orange\">"); break;
      }
  }
  else strcpy(couleur,"<font color=\"black\">");
  if (strcmp(getNom(),personne) == 0) strcpy(couleur,"<font color=\"purple\">");

  // ajout du message dans la conversation
  char buffer[300];
  sprintf(buffer,"%s(%s)</font> %s",couleur,personne,message);
  ui->textEditConversations->append(buffer);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setNomRenseignements(const char* Text)
{
  if (strlen(Text) == 0 )
  {
    ui->lineEditNomRenseignements->clear();
    return;
  }
  ui->lineEditNomRenseignements->setText(Text);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* WindowClient::getNomRenseignements()
{
  strcpy(nomR,ui->lineEditNomRenseignements->text().toStdString().c_str());
  return nomR;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setGsm(const char* Text)
{
  if (strlen(Text) == 0 )
  {
    ui->lineEditGsm->clear();
    return;
  }
  ui->lineEditGsm->setText(Text);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setEmail(const char* Text)
{
  if (strlen(Text) == 0 )
  {
    ui->lineEditEmail->clear();
    return;
  }
  ui->lineEditEmail->setText(Text);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setCheckbox(int i,bool b)
{
  QCheckBox *tmp;
  switch(i)
  {
    case 1 : tmp = ui->checkBox1; break;
    case 2 : tmp = ui->checkBox2; break;
    case 3 : tmp = ui->checkBox3; break;
    case 4 : tmp = ui->checkBox4; break;
    case 5 : tmp = ui->checkBox5; break;
    default : return;
  }
  tmp->setChecked(b);
  if (b) tmp->setText("Accepté");
  else tmp->setText("Refusé");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::loginOK()
{
  ui->pushButtonLogin->setEnabled(false);
  ui->pushButtonLogout->setEnabled(true);
  ui->lineEditNom->setReadOnly(true);
  ui->lineEditMotDePasse->setReadOnly(true);
  ui->pushButtonEnvoyer->setEnabled(true);
  ui->pushButtonConsulter->setEnabled(true);
  ui->pushButtonModifier->setEnabled(true);
  ui->checkBox1->setEnabled(true);
  ui->checkBox2->setEnabled(true);
  ui->checkBox3->setEnabled(true);
  ui->checkBox4->setEnabled(true);
  ui->checkBox5->setEnabled(true);
  ui->lineEditAEnvoyer->setEnabled(true);
  ui->lineEditNomRenseignements->setEnabled(true);
  setTimeOut(TIME_OUT);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::logoutOK()
{
  ui->pushButtonLogin->setEnabled(true);
  ui->pushButtonLogout->setEnabled(false);
  ui->lineEditNom->setReadOnly(false);
  ui->lineEditNom->setText("");
  ui->lineEditMotDePasse->setReadOnly(false);
  ui->lineEditMotDePasse->setText("");
  ui->pushButtonEnvoyer->setEnabled(false);
  ui->pushButtonConsulter->setEnabled(false);
  ui->pushButtonModifier->setEnabled(false);
  for (int i=1 ; i<=5 ; i++)
  {
      setCheckbox(i,false);
      setPersonneConnectee(i,"");
  }
  ui->checkBox1->setEnabled(false);
  ui->checkBox2->setEnabled(false);
  ui->checkBox3->setEnabled(false);
  ui->checkBox4->setEnabled(false);
  ui->checkBox5->setEnabled(false);
  setNomRenseignements("");
  setGsm("");
  setEmail("");
  ui->textEditConversations->clear();
  setAEnvoyer("");
  ui->lineEditAEnvoyer->setEnabled(false);
  ui->lineEditNomRenseignements->setEnabled(false);
  setTimeOut(TIME_OUT);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Fonctions clics sur les boutons ////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::on_pushButtonLogin_clicked()
{
  // TO DO
  MESSAGE m;
  m.expediteur = getpid();
  m.requete = LOGIN;
  m.type = 1;
  strcpy(m.data1,getNom());
  strcpy(m.data2,getMotDePasse());
  if (msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long),0) == -1)
  {
    perror("CLIENT -- Erreur de msgsnd");
    exit(1);
  }


}

void WindowClient::on_pushButtonLogout_clicked()
{
  // TO DO
  MESSAGE m;
  m.expediteur = getpid();
  m.requete = LOGOUT;
  m.type = 1;
  if (msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long),0) == -1)
  {
    perror("CLIENT -- Erreur de msgsnd");
    exit(1);
  }

  logoutOK();
}

void WindowClient::on_pushButtonQuitter_clicked()
{
  // TO DO
  if(logged ==1)
  {
    MESSAGE m;
    m.expediteur = getpid();
    m.requete = LOGOUT;
    m.type = 1;
    if (msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long),0) == -1)
    {
      perror("CLIENT -- Erreur de msgsnd");
      exit(1);
    }
    logged =0;
  }

  MESSAGE requete;
    requete.expediteur = getpid();
    requete.requete = DECONNECT;
    requete.type=1;
    if (msgsnd(idQ,&requete,sizeof(MESSAGE)-sizeof(long),0) == -1)
    {
      perror("CLIENT -- Erreur de msgsnd");
      exit(1);
    }
    exit(0);
}

void WindowClient::on_pushButtonEnvoyer_clicked()
{
  // TO DO
  // Envoyer une requete de type SEND au server
  

  MESSAGE m;
  m.expediteur = getpid();
  m.requete = SEND;
  m.type = 1;
  strcpy(m.texte,getAEnvoyer());
  if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long),0) == -1)
  {
    perror("CLIENT -- Erreur de msgsnd SEND");
    exit(1);
  }
  ResetAlrm();
}

void WindowClient::on_pushButtonConsulter_clicked()
{
  // TO DO
  ResetAlrm();
}

void WindowClient::on_pushButtonModifier_clicked()
{
  // TO DO
  ResetAlrm();
  // Envoi d'une requete MODIF1 au serveur
  MESSAGE m;
  // ...

  // Attente d'une reponse en provenance de Modification
  fprintf(stderr,"(CLIENT %d) Attente reponse MODIF1\n",getpid());
  // ...

  // Verification si la modification est possible
  if (strcmp(m.data1,"KO") == 0 && strcmp(m.data2,"KO") == 0 && strcmp(m.texte,"KO") == 0)
  {
    QMessageBox::critical(w,"Problème...","Modification déjà en cours...");
    return;
  }

  // Modification des données par utilisateur
  DialogModification dialogue(this,getNom(),m.data1,m.data2,m.texte);
  dialogue.exec();
  char motDePasse[40];
  char gsm[40];
  char email[40];
  strcpy(motDePasse,dialogue.getMotDePasse());
  strcpy(gsm,dialogue.getGsm());
  strcpy(email,dialogue.getEmail());

  // Envoi des données modifiées au serveur
  // ...
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Fonctions clics sur les checkbox ///////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::on_checkBox1_clicked(bool checked)
{
  ResetAlrm();
    if (checked)
    {
        ui->checkBox1->setText("Accepté");
        // TO DO
        // send ACCEPT_USER to server
        MESSAGE m;
        m.expediteur = getpid();
        m.requete = ACCEPT_USER;
        m.type = 1;
        strcpy(m.data1,getPersonneConnectee(1));
        if (msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long),0) == -1)
        {
          perror("CLIENT -- Erreur de msgsnd");
          exit(1);
        }
    }
    else
    {
        ui->checkBox1->setText("Refusé");
        // TO DO
        // send REFUSE_USER to server
        MESSAGE m;
        m.expediteur = getpid();
        m.requete = REFUSE_USER;
        m.type = 1;
        strcpy(m.data1,getPersonneConnectee(1));
        if (msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long),0) == -1)
        {
          perror("CLIENT -- Erreur de msgsnd");
          exit(1);
        }
    }
}

void WindowClient::on_checkBox2_clicked(bool checked)
{
  ResetAlrm();
    if (checked)
    {
        ui->checkBox2->setText("Accepté");
        // TO DO
         MESSAGE m;
        m.expediteur = getpid();
        m.requete = ACCEPT_USER;
        m.type = 1;
        strcpy(m.data1,getPersonneConnectee(2));
        if (msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long),0) == -1)
        {
          perror("CLIENT -- Erreur de msgsnd");
          exit(1);
        }
    }
    else
    {
        ui->checkBox2->setText("Refusé");
        // TO DO
        MESSAGE m;
        m.expediteur = getpid();
        m.requete = REFUSE_USER;
        m.type = 1;
        strcpy(m.data1,getPersonneConnectee(2));
        if (msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long),0) == -1)
        {
          perror("CLIENT -- Erreur de msgsnd");
          exit(1);
        }
    }
}

void WindowClient::on_checkBox3_clicked(bool checked)
{
  ResetAlrm();
    if (checked)
    {
        ui->checkBox3->setText("Accepté");
        // TO DO
         MESSAGE m;
        m.expediteur = getpid();
        m.requete = ACCEPT_USER;
        m.type = 1;
        strcpy(m.data1,getPersonneConnectee(3));
        if (msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long),0) == -1)
        {
          perror("CLIENT -- Erreur de msgsnd");
          exit(1);
        }
    }
    else
    {
        ui->checkBox3->setText("Refusé");
        // TO DO
        MESSAGE m;
        m.expediteur = getpid();
        m.requete = REFUSE_USER;
        m.type = 1;
        strcpy(m.data1,getPersonneConnectee(3));
        if (msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long),0) == -1)
        {
          perror("CLIENT -- Erreur de msgsnd");
          exit(1);
        }
    }
}

void WindowClient::on_checkBox4_clicked(bool checked)
{
  ResetAlrm();
    if (checked)
    {
        ui->checkBox4->setText("Accepté");
        // TO DO
         MESSAGE m;
        m.expediteur = getpid();
        m.requete = ACCEPT_USER;
        m.type = 1;
        strcpy(m.data1,getPersonneConnectee(4));
        if (msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long),0) == -1)
        {
          perror("CLIENT -- Erreur de msgsnd");
          exit(1);
        }
    }
    else
    {
        ui->checkBox4->setText("Refusé");
        // TO DO
        MESSAGE m;
        m.expediteur = getpid();
        m.requete = REFUSE_USER;
        m.type = 1;
        strcpy(m.data1,getPersonneConnectee(4));
        if (msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long),0) == -1)
        {
          perror("CLIENT -- Erreur de msgsnd");
          exit(1);
        }
    }
}

void WindowClient::on_checkBox5_clicked(bool checked)
{
  ResetAlrm();
    if (checked)
    {
        ui->checkBox5->setText("Accepté");
        // TO DO
         MESSAGE m;
        m.expediteur = getpid();
        m.requete = ACCEPT_USER;
        m.type = 1;
        strcpy(m.data1,getPersonneConnectee(5));
        if (msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long),0) == -1)
        {
          perror("CLIENT -- Erreur de msgsnd");
          exit(1);
        }
    }
    else
    {
        ui->checkBox5->setText("Refusé");
        // TO DO
        MESSAGE m;
        m.expediteur = getpid();
        m.requete = REFUSE_USER;
        m.type = 1;
        strcpy(m.data1,getPersonneConnectee(5));
        if (msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long),0) == -1)
        {
          perror("CLIENT -- Erreur de msgsnd");
          exit(1);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Handlers de signaux ////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void handlerSIGUSR1(int sig)
{
  fprintf(stderr,"(CLIENT %d) Signal SIGUSR1 reçu\n",getpid());
    MESSAGE m;
    
    // ...msgrcv(idQ,&m,...)
    if (msgrcv(idQ,&m,sizeof(MESSAGE)-sizeof(long),getpid(),0) == -1)
    {
      perror("(CLIENT) Erreur de msgrcv");
      msgctl(idQ,IPC_RMID,NULL);
      exit(1);
    }
    fprintf(stderr,"(CLIENT %d) Message reçu: %s\n",getpid(),m.data1);
      switch(m.requete)
      {
        case LOGIN :
                    if (strcmp(m.data1,"OK") == 0)
                    {
                      fprintf(stderr,"(CLIENT %d) Login OK\n",getpid());
                      w->loginOK();
                      QMessageBox::information(w,"Login...","Login Ok ! Bienvenue...");
                      // ...
                      logged =1;
                      timeOut = TIME_OUT;
                      w->setTimeOut(TIME_OUT);
                      alarm(1);
                      
                    }
                    else QMessageBox::critical(w,"Login...","Erreur de login...");
                    break;

        case ADD_USER :
        {
                    // TO DO;
                    fprintf(stderr,"(CLIENT %d) Ajout d'un utilisateur\n",getpid());
                    for(int i=1;i<6;i++)
                    {
                      int comp= strcmp("",w->getPersonneConnectee(i));
                      if(comp==0)
                      {
                        fprintf(stderr,"Ajout en pos : %d\n",i);
                        w->setPersonneConnectee(i,m.data1);
                        break;
                      }
                    }
                    break;
        }
        case REMOVE_USER :
        {
                    // TO DO
                    fprintf(stderr,"(CLIENT %d) Suppression d'un utilisateur\n",getpid());
                    for(int i=1;i<6;i++)
                    {
                      int comp= strcmp(m.data1,w->getPersonneConnectee(i));
                      if(comp==0)
                      {
                        fprintf(stderr,"Retrait en pos : %d\n",i);
                        w->setPersonneConnectee(i,"");
                        break;
                      }
                    }
                    break;
        }
        case SEND :
                    // TO DO
                    fprintf(stderr,"(CLIENT %d) Reception d'un message\n",getpid());
                    w->ajouteMessage(m.data1,m.texte);
                    break;

        case CONSULT :
                    // TO
                    break;
      }
}

void handlerSIGALRM(int sig)
{
  if(timeOut == 0)
  {
    fprintf(stderr,"(CLIENT %d) TimeOut atteint\n",getpid());
    MESSAGE m;
    m.expediteur = getpid();
    m.requete = LOGOUT;
    m.type = 1;
    if (msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long),0) == -1)
    {
      perror("CLIENT -- Erreur de msgsnd");
      exit(1);
    }

    w->logoutOK();
    
  }
  else
  {
    timeOut--;
    w->setTimeOut(timeOut);
    alarm(1);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Fonctions Annexes ////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WindowClient::ResetAlrm()
{
  alarm(0);
  timeOut = TIME_OUT;
  w->setTimeOut(TIME_OUT);
  alarm(1);
}
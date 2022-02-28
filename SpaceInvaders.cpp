#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include "GrilleSDL.h"
#include "Ressources.h"

// Dimensions de la grille de jeu
#define NB_LIGNE 18
#define NB_COLONNE 23

// Direction de mouvement
#define GAUCHE 10
#define DROITE 11
#define BAS 12

// Intervenants du jeu (type)
#define VIDE 0
#define VAISSEAU 1
#define MISSILE 2
#define ALIEN 3
#define BOMBE 4
#define BOUCLIER1 5
#define BOUCLIER2 6
#define AMIRAL 7

typedef struct
{
    int type;
    pthread_t tid;
} S_CASE;

typedef struct
{
    int L;
    int C;
} S_POSITION;

S_CASE tab[NB_LIGNE][NB_COLONNE];

void Attente(int milli);
void setTab(int l, int c, int type = VIDE, pthread_t tid = 0);

// Fonction Thread

void *fctThVaisseau();
void *fctThEvent();
void *fctThMissile(S_POSITION *);
void *fctThTimeOut();
void *fctThInvader();
void *fctThFlotteAliens();

// Fonction SIGNAUX

void HandlerSIGUSR1(int sig);
void HandlerSIGUSR2(int sig);
void HandlerSIGHUP(int sig);

///////////////////////////////////////////////////////////////////////////////////////////////////
void Attente(int milli)
{
    struct timespec del;
    del.tv_sec = milli / 1000;
    del.tv_nsec = (milli % 1000) * 1000000;
    nanosleep(&del, NULL);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void setTab(int l, int c, int type, pthread_t tid)
{
    tab[l][c].type = type;
    tab[l][c].tid = tid;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

// Variable Threads

pthread_t thVaisseau;
pthread_t thEvent;
pthread_t thMissile;
pthread_t thTimeOut;
pthread_t thInvader;
pthread_t thFlotteAliens;

// Variable Mutex

pthread_mutex_t mutexGrille;
pthread_mutex_t mutexFlotteAliens;

// Variable Conditions

// Variable Spécifiques

// Variable Globales

int colonne;
bool fireOn=true;

int nbAliens = 24;
int lh = 2;
int cg = 8;
int lb = 8;
int cd = 18;

// Code du main

int main(int argc, char *argv[])
{

    srand((unsigned)time(NULL));

    // Ouverture de la fenetre graphique
    printf("(MAIN %d) Ouverture de la fenetre graphique\n", pthread_self());
    fflush(stdout);
    if (OuvertureFenetreGraphique() < 0)
    {
        printf("Erreur de OuvrirGrilleSDL\n");
        fflush(stdout);
        exit(1);
    }

    // Initialisation des mutex et variables de condition
    
    pthread_mutex_init(&mutexGrille, NULL);

    // Armement des signaux
    
    struct sigaction A;
    A.sa_handler = HandlerSIGUSR1;
    A.sa_flags = 0;
    sigemptyset(&A.sa_mask);
    sigaction(SIGUSR1, &A, NULL);

    struct sigaction B;
    B.sa_handler = HandlerSIGUSR2;
    B.sa_flags = 0;
    sigemptyset(&B.sa_mask);
    sigaction(SIGUSR2,&B, NULL);

    struct sigaction C;
    C.sa_handler = HandlerSIGHUP;
    C.sa_flags = 0;
    sigemptyset(&C.sa_mask);
    sigaction(SIGHUP, &C, NULL);

    // Initialisation de tab
    for (int l = 0; l < NB_LIGNE; l++)
        for (int c = 0; c < NB_COLONNE; c++)
            setTab(l, c);

    // Initialisation des boucliers
    setTab(NB_LIGNE - 2, 11, BOUCLIER1, 0);
    DessineBouclier(NB_LIGNE - 2, 11, 1);
    setTab(NB_LIGNE - 2, 12, BOUCLIER1, 0);
    DessineBouclier(NB_LIGNE - 2, 12, 1);
    setTab(NB_LIGNE - 2, 13, BOUCLIER1, 0);
    DessineBouclier(NB_LIGNE - 2, 13, 1);
    setTab(NB_LIGNE - 2, 17, BOUCLIER1, 0);
    DessineBouclier(NB_LIGNE - 2, 17, 1);
    setTab(NB_LIGNE - 2, 18, BOUCLIER1, 0);
    DessineBouclier(NB_LIGNE - 2, 18, 1);
    setTab(NB_LIGNE - 2, 19, BOUCLIER1, 0);
    DessineBouclier(NB_LIGNE - 2, 19, 1);

    // Creation des threads

    pthread_create(&thVaisseau, NULL, (void *(*)(void *))fctThVaisseau, NULL);
    pthread_create(&thEvent, NULL, (void *(*)(void *))fctThEvent, NULL);
    pthread_create(&thInvader,NULL,(void*(*)(void *))fctThInvader,NULL);


    // Exemples d'utilisation du module Ressources --> a supprimer
    /* DessineChiffre(13, 4, 7);
    DessineMissile(4, 12);
    DessineAlien(2, 12);
    DessineVaisseauAmiral(0, 15);
    DessineBombe(8, 16); */

    // Fermeture de la fenetre
    /* printf("(MAIN %d) Fermeture de la fenetre graphique...", pthread_self());
    fflush(stdout);
    FermetureFenetreGraphique();
    printf("OK\n"); */

//    pthread_join(thVaisseau,NULL);
    pthread_join(thEvent, NULL);
//    pthread_join(thMissile,NULL);

}

///////////////////////////////////////////////////////////////////////////////////////////////////

// Fonction Thread

void *fctThVaisseau()
{
    if (tab[17][colonne].type == VIDE)      // Vérif de si la case de l'init du vaisseau est vide
    {
        pthread_mutex_lock(&mutexGrille);
        tab[17][15].type = VAISSEAU;
        tab[17][15].tid = pthread_self();
        pthread_mutex_unlock(&mutexGrille);
        DessineVaisseau(17, 15);
        colonne=15;
    }

    while(1)
    {
        pause();
    }

    pthread_exit(NULL);

    return 0;
}

void *fctThEvent()
{
    EVENT_GRILLE_SDL event;

    printf("(Th Event %d) Attente du clic sur la croix\n", pthread_self());
    bool ok = false;
    while (!ok)
    {
        event = ReadEvent();
        if (event.type == CROIX)
            ok = true;
        if (event.type == GAUCHE)
        {
            if (tab[event.ligne][event.colonne].type == VIDE)
            {
                DessineVaisseau(event.ligne, event.colonne);
                setTab(event.ligne, event.colonne, VAISSEAU, pthread_self());
            }
        }
        if (event.type == CLAVIER)
        {
            if (event.touche == KEY_RIGHT)
            {
                printf("Flèche droite enfoncée\n");
                kill(getpid(), SIGUSR2);
            }
            if (event.touche == KEY_LEFT)
            {
                printf("Flèche gauche enfoncée\n");
                kill(getpid(), SIGUSR1);
            }
            if (event.touche == KEY_SPACE)
            {
                printf("Touche espace enfoncée\n");
                kill(getpid(), SIGHUP);
            }
            printf("Touche enfoncee : %c\n", event.touche);
        }
    }

    FermetureFenetreGraphique();

    pthread_exit(NULL);
}

void *fctThMissile(S_POSITION *pos)
{
    printf("Th %ld > DEBUT MISSILE\n", pthread_self());
    printf("pos L : %d\n",pos->L);
    printf("pos C : %d\n", pos->C);
    
    if (tab[pos->L][pos->C].type != BOUCLIER1 && tab[pos->L][pos->C].type != BOUCLIER2)       // Si la case d'init. du missile est vide alors on créé le missile
    {    
        while(pos->L != 0)      // Temps que le missile n'a pas atteint le haut 
        {
            EffaceCarre(pos->L, pos->C);    // Efface l'ancien missile
            setTab(0, pos->C, VIDE, 0);

            setTab(pos->L--, pos->C, MISSILE, pthread_self());      // Place le nouveau missile
            DessineMissile(pos->L, pos->C);

            Attente(80);        // Attente de 80ms
        }

        setTab(pos->L, pos->C, VIDE, 0);
        EffaceCarre(0,pos->C);
        pthread_exit(NULL);
    }
    else
    {
        if(tab[pos->L][pos->C].type == BOUCLIER1)        // Si la case d'init. du missile est un bouclier vert alors on le passe en bouclier rouge
        {
            EffaceCarre(pos->L, pos->C);
            setTab(pos->L,pos->C, VIDE, 0);
            setTab(pos->L, pos->C, BOUCLIER2, pthread_self());
            DessineBouclier(pos->L,pos->C, BOUCLIER2); 
            pthread_exit(NULL);
        }
        else
        {
            if (tab[pos->L][pos->C].type == BOUCLIER2)
            {
                EffaceCarre(pos->L, pos->C);            // Si la case d'init. du missile est un bouclier rouge alors on efface le bouclier
                setTab(pos->L, pos->C, VIDE, 0);
                pthread_exit(NULL); 
            }
        }
    }

    return 0;
}

void *fctThTimeOut()
{
    Attente(600);
    fireOn=true;
    pthread_exit(NULL);
    return 0;
}

void *fctThInvader()
{
    pthread_create(&thFlotteAliens,NULL,(void*(*)(void*))fctThFlotteAliens,NULL);
    
    pthread_join(thFlotteAliens, NULL);

    return 0;
}

void *fctThFlotteAliens()
{
    int l=2, c=8;               // Initilisation des aliens au tout début de la partie
    for(int i=0;i<4;i++)
    {
        for (int j=0;j<6;j++)
        {
            setTab(l, c, ALIEN, pthread_self());
            DessineAlien(l, c);
            c+=2;
        }
        l+=2;
        c=8;
    }

    Attente(1000);

    l=2; c=8;
    for(int i=0;i<4;i++)
    {
        for (int j=0;j<12;j++)
        {
            if(c % 2 != 0)
            {
                setTab(l, c, ALIEN, pthread_self());
                DessineAlien(l, c);
            }
            else
            {
                setTab(l,c,VIDE,0);
                EffaceCarre(l,c);
            }
            c++;
        }
        l+=2;
        c=8;
    }

    Attente(1000);

    l=2; c=9;
    for(int i=0;i<4;i++)
    {
        for(int j=0;j<12;j++)
        {
            if(c % 2 == 0)
            {
                setTab(l, c, ALIEN, pthread_self());
                DessineAlien(l, c);
            }
            else
            {
                setTab(l,c,VIDE,0);
                EffaceCarre(l,c);
            }
            c++;
        }
        l+=2;
        c=9;
    }

    /* for (int x=0;x<4;x++)
    {
        l=2; c=8;
        for(int i=0;i<4;i++)
        {
            for (int j=0;j<12;j++)
            {
                if(c % 2 != 0)
                {
                    setTab(l, c, ALIEN, pthread_self());
                    DessineAlien(l, c);
                }
                else
                {
                    setTab(l,c,VIDE,0);
                    EffaceCarre(l,c);
                }
                c++;
            }
            l+=2;
            c=8;
        }
        Attente(2000);
    } */


    return 0;
}

// Handler de Signal

void HandlerSIGUSR1(int sig)        // Mouvement pour la direction de gauche
{
    if (colonne>8)      // Vérifie les dépassements
    {
        EffaceCarre(17, colonne);
        pthread_mutex_lock(&mutexGrille);
        colonne--;
        setTab(17,colonne,VAISSEAU,pthread_self());
        DessineVaisseau(17, colonne);

        pthread_mutex_unlock(&mutexGrille);
    }
}

void HandlerSIGUSR2(int sig)        // Mouvement pour la direction de droite
{
    if (colonne<22)     // Vérifie les dépassements
    {
        EffaceCarre(17, colonne);
        pthread_mutex_lock(&mutexGrille);
        colonne++;
        setTab(17,colonne,VAISSEAU,pthread_self());
        DessineVaisseau(17, colonne);

        pthread_mutex_unlock(&mutexGrille);
    }
}

void HandlerSIGHUP(int sig)        // Mouvement pour le tir de missile
{
    S_POSITION* posMissile = (S_POSITION*) malloc(sizeof(S_POSITION));
    posMissile->C=colonne;
    posMissile->L=NB_LIGNE-2;

    if (fireOn)
    {
        fireOn=false;
        pthread_create(&thTimeOut, NULL, (void *(*)(void *))fctThTimeOut, NULL);
        pthread_create(&thMissile, NULL,(void *(*)(void *))fctThMissile, posMissile);
    }
}
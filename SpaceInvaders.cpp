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
void *fctThScore();
void *fctThBombe(S_POSITION *);

// Fonction SIGNAUX

void HandlerSIGUSR1(int sig);
void HandlerSIGUSR2(int sig);
void HandlerSIGHUP(int sig);
void HandlerSIGINT(int sig);
void HandlerSIGQUIT(int sig);

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
pthread_t thScore;
pthread_t thBombe;

// Variable Mutex

pthread_mutex_t mutexGrille;
pthread_mutex_t mutexFlotteAliens;
pthread_mutex_t mutexScore;

// Variable Conditions

pthread_cond_t condScore;

// Variable Spécifiques

// Variable Globales

int colonne;
bool fireOn=true;

int nbAliens = 24;
int lh = 2;         // Ligne d'alien la plus haute
int cg = 8;         // Colonne d'alien la plus à gauche
int lb = 8;         // Ligne d'alien la plus basse
int cd = 18;        // Colonne d'alien la plus à droite
long tidFlotteAliens;
int score=0;
bool MAJScore=false;

int delai = 1000;

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
    pthread_mutex_init(&mutexFlotteAliens, NULL);
    pthread_mutex_init(&mutexScore, NULL);

    pthread_cond_init(&condScore, NULL);

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

    struct sigaction D;
    D.sa_handler = HandlerSIGINT;
    D.sa_flags = 0;
    sigemptyset(&D.sa_mask);
    sigaction(SIGINT, &D, NULL);

    struct sigaction E;
    E.sa_handler = HandlerSIGQUIT;
    E.sa_flags = 0;
    sigemptyset(&E.sa_mask);
    sigaction(SIGQUIT, &E, NULL);

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
    pthread_create(&thScore,NULL,(void*(*)(void *))fctThScore,NULL);

    // Dessin des chiffres

    DessineChiffre(10, 2, 0);
    DessineChiffre(10, 3, 0);
    DessineChiffre(10, 4, 0);
    DessineChiffre(10, 5, 0);

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

    pthread_join(thEvent, NULL);

}

///////////////////////////////////////////////////////////////////////////////////////////////////

// Fonctions des Threads

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
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGPIPE);
    sigprocmask(SIG_SETMASK, &mask, NULL);

    EVENT_GRILLE_SDL event;

    printf("(Th Event %d) Attente du clic sur la croix\n", pthread_self());
    bool ok = false;
    while (!ok)
    {
        event = ReadEvent();
        if (event.type == CROIX)
            ok = true;
        /* if (event.type == GAUCHE)
        {
            if (tab[event.ligne][event.colonne].type == VIDE)
            {
                pthread_mutex_lock(&mutexGrille);
                DessineVaisseau(event.ligne, event.colonne);
                setTab(event.ligne, event.colonne, VAISSEAU, pthread_self());
                pthread_mutex_unlock(&mutexGrille);
            }
        } */
        if (event.type == CLAVIER)
        {
            if (event.touche == KEY_RIGHT)
            {
                kill(getpid(), SIGUSR2);
            }
            if (event.touche == KEY_LEFT)
            {
                kill(getpid(), SIGUSR1);
            }
            if (event.touche == KEY_SPACE)
            {
                kill(getpid(), SIGHUP);
            }
        }
    }

    FermetureFenetreGraphique();

    pthread_exit(NULL);
}

void *fctThMissile(S_POSITION *pos)
{
    /* printf("Th %ld > DEBUT MISSILE\n", pthread_self());
    printf("pos L : %d\n",pos->L);
    printf("pos C : %d\n", pos->C); */
    
    if (tab[pos->L][pos->C].type != BOUCLIER1 && tab[pos->L][pos->C].type != BOUCLIER2 && tab[pos->L][pos->C].type != BOMBE)       // Si la case d'init. du missile est vide alors on créé le missile
    {   
        setTab(pos->L, pos->C, MISSILE, pthread_self());      // Init le missile
        DessineMissile(pos->L, pos->C);

        while(pos->L != 0)      // Tant que le missile n'a pas atteint le haut 
        {
            if (tab[pos->L-1][pos->C].type == ALIEN)        // Si le missile rencontre un alien
            {
                pthread_mutex_lock(&mutexGrille);
                EffaceCarre(pos->L, pos->C);        // Efface le missile
                setTab(pos->L, pos->C, VIDE, 0);

                EffaceCarre(pos->L-1, pos->C);      // Efface l'alien
                setTab(pos->L-1, pos->C, VIDE, 0);

                nbAliens--;

                // Incrémentation du score + signal

                pthread_mutex_lock(&mutexScore);
                score++;
                pthread_mutex_unlock(&mutexScore);
                pthread_cond_signal(&condScore);

                printf("NBALIENS: %d\n", nbAliens);

                pthread_mutex_unlock(&mutexGrille);
                
                pthread_exit(NULL);
            }
            else if (tab[pos->L-1][pos->C].type == BOMBE)       // Si le missile rencontre une bombe
            {
                printf("THREAD MISSILE\n");
                pthread_mutex_lock(&mutexGrille);
                EffaceCarre(pos->L-1, pos->C);
                setTab(pos->L-1,pos->C, VIDE, 0);

                EffaceCarre(pos->L, pos->C);
                setTab(pos->L,pos->C, VIDE, 0);

                pthread_mutex_unlock(&mutexGrille);
                pthread_exit(NULL); 
            }

            EffaceCarre(pos->L, pos->C);        // Efface l'ancien missile
            setTab(pos->L, pos->C, VIDE, 0);

            pos->L--;
            setTab(pos->L, pos->C, MISSILE, pthread_self());      // Place le nouveau missile
            DessineMissile(pos->L, pos->C);
            
            pthread_mutex_unlock(&mutexGrille);
            Attente(80);        // Attente de 80ms
        }
        
        pthread_mutex_lock(&mutexGrille);

        setTab(pos->L, pos->C, VIDE, 0);
        EffaceCarre(0,pos->C);
        
        pthread_mutex_unlock(&mutexGrille);
        pthread_exit(NULL);
    }
    else
    {
        pthread_mutex_lock(&mutexGrille);
        if(tab[pos->L][pos->C].type == BOUCLIER1)        // Si la case d'init. du missile est un bouclier vert alors on le passe en bouclier rouge
        {
            EffaceCarre(pos->L, pos->C);
            setTab(pos->L,pos->C, VIDE, 0);
            setTab(pos->L, pos->C, BOUCLIER2, pthread_self());
            DessineBouclier(pos->L,pos->C, BOUCLIER2);

            pthread_mutex_unlock(&mutexGrille);
            pthread_exit(NULL);
        }
        else if (tab[pos->L][pos->C].type == BOUCLIER2)
        {
            EffaceCarre(pos->L, pos->C);            // Si la case d'init. du missile est un bouclier rouge alors on efface le bouclier
            setTab(pos->L, pos->C, VIDE, 0);

            pthread_mutex_unlock(&mutexGrille);
            pthread_exit(NULL); 
        }
        else if(tab[pos->L][pos->C].type == BOMBE)
        {
            EffaceCarre(pos->L, pos->C);
            //pthread_kill(tab[pos->L][pos->C].tid, SIGINT);
            setTab(pos->L,pos->C, VIDE, 0);

            pthread_mutex_unlock(&mutexGrille);
            pthread_exit(NULL); 
        }
        pthread_mutex_unlock(&mutexGrille);
    }

    pthread_exit(NULL);
    return 0;
}

void *fctThTimeOut()
{
    Attente(200);   // Vraie valeur de base : 600
    fireOn=true;
    pthread_exit(NULL);
    return 0;
}

void *fctThInvader()
{
    printf("TH Invader > Coucou\n");

    int * ret;

    while(1)
    {
        pthread_create(&thFlotteAliens,NULL,(void*(*)(void*))fctThFlotteAliens,NULL);
        pthread_join(thFlotteAliens, (void**)&ret);
        if(*ret == 0)
        {
            delai=delai*0.7;
            printf("delai : %d\n", delai);
        }
        else
        {
            printf("Alien win\n");
            printf("delai : %d\n", delai);

            for(int i=0;i<NB_LIGNE;i++)
            {
                for(int j=8;j<NB_COLONNE;j++)
                {
                    if (tab[i][j].type == ALIEN)
                    {
                        setTab(i, j, VIDE, 0);
                        EffaceCarre(i, j);
                    }
                }
            }
        }

        nbAliens = 24;

        // Ré-initialisation des boucliers à la base 

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

        Attente(1000);
    }

    return 0;
}

void *fctThFlotteAliens()
{
    tidFlotteAliens=pthread_self();
    printf("Debut flotte\n");

    // Initalisation des aliens

    for(int i=2;i<=8;i+=2)
    {
        for(int j=8;j<=18;j+=2)
        {
            pthread_mutex_lock(&mutexGrille);
            EffaceCarre(i,j);
            setTab(i, j, VIDE, 0);
            setTab(i, j, ALIEN, pthread_self());
            DessineAlien(i, j);
            pthread_mutex_unlock(&mutexGrille);
        }
    }

    Attente(delai);
    printf("Debut boucle miteuse\n");

    // ------------------ JEUNE BOUCLE MITEUSE ------------------------

    int y=0, c=8,l=0, p=0, rdm=-1, cur_aliens;

    while(y<7)      // Valeur de test   Peut-être à opti. retirer la boucle 7 et à la fin des trois boucles faire un if qui regarde si on est sur la ligne des boucliers
    {
        int x=0;
        while (x<4)     // Aller retour gauche ---> droite
        {

            // Un déplacement sur deux un alien random lache une bombe 

            if (x==1 || x==3)
            {
                rdm=(rand() % (nbAliens+1));
                printf("Nombre aléatoire : %d\n", rdm);
                cur_aliens=0;
            }

            l = 0+p;
            c=8+x;
            for (int i=0;i<6;i++)
            {
                l+=2;
                c=8+x;
                //printf("(%d,%d)\n", l, c);
                for (int j=0;j<11;j++)
                {
                    //printf("(%d,%d)\n", l, c);
                    if (tab[l][c].type == ALIEN)
                    {
                        if (cur_aliens==rdm)
                        {
                            // Lancement du threadBombe
                            printf("Pos de l'alien : %d;%d\n", l, c);
                            printf("Pos de la bombe : %d;%d\n", l+1, c);

                            S_POSITION* posBombe = (S_POSITION*) malloc(sizeof(S_POSITION));
                            posBombe->L=l+1;
                            posBombe->C=c;
                            pthread_create(&thBombe,NULL,(void*(*)(void *))fctThBombe, posBombe);
                        }
                        cur_aliens++;

                        pthread_mutex_lock(&mutexGrille);
                        if (tab[l][c+1].type == MISSILE)
                        {
                            EffaceCarre(l,c);
                            setTab(l, c, VIDE, 0);      // On tue l'alien
                            EffaceCarre(l,c+1);
                            long tid;                   // On stock le tid pour pouvoir kill le thread avant de l'effacer
                            tid=tab[l][c+1].tid;
                            setTab(l, c+1, VIDE, 0);      // On tue le missile (tab & affichage)

                            // Mutex à faire !!!
                            nbAliens--;
                            printf("NBALIENS: %d\n", nbAliens);

                            // Incrémentation du score + signal

                            pthread_mutex_lock(&mutexScore);
                            score++;
                            pthread_mutex_unlock(&mutexScore);
                            pthread_cond_signal(&condScore);

                            pthread_mutex_unlock(&mutexGrille);
                            
                            pthread_kill(tid, SIGINT);     // On tue le missile en envoyant SIGINT au threadMissile
                        }
                        else
                        {
                            EffaceCarre(l,c);
                            setTab(l, c, VIDE, 0);

                            DessineAlien(l,c+1);
                            setTab(l,c+1,ALIEN, pthread_self());

                            pthread_mutex_unlock(&mutexGrille);
                        }
                    }
                    if (nbAliens==0)
                    {
                        printf("Tous les aliens sont morts !\n");
                        pthread_mutex_unlock(&mutexGrille);
                        pthread_exit(&nbAliens);
                    }  
                    c+=2;
                }
                //printf("Ligne entiere\n");
            }
            //printf("Cycle Complet ->\n");
            Attente(delai);
            x++;
        }

        printf("Gauche->droite fini\n");

        x=0;
        while (x<4)     // Aller retour droite ---> gauche
        {
            l=0+p;
            c=22-x;

            for (int i=0;i<6;i++)
            {
                l+=2;
                c=22-x;
                //printf("(%d,%d)\n", l, c);
                for (int j=0;j<11;j++)
                {
                    //printf("(%d,%d)\n", l, c);
                    if (tab[l][c].type == ALIEN)
                    {
                        pthread_mutex_lock(&mutexGrille);
                        if (tab[l][c-1].type == MISSILE)
                        {
                            EffaceCarre(l,c);
                            setTab(l, c, VIDE, 0);      // On tue l'alien

                            EffaceCarre(l,c-1);
                            long tid;                   // On stock le tid pour pouvoir kill le thread avant de l'effacer
                            tid=tab[l][c-1].tid;
                            setTab(l, c-1, VIDE, 0);      // On tue le missile (tab & affichage)

                            nbAliens--;
                            printf("NBALIENS: %d\n", nbAliens);

                            // Incrémentation du score + signal

                            pthread_mutex_lock(&mutexScore);
                            score++;
                            pthread_mutex_unlock(&mutexScore);
                            pthread_cond_signal(&condScore);

                            pthread_mutex_unlock(&mutexGrille);

                            pthread_kill(tid, SIGINT);     // On tue le missile en envoyant SIGINT au threadMissile
                        }
                        else
                        {
                            EffaceCarre(l,c);
                            setTab(l, c, VIDE, 0);

                            DessineAlien(l,c-1);
                            setTab(l,c-1,ALIEN, pthread_self());

                            pthread_mutex_unlock(&mutexGrille);
                        }
                        
                    }
                    if (nbAliens == 0)
                    {
                        printf("Tous les aliens sont morts !\n");
                        pthread_mutex_unlock(&mutexGrille);
                        pthread_exit(&nbAliens);
                    }
                    c-=2;
                }
                //printf("Ligne entiere\n");
            }
            //printf("Cycle Complet <-\n");
            Attente(delai);
            x++;
        }

        printf("droite->gauche fini\n");

        l=0+p;
        c=8;
        for (int i=0;i<6;i++)       // Descente aux enfers
        {
            l+=2;
            c=8;
            //printf("(%d,%d)\n", l, c);
            for (int j=0;j<11;j++)
            {
               
                //printf("(%d,%d)\n", l, c);
                if (tab[l][c].type == ALIEN)
                {
                    pthread_mutex_lock(&mutexGrille);
                    if (tab[l+1][c].type == MISSILE)
                    {
                        EffaceCarre(l,c);
                        setTab(l, c, VIDE, 0); // On tue l'alien

                        EffaceCarre(l+1,c);
                        long tid;                       // On stock le tid pour pouvoir kill le thread avant de l'effacer
                        tid=tab[l+1][c].tid;
                        setTab(l+1, c, VIDE, 0);        // On tue le missile (tab & affichage)

                        pthread_mutex_unlock(&mutexGrille);

                        nbAliens--;
                        printf("NBALIENS: %d\n", nbAliens);

                        // Incrémentation du score + signal

                        pthread_mutex_lock(&mutexScore);
                        score++;
                        pthread_mutex_unlock(&mutexScore);
                        pthread_cond_signal(&condScore);

                        pthread_kill(tid, SIGINT); // On tue le missile en envoyant SIGINT au threadMissile
                    }
                    else
                    {
                        EffaceCarre(l,c);
                        setTab(l, c, VIDE, 0);

                        DessineAlien(l+1,c);
                        setTab(l+1,c,ALIEN, pthread_self());

                        pthread_mutex_unlock(&mutexGrille);
                    }
                    
                }
                if (nbAliens == 0)
                {
                    printf("Tous les aliens sont morts !\n");
                    pthread_mutex_unlock(&mutexGrille);
                    pthread_exit(&nbAliens);
                }
                c+=2;
            }
        }
        
        Attente(delai);
        printf("Descente\n");
        y++;
        p++;
    }

        for (int i=8;i<22;i++)
        {
            if (tab[17][i].type == VAISSEAU)
            {
                EffaceCarre(17,i);
                setTab(17, i, VIDE, 0);
                pthread_kill(tab[17][i].tid, SIGQUIT);
                pthread_exit(&nbAliens);
            }
        }

    return 0;
}

void *fctThScore()
{
    while (1)
    {
        // Paradigme d'attente

        pthread_mutex_lock(&mutexScore);
        while (!MAJScore)
        {
            pthread_cond_wait(&condScore, &mutexScore);
            printf("Le score est de : %d\n", score);

            // Calcul avec modulo pour afficher un par un

            int cur_score=score;
            int i=5;
            while (cur_score!=0 || i>1)
            {
                int mod_score=cur_score%10;
                cur_score=(cur_score-mod_score)/10;
                DessineChiffre(10, i, mod_score);
                i--;
            }
            MAJScore=false;
        }
        pthread_mutex_unlock(&mutexScore);
    }

    pthread_exit(&nbAliens);

    return 0;
}

void *fctThBombe(S_POSITION * pos)
{
    printf("BOOOOOOOOOOM\n");

    printf("pos -> L : %d\n",pos->L);
    printf("NB-1 : %d\n",NB_LIGNE-1);

    do
    {
        switch(tab[pos->L][pos->C].type)      // Prochaine position de la bombe si i=1, sinon si i=0 initialisation de la bombe
        {
            case BOUCLIER2:
                pthread_mutex_lock(&mutexGrille);
                printf("CASE BOUCLIER2\n");
                EffaceCarre(pos->L, pos->C);            // Si la prochaine case est un bouclier rouge alors on efface le bouclier
                setTab(pos->L, pos->C, VIDE, 0);

                pthread_mutex_unlock(&mutexGrille);
                pthread_exit(NULL); 
                break;
            case BOUCLIER1:
                pthread_mutex_lock(&mutexGrille);
                printf("CASE BOUCLIER1\n");
                EffaceCarre(pos->L, pos->C);            // Si la prochaine case est un bouclier vert alors on passe le bouclier en rouge (bouclier2)
                setTab(pos->L,pos->C, VIDE, 0);
                setTab(pos->L, pos->C, BOUCLIER2, pthread_self());
                DessineBouclier(pos->L,pos->C, BOUCLIER2);

                pthread_mutex_unlock(&mutexGrille);
                pthread_exit(NULL);
                break;
            case BOMBE:
                pthread_mutex_lock(&mutexGrille);
                printf("CASE BOMBE\n");
                EffaceCarre(pos->L, pos->C);
                setTab(pos->L, pos->C, VIDE, 0);

                pthread_mutex_unlock(&mutexGrille);
                pthread_exit(NULL);
                break;
            case MISSILE:
                pthread_mutex_lock(&mutexGrille);
                printf("CASE MISSILE\n");
                EffaceCarre(pos->L, pos->C);      // Efface le missile

                pthread_kill(tab[pos->L][pos->C].tid, SIGINT);        // Envoie du SIGINT au thMissile
                setTab(pos->L, pos->C, VIDE, 0);
                pthread_mutex_unlock(&mutexGrille);
                pthread_exit(NULL);
                break;
            case VAISSEAU:
                pthread_mutex_lock(&mutexGrille);
                printf("CASE VAISSEAU\n");
                EffaceCarre(pos->L, pos->C);
                
                pthread_kill(tab[pos->L][pos->C].tid, SIGQUIT);

                setTab(pos->L, pos->C, VIDE, 0);
                pthread_mutex_unlock(&mutexGrille);

                pthread_exit(NULL);
                break;
            case ALIEN:
                // ???
                break;
        }

        setTab(pos->L,pos->C,BOMBE, pthread_self());
        DessineBombe(pos->L,pos->C);

        pos->L++;

        Attente(160);
        setTab(pos->L-1,pos->C,VIDE,0);
        EffaceCarre(pos->L-1,pos->C);
        
        //pthread_mutex_unlock(&mutexGrille);
    }while(pos->L < NB_LIGNE-1);
    
    pthread_exit(NULL);

    return 0;
}

// Handler de Signal

void HandlerSIGUSR1(int sig)        // Mouvement pour la direction de gauche
{
    if (colonne>8)      // Vérifie les dépassements
    {
        pthread_mutex_lock(&mutexGrille);
        EffaceCarre(17, colonne);
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
        pthread_mutex_lock(&mutexGrille);
        EffaceCarre(17, colonne);
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

void HandlerSIGINT(int sig)
{
    printf("SIGINT\n");
    pthread_exit(NULL);
}

void HandlerSIGQUIT(int sig)
{
    printf("SIGQUIT\n");
    
    pthread_mutex_lock(&mutexGrille);
    for (int i=8;i<22;i++)
    {
        if (tab[17][i].type == VAISSEAU)
        {
            EffaceCarre(17,i);
            setTab(17, i, VIDE, 0);
            pthread_mutex_unlock(&mutexGrille);
            pthread_exit(NULL);
        }
    }
}
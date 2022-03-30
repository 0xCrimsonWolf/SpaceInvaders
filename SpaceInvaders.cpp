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

// Define ajoutés
#define PAIR 0
#define IMPAIR 1

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

// Fonctions Threads

void *fctThVaisseau();
void *fctThEvent();
void *fctThMissile(S_POSITION *);
void *fctThTimeOut();
void *fctThInvader();
void *fctThFlotteAliens();
void *fctThScore();
void *fctThBombe(S_POSITION *);
void *fctThAmiral();
void *fctThBouclier(int *);

// Fonctions SIGNAUX

void HandlerSIGUSR1(int sig);
void HandlerSIGUSR2(int sig);
void HandlerSIGHUP(int sig);
void HandlerSIGINT(int sig);
void HandlerSIGQUIT(int sig);
void HandlerSIGALRM(int sig);
void HandlerSIGCHLD(int sig);
void HandlerSIGPIPE(int sig);

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
pthread_t thAmiral;
pthread_t thBouclier[6];

// Variable Mutex

pthread_mutex_t mutexGrille;
pthread_mutex_t mutexScore;
pthread_mutex_t mutexAliens; // (mutexFlotteAlien)
pthread_mutex_t mutexVies;
pthread_mutex_t mutexBouclier;

// Variable Conditions

pthread_cond_t condScore;
pthread_cond_t condVies;
pthread_cond_t condFlotteAliens;

// Variable Spécifiques

pthread_key_t cle_etat;
pthread_key_t cle_pos;

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
bool MAJScore=false, OK=true;
int nbVies=3;
int niveau=0;

int delai = 1000;

// Fonctions ajoutées

int RandomNumberPairImpair(bool PP, int compteur, int* current_alien);
void DecrementNbVies(void *p);
void Destructeur(void *p);

// Code du main

int main(int argc, char *argv[])
{
    sigset_t mask;
    sigfillset(&mask);
    sigprocmask(SIG_SETMASK, &mask, NULL);

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
    pthread_mutex_init(&mutexScore, NULL);
    pthread_mutex_init(&mutexAliens, NULL);
    pthread_mutex_init(&mutexVies, NULL);
    pthread_mutex_init(&mutexBouclier, NULL);

    pthread_cond_init(&condScore, NULL);
    pthread_cond_init(&condVies, NULL);
    pthread_cond_init(&condFlotteAliens, NULL);

    pthread_key_create(&cle_etat, Destructeur);
    pthread_key_create(&cle_pos, Destructeur);

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

    struct sigaction F;
    F.sa_handler = HandlerSIGALRM;
    F.sa_flags = 0;
    sigemptyset(&F.sa_mask);
    sigaction(SIGALRM, &F, NULL);

    struct sigaction G;
    G.sa_handler = HandlerSIGCHLD;
    G.sa_flags = 0;
    sigemptyset(&G.sa_mask);
    sigaction(SIGCHLD, &G, NULL);

    struct sigaction H;
    H.sa_handler = HandlerSIGPIPE;
    H.sa_flags = 0;
    sigemptyset(&H.sa_mask);
    sigaction(SIGPIPE, &H, NULL);

    // Initialisation de tab

    for (int l = 0; l < NB_LIGNE; l++)
        for (int c = 0; c < NB_COLONNE; c++)
            setTab(l, c);

    // Initialisation des boucliers

    int p = 0;
    for (int i = 0; i < 9; i++)
    {
        pthread_mutex_lock(&mutexBouclier);
        
        if (i == 3)
            i += 3;
        pthread_create(&thBouclier[p], NULL, (void *(*)(void *))fctThBouclier, &i);

        p++;
    }

    // Creation des threads

    pthread_create(&thVaisseau, NULL, (void *(*)(void *))fctThVaisseau, NULL);
    pthread_create(&thEvent, NULL, (void *(*)(void *))fctThEvent, NULL);
    pthread_create(&thInvader,NULL,(void*(*)(void *))fctThInvader,NULL);
    pthread_create(&thScore,NULL,(void*(*)(void *))fctThScore,NULL);
    pthread_create(&thAmiral,NULL,(void*(*)(void *))fctThAmiral,NULL);

    // Initialisation des sprites

    DessineChiffre(10, 2, 0);
    DessineChiffre(10, 3, 0);
    DessineChiffre(10, 4, 0);
    DessineChiffre(10, 5, 0);

    DessineChiffre(13, 3, 0);
    DessineChiffre(13, 4, 0);

    DessineVaisseau(16, 4);
    DessineVaisseau(16, 3);

    // Gestion des vies

    pthread_mutex_lock(&mutexVies);
    while(nbVies>0)
    {
        pthread_cond_wait(&condVies, &mutexVies);
        if (nbVies>0)
        {
            if (nbVies==2)
            {
                EffaceCarre(16,4);
            }
            if (nbVies==1)
            {
                EffaceCarre(16,3);
            }
            pthread_create(&thVaisseau, NULL, (void *(*)(void *))fctThVaisseau, NULL);
        }
        
    }
    pthread_mutex_unlock(&mutexVies);

    DessineGameOver(6,11);
    pthread_cancel(thFlotteAliens);
    pthread_cancel(thInvader);
    pthread_cancel(thAmiral);

    pthread_join(thEvent, NULL);

    exit(0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

// Fonctions des Threads

void *fctThVaisseau()
{
    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGQUIT);
    sigdelset(&mask, SIGUSR1);
    sigdelset(&mask, SIGUSR2);
    sigdelset(&mask, SIGHUP);
    sigprocmask(SIG_SETMASK, &mask, NULL);

    pthread_cleanup_push((void(*)(void*))DecrementNbVies, 0);


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
    
    pthread_cleanup_pop(1);

    pthread_exit(NULL);

    return 0;
}

void *fctThEvent()
{
    EVENT_GRILLE_SDL event;

    bool ok = false;
    while (!ok)
    {
        event = ReadEvent();
        if (event.type == CROIX)
            ok = true;
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

    // On remets le nombre de vie a 0 pour quitter la condition du main sur nbVies
    pthread_mutex_lock(&mutexVies);
    nbVies = 0;
    pthread_mutex_unlock(&mutexVies);
    pthread_cond_signal(&condVies);

    FermetureFenetreGraphique();

    pthread_exit(NULL);
}

void *fctThMissile(S_POSITION *pos)
{
    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGINT);
    sigprocmask(SIG_SETMASK, &mask, NULL);

    pthread_mutex_lock(&mutexGrille);
    if (tab[pos->L][pos->C].type != BOUCLIER1 && tab[pos->L][pos->C].type != BOUCLIER2 && tab[pos->L][pos->C].type != BOMBE)       // Si la case d'init. du missile est vide alors on créé le missile
    {
        setTab(pos->L, pos->C, MISSILE, pthread_self());      // Init le missile
        DessineMissile(pos->L, pos->C);
        pthread_mutex_unlock(&mutexGrille);

        while(pos->L != 0)      // Tant que le missile n'a pas atteint le haut 
        {
            pthread_mutex_lock(&mutexGrille);
            if (tab[pos->L-1][pos->C].type == ALIEN)        // Si le missile rencontre un alien
            {
                EffaceCarre(pos->L, pos->C);        // Efface le missile
                setTab(pos->L, pos->C, VIDE, 0);

                EffaceCarre(pos->L-1, pos->C);      // Efface l'alien
                setTab(pos->L-1, pos->C, VIDE, 0);
                pthread_mutex_unlock(&mutexGrille);

                pthread_mutex_lock(&mutexAliens);
                nbAliens--;
                if ((nbAliens%6)==0 && nbAliens!=0)
                    pthread_cond_signal(&condFlotteAliens);
                pthread_mutex_unlock(&mutexAliens);

                // Incrémentation du score + signal

                pthread_mutex_lock(&mutexScore);
                score++;
                pthread_mutex_unlock(&mutexScore);
                pthread_cond_signal(&condScore);

                printf("NBALIENS: %d\n", nbAliens);
                
                pthread_exit(NULL);
            }
            else if (tab[pos->L-1][pos->C].type == BOMBE)       // Si le missile rencontre une bombe
            {
                printf("Missile rencontre une bombe !\n");
                EffaceCarre(pos->L-1, pos->C);      // Efface la bombe
                pthread_kill(tab[pos->L-1][pos->C].tid, SIGINT);
                setTab(pos->L-1,pos->C, VIDE, 0);

                EffaceCarre(pos->L, pos->C);        // Efface le missile
                setTab(pos->L,pos->C, VIDE, 0);

                pthread_mutex_unlock(&mutexGrille);
                pthread_exit(NULL); 
            }
            else if (tab[pos->L-1][pos->C].type == AMIRAL)
            {
                printf("BOOOOOM DANS TA GUEULE\n");

                // Incrémentation du score de 10

                pthread_mutex_lock(&mutexScore);
                score+=10;
                pthread_mutex_unlock(&mutexScore);
                pthread_cond_signal(&condScore);

                // Envoi du signal SIGCHLD au VaisseauAmiral

                printf("Envoi du SIGCHLD à %ld\n", tab[pos->L-1][pos->C].tid);
                pthread_kill(tab[pos->L-1][pos->C].tid, SIGCHLD);

                EffaceCarre(pos->L, pos->C);        // Efface le missile
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
        printf("ELSE\n");
        if(tab[pos->L][pos->C].type == BOUCLIER1)        // Si la case d'init. du missile est un bouclier vert alors on le passe en bouclier rouge
        {
            printf("BOUCLIER 1\n");
            printf("tid à kill : %ld\n", tab[pos->L][pos->C].tid);
            pthread_kill(tab[pos->L][pos->C].tid, SIGPIPE);

            pthread_mutex_unlock(&mutexGrille);
            pthread_exit(NULL);
        }
        else if (tab[pos->L][pos->C].type == BOUCLIER2)
        {
            printf("BOUCLIER 1\n");
            // Si la case d'init. du missile est un bouclier rouge alors on efface le bouclier

            pthread_kill(tab[pos->L][pos->C].tid, SIGPIPE);

            pthread_mutex_unlock(&mutexGrille);
            pthread_exit(NULL); 
        }
        else if(tab[pos->L][pos->C].type == BOMBE)
        {
            EffaceCarre(pos->L, pos->C);
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
    sigset_t mask;
    sigfillset(&mask);
    sigprocmask(SIG_SETMASK, &mask, NULL);

    Attente(300);   // Vraie valeur de base : 600
    fireOn=true;
    pthread_exit(NULL);
    return 0;
}

void *fctThInvader()
{
    sigset_t mask;
    sigfillset(&mask);
    sigprocmask(SIG_SETMASK, &mask, NULL);

    int * ret;
    while(1)
    {
        pthread_create(&thFlotteAliens,NULL,(void*(*)(void*))fctThFlotteAliens,NULL);
        pthread_join(thFlotteAliens, (void**)&ret);

        if(*ret == 0)       // Si les aliens sont tous morts (aliens LOOSE)
        {
            delai=delai*0.7;                    // Augmente la vitesse (retrait de 30% sur le délai)
            printf("delai : %d\n", delai);

            // Calcul avec modulo pour afficher chiffre par chiffre le niveau

            niveau++;
            int cur_niveau=niveau;
            int i=4;
            printf("CurNiv = %d\n", cur_niveau);
            while(cur_niveau!=0 || i>2)
            {
                int mod_niveau=cur_niveau%10;
                cur_niveau=(cur_niveau-mod_niveau)/10;
                DessineChiffre(13, i, mod_niveau);
                printf("Dessin en 10,4 de %d\n", mod_niveau);
                i--;
            }
        }
        else        // Si les aliens ont touché les boucliers (aliens WIN)
        {
            printf("Alien win\n");
            printf("delai : %d\n", delai);

            // On supprime les aliens restants

            pthread_mutex_lock(&mutexGrille);
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
            pthread_mutex_unlock(&mutexGrille);
        }

        pthread_mutex_lock(&mutexAliens);
        nbAliens = 24;
        pthread_mutex_unlock(&mutexAliens);

        // Ré-initialisation des boucliers à la base 
        
        int p = 0;
        for (int i = 0; i < 9; i++)
        {
            pthread_mutex_lock(&mutexBouclier);
            
            if (i == 3)
                i += 3;
            pthread_create(&thBouclier[p], NULL, (void *(*)(void *))fctThBouclier, &i);

            p++;
        }

        Attente(1000);
    }

    return 0;
}

void *fctThFlotteAliens()
{
    sigset_t mask;
    sigfillset(&mask);
    sigprocmask(SIG_SETMASK, &mask, NULL);

    tidFlotteAliens=pthread_self();

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
    //printf("Debut boucle miteuse\n");

    // ------------------ JEUNE BOUCLE MITEUSE ------------------------

    int y=0, c=8,l=0, p=0, rdm=-1, cur_aliens, compt=1;

    while(y<7)      // Valeur de test   Peut-être à opti. retirer la boucle 7 et à la fin des trois boucles faire un if qui regarde si on est sur la ligne des boucliers
    {
        int x=0;
        while (x<4)     // Aller retour gauche ---> droite
        {
            // Un déplacement sur deux un alien random lache une bombe 

            rdm=RandomNumberPairImpair(IMPAIR, compt, &cur_aliens);

            l = 0+p;
            c=8+x;
            for (int i=0;i<6;i++)
            {
                l+=2;
                c=8+x;
                for (int j=0;j<11;j++)
                {
                    if (tab[l][c].type == ALIEN)
                    {
                        if (cur_aliens==rdm)
                        {
                            // Lancement du threadBombe

                            S_POSITION* posBombe = (S_POSITION*) malloc(sizeof(S_POSITION));
                            posBombe->L=l+1;
                            posBombe->C=c+1;
                            pthread_create(&thBombe,NULL,(void*(*)(void *))fctThBombe, posBombe);
                        }
                        cur_aliens++;

                        pthread_mutex_lock(&mutexGrille);                
                        
                        if (tab[l][c+1].type == MISSILE)
                        {
                            printf("GD -> Missile\n");
                            
                            EffaceCarre(l,c);
                            setTab(l, c, VIDE, 0);      // On tue l'alien
                            
                            EffaceCarre(l,c+1);
                            pthread_kill(tab[l][c+1].tid, SIGINT);     // On tue le missile en envoyant SIGINT au threadMissile
                            setTab(l, c+1, VIDE, 0);      // On tue le missile (tab & affichage)

                            pthread_mutex_lock(&mutexAliens);
                            nbAliens--;
                            if ((nbAliens%6)==0 && nbAliens!=0)
                                pthread_cond_signal(&condFlotteAliens);
                            pthread_mutex_unlock(&mutexAliens);
                            
                            printf("NBALIENS: %d\n", nbAliens);

                            // Incrémentation du score + signal

                            pthread_mutex_lock(&mutexScore);
                            score++;
                            pthread_mutex_unlock(&mutexScore);
                            pthread_cond_signal(&condScore);
                        }
                        else
                        {
                            //printf("GD -> Else\n");
                            
                            EffaceCarre(l,c);
                            setTab(l, c, VIDE, 0);

                            //Attente(500);

                            if (tab[l][c+1].type == BOMBE)
                            {
                                //printf("GD -> Bombe\n");
                                EffaceCarre(l,c+1);
                                pthread_kill(tab[l][c+1].tid, SIGINT);
                                setTab(l,c+1, VIDE, 0);
                                
                                S_POSITION* posBombe = (S_POSITION*) malloc(sizeof(S_POSITION));
                                posBombe->L=l+1;
                                posBombe->C=c+1;
                                pthread_create(&thBombe,NULL,(void*(*)(void *))fctThBombe, posBombe); 
                            }

                            DessineAlien(l,c+1);
                            setTab(l,c+1,ALIEN, pthread_self());
                    
                        }
                        pthread_mutex_unlock(&mutexGrille);
                    }
                    if (nbAliens==0)
                    {
                        printf("Tous les aliens sont morts !\n");
                        pthread_exit(&nbAliens);
                    }  
                    c+=2;
                }
                //printf("Ligne entiere\n");
            }
            //printf("Cycle Complet ->\n");
            Attente(delai);
            x++;
            compt++;
        }

        x=0;
        while (x<4)     // Aller retour droite ---> gauche
        {
            // Un déplacement sur deux un alien random lache une bombe 

            rdm=RandomNumberPairImpair(IMPAIR, compt, &cur_aliens);

            l=0+p;
            c=22-x;

            for (int i=0;i<6;i++)
            {
                l+=2;
                c=22-x;
                for (int j=0;j<11;j++)
                {
                    if (tab[l][c].type == ALIEN)
                    {
                        if (cur_aliens==rdm)
                        {
                            // Lancement du threadBombe
                            /* printf("Pos de l'alien : %d;%d\n", l, c);
                            printf("Pos de la bombe : %d;%d\n", l+1, c); */

                            S_POSITION* posBombe = (S_POSITION*) malloc(sizeof(S_POSITION));
                            posBombe->L=l+1;
                            posBombe->C=c-1;
                            if(posBombe->C == 7)
                                posBombe->C++;
                            pthread_create(&thBombe,NULL,(void*(*)(void *))fctThBombe, posBombe);
                        }
                        cur_aliens++;

                        pthread_mutex_lock(&mutexGrille);

                        if (tab[l][c-1].type == MISSILE)
                        {
                            printf("DG -> Missile\n");
                            
                            EffaceCarre(l,c);
                            setTab(l, c, VIDE, 0);      // On tue l'alien

                            EffaceCarre(l,c-1);
                            pthread_kill(tab[l][c-1].tid, SIGINT);     // On tue le missile en envoyant SIGINT au threadMissile
                            setTab(l, c-1, VIDE, 0);      // On tue le missile (tab & affichage)

                            pthread_mutex_lock(&mutexAliens);
                            nbAliens--;
                            if ((nbAliens%6)==0 && nbAliens!=0)
                                pthread_cond_signal(&condFlotteAliens);
                            pthread_mutex_unlock(&mutexAliens);
                            printf("NBALIENS: %d\n", nbAliens);

                            // Incrémentation du score + signal

                            pthread_mutex_lock(&mutexScore);
                            score++;
                            pthread_mutex_unlock(&mutexScore);
                            pthread_cond_signal(&condScore);
                        }
                        else
                        {
                            //printf("DG -> ELSE\n");
                            
                            EffaceCarre(l,c);
                            setTab(l, c, VIDE, 0);

                            //Attente(500);

                            if (tab[l][c-1].type == BOMBE)
                            {
                                //printf("DG -> Bombe\n");
                                EffaceCarre(l,c-1);
                                pthread_kill(tab[l][c-1].tid, SIGINT);
                                setTab(l,c-1, VIDE, 0);
                                
                                S_POSITION* posBombe = (S_POSITION*) malloc(sizeof(S_POSITION));
                                posBombe->L=l+1;
                                posBombe->C=c-1;

                                pthread_create(&thBombe,NULL,(void*(*)(void *))fctThBombe, posBombe); 
                            }

                            DessineAlien(l,c-1);
                            setTab(l,c-1,ALIEN, pthread_self());
                            
                        }
                        pthread_mutex_unlock(&mutexGrille);
                    }
                    if (nbAliens == 0)
                    {
                        printf("Tous les aliens sont morts !\n");
                        pthread_exit(&nbAliens);
                    }
                    c-=2;
                }
                //printf("Ligne entiere\n");
            }
            //printf("Cycle Complet <-\n");
            Attente(delai);
            x++;
            compt++;
        }

        //printf("droite->gauche fini\n");

        l=0+p;
        c=8;

        rdm=RandomNumberPairImpair(IMPAIR, compt, &cur_aliens);

        for (int i=0;i<6;i++)       // Descente aux enfers
        {
            l+=2;
            c=8;
            for (int j=0;j<11;j++)
            {
                if (tab[l][c].type == ALIEN)
                {         
                    if (cur_aliens==rdm)
                    {
                        // Lancement du threadBombe

                        S_POSITION* posBombe = (S_POSITION*) malloc(sizeof(S_POSITION));
                        posBombe->L=l+2;
                        posBombe->C=c;
                        pthread_create(&thBombe,NULL,(void*(*)(void *))fctThBombe, posBombe);
                    }
                    cur_aliens++;

                    pthread_mutex_lock(&mutexGrille);

                    if (tab[l+1][c].type == MISSILE)
                    {
                        printf("Desc -> MISSILE\n");
                        
                        EffaceCarre(l,c);
                        setTab(l, c, VIDE, 0); // On tue l'alien

                        EffaceCarre(l+1,c);
                        pthread_kill(tab[l+1][c].tid, SIGINT); // On tue le missile en envoyant SIGINT au threadMissile
                        setTab(l+1, c, VIDE, 0);        // On tue le missile (tab & affichage)

                        pthread_mutex_unlock(&mutexGrille);

                        pthread_mutex_lock(&mutexAliens);
                        nbAliens--;
                        if ((nbAliens%6)==0 && nbAliens!=0)
                            pthread_cond_signal(&condFlotteAliens);
                        pthread_mutex_unlock(&mutexAliens);
                        printf("NBALIENS: %d\n", nbAliens);

                        // Incrémentation du score + signal

                        pthread_mutex_lock(&mutexScore);
                        score++;
                        pthread_mutex_unlock(&mutexScore);
                        pthread_cond_signal(&condScore);
                    }
                    else
                    {
                        //printf("Desc -> ELSE\n");
        
                        EffaceCarre(l,c);
                        setTab(l, c, VIDE, 0);

                        if (tab[l+1][c].type == BOMBE)
                        {
                            //printf("Desc -> Bombe\n");
                            EffaceCarre(l+1,c);
                            pthread_kill(tab[l+1][c].tid, SIGINT);
                            setTab(l+1,c, VIDE, 0);
                                
                            S_POSITION* posBombe = (S_POSITION*) malloc(sizeof(S_POSITION));
                            posBombe->L=l+2;
                            posBombe->C=c;

                            pthread_create(&thBombe,NULL,(void*(*)(void *))fctThBombe, posBombe); 
                        }

                        DessineAlien(l+1,c);
                        setTab(l+1,c,ALIEN, pthread_self());

                    }
                    pthread_mutex_unlock(&mutexGrille);
                }
                if (nbAliens == 0)
                {
                    printf("Tous les aliens sont morts !\n");
                    pthread_exit(&nbAliens);
                }
                c+=2;
            }
        }
        compt++;
        
        Attente(delai);
        printf("Descente\n");
        y++;
        p++;
    }

    printf("Finito les aliens ont win poto\n");

        for (int i=8;i<22;i++)
        {
            if (tab[17][i].type == VAISSEAU)
            {
                pthread_kill(tab[17][i].tid, SIGQUIT);
                pthread_exit(&nbAliens);
            }
        }

    return 0;
}

void *fctThScore()
{
    sigset_t mask;
    sigfillset(&mask);
    sigprocmask(SIG_SETMASK, &mask, NULL);

    while (1)
    {
        // Paradigme d'attente

        pthread_mutex_lock(&mutexScore);
        while (!MAJScore)
        {
            pthread_cond_wait(&condScore, &mutexScore);
            printf("Le score est de : %d\n", score);

            // Calcul avec modulo pour afficher chiffre par chiffre

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
    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGINT);
    sigprocmask(SIG_SETMASK, &mask, NULL);

    pthread_mutex_lock(&mutexGrille);
    if (tab[pos->L][pos->C].type != ALIEN && tab[pos->L][pos->C].type != BOUCLIER1 && tab[pos->L][pos->C].type != BOUCLIER2 && tab[pos->L][pos->C].type != BOMBE && tab[pos->L][pos->C].type != MISSILE && tab[pos->L][pos->C].type != VAISSEAU)
    {
        setTab(pos->L,pos->C,BOMBE, pthread_self());
        DessineBombe(pos->L,pos->C);
        pthread_mutex_unlock(&mutexGrille);

        while (pos->L < NB_LIGNE-1)
        {
            pthread_mutex_lock(&mutexGrille);
            switch(tab[pos->L+1][pos->C].type)      // Prochaine position de la bombe
            {
                case BOUCLIER2:
                    
                    printf("CASE BOUCLIER2\n");
                    
                    pthread_kill(tab[pos->L+1][pos->C].tid, SIGPIPE);

                    EffaceCarre(pos->L, pos->C);
                    setTab(pos->L,pos->C, VIDE, 0);     // Efface la bombe

                    pthread_mutex_unlock(&mutexGrille);
                    pthread_exit(NULL); 
                    break;
                case BOUCLIER1:
                    
                    printf("CASE BOUCLIER1\n");
                    
                    pthread_kill(tab[pos->L+1][pos->C].tid, SIGPIPE);

                    EffaceCarre(pos->L, pos->C);
                    setTab(pos->L,pos->C, VIDE, 0);     // Efface la bombe

                    pthread_mutex_unlock(&mutexGrille);
                    pthread_exit(NULL);
                    break;
                case BOMBE:
                    
                    printf("CASE BOMBE\n");
                    EffaceCarre(pos->L+1, pos->C);
                    setTab(pos->L+1, pos->C, VIDE, 0);

                    pthread_mutex_unlock(&mutexGrille);
                    pthread_exit(NULL);
                    break;
                case MISSILE:
                    
                    printf("CASE MISSILE\n");
                    EffaceCarre(pos->L+1, pos->C);      // Efface le missile

                    EffaceCarre(pos->L, pos->C);
                    setTab(pos->L,pos->C, VIDE, 0);     // Efface la bombe

                    pthread_kill(tab[pos->L+1][pos->C].tid, SIGINT);        // Envoie du SIGINT au thMissile
                    setTab(pos->L+1, pos->C, VIDE, 0);

                    pthread_mutex_unlock(&mutexGrille);
                    pthread_exit(NULL);
                    break;
                case VAISSEAU:
                    
                    printf("CASE VAISSEAU\n");
                    //EffaceCarre(pos->L+1, pos->C);
                    
                    pthread_kill(tab[pos->L+1][pos->C].tid, SIGQUIT);

                    //setTab(pos->L+1, pos->C, VIDE, 0);

                    EffaceCarre(pos->L, pos->C);
                    setTab(pos->L,pos->C, VIDE, 0);     // Efface la bombe

                    pthread_mutex_unlock(&mutexGrille);
                    pthread_exit(NULL);
                    break;
                case VIDE:
                    
                    EffaceCarre(pos->L,pos->C);
                    setTab(pos->L,pos->C,VIDE, 0);      // Efface l'ancienne bombe 
            
                    pos->L++;       // Passe à la ligne en bas

                    setTab(pos->L,pos->C,BOMBE,pthread_self());     // Place la nouvelle bombe
                    DessineBombe(pos->L,pos->C);
                    
                    pthread_mutex_unlock(&mutexGrille);
                    break;
                case ALIEN:
                    break;
            }
            pthread_mutex_unlock(&mutexGrille);
            Attente(160);
        }

        pthread_mutex_lock(&mutexGrille);

        EffaceCarre(pos->L,pos->C);
        setTab(pos->L,pos->C,VIDE, 0);      // Efface l'ancienne bombe

        pthread_mutex_unlock(&mutexGrille);
        pthread_exit(NULL); 
    }
    else
    {
        if (tab[pos->L][pos->C].type == BOUCLIER2)
        {
            printf("BOUCLIER 2\n");
            pthread_kill(tab[pos->L][pos->C].tid, SIGPIPE);

            pthread_mutex_unlock(&mutexGrille);
            pthread_exit(NULL); 
        }
        else if (tab[pos->L][pos->C].type == BOUCLIER1)
        {
            printf("BOUCLIER 1\n");
            pthread_kill(tab[pos->L][pos->C].tid, SIGPIPE);

            pthread_mutex_unlock(&mutexGrille);
            pthread_exit(NULL);
        }
        else if (tab[pos->L][pos->C].type == BOMBE)
        {
            printf("BOMBE\n");
            pthread_mutex_unlock(&mutexGrille);
            pthread_exit(NULL);
        }
        else if (tab[pos->L][pos->C].type == MISSILE)
        {
            printf("MISSILE\n");
            EffaceCarre(pos->L, pos->C);        // Si la case d'init. de la bombe est un missile alors on le supprime

            pthread_kill(tab[pos->L][pos->C].tid, SIGINT);
            setTab(pos->L,pos->C, VIDE, 0);

            pthread_mutex_unlock(&mutexGrille);
            pthread_exit(NULL);
        }
        else if (tab[pos->L][pos->C].type == ALIEN)
        {
            printf("ALIEN\n");
            pthread_mutex_unlock(&mutexGrille);
            pthread_exit(NULL);
        }
    }
    
    pthread_exit(NULL);

    return 0;
}

void *fctThAmiral()
{
    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGALRM);
    sigdelset(&mask, SIGCHLD);
    sigprocmask(SIG_SETMASK, &mask, NULL);

    bool DG;            // 0 = Gauche 1 = Droite
    int pos, temps;

    printf("Vaisseau Amiral\n");

    while(1)
    {
        if (!OK)
        {
            pthread_mutex_lock(&mutexGrille);
            EffaceCarre(0, pos);
            EffaceCarre(0, pos+1);
            setTab(0, pos, VIDE, 0);
            setTab(0, pos+1, VIDE, 0);
            pthread_mutex_unlock(&mutexGrille);
        }

        pthread_mutex_lock(&mutexAliens);
        pthread_cond_wait(&condFlotteAliens, &mutexAliens);
        pthread_mutex_unlock(&mutexAliens);

        OK = true;
        DG = (rand() % (2 + 1));
        pos = rand() % ((NB_COLONNE - 2) - 8 + 1) + 8;
        temps = rand() % (12 - 4 + 1) + 4;
        alarm(temps);

        // Initialisation de l'Amiral

        if (tab[0][pos].type == VIDE && tab[0][pos + 1].type == VIDE)
        {
            pthread_mutex_lock(&mutexGrille);
            DessineVaisseauAmiral(0, pos);
            setTab(0, pos, AMIRAL, pthread_self());
            setTab(0, pos + 1, AMIRAL, pthread_self());
            pthread_mutex_unlock(&mutexGrille);
        }

        while(OK)
        {
            Attente(200);

            // Effacement de l'Amiral

            pthread_mutex_lock(&mutexGrille);
            EffaceCarre(0, pos);
            EffaceCarre(0, pos+1);
            setTab(0, pos, VIDE, 0);
            setTab(0, pos+1, VIDE, 0);

            // Re-construction de l'Amiral

            if (DG)     // Chemin en allant de gauche à droite --->
            {
                if (pos + 1 > (NB_COLONNE - 2))     // Si on arrive hors du jeu alors... (trop à droite)
                {
                    pos = 8;    // On se remet tout à gauche

                    DessineVaisseauAmiral(0, pos);
                    setTab(0, pos, AMIRAL, pthread_self());
                    setTab(0, pos + 1, AMIRAL, pthread_self());
                }
                else if (tab[0][pos+2].type == MISSILE)
                {
                    printf("Amiral rencontre un missile");
                    // On ne fait rien
                }
                else
                {
                    pos++;
                    DessineVaisseauAmiral(0, pos);
                    setTab(0, pos, AMIRAL, pthread_self());
                    setTab(0, pos + 1, AMIRAL, pthread_self());
                }
            }
            else if (!DG) // Chemin en allant de droite à gauche <---
            {
                if (pos - 1 < 8)        // Si on arrive hors du jeu alors... (trop à gauche)
                {
                    pos = 21;   // On se remet tout à droite

                    DessineVaisseauAmiral(0, pos);
                    setTab(0, pos, AMIRAL, pthread_self());
                    setTab(0, pos + 1, AMIRAL, pthread_self());
                }
                else if (tab[0][pos-2].type == MISSILE)
                {
                    printf("Amiral rencontre un missile");
                    // On ne fait rien
                }
                else 
                {
                    pos--;
                    DessineVaisseauAmiral(0, pos);
                    setTab(0, pos, AMIRAL, pthread_self());
                    setTab(0, pos + 1, AMIRAL, pthread_self());
                }
            }
            pthread_mutex_unlock(&mutexGrille);
        }
    }
}

void *fctThBouclier(int *num)
{
    int numeroBouc = (*num);
    numeroBouc--;
    printf("Thread Bouclier %d : %ld\n",numeroBouc, pthread_self());

    sigset_t Masque;
	sigfillset(&Masque);
	sigdelset(&Masque, SIGPIPE);
	sigprocmask(SIG_SETMASK, &Masque, NULL);

    // Allocation dynamique + passage de l'état du bouclier en variable spécifique

    int * Etat_bouclier = (int*) malloc(sizeof(int));
    *Etat_bouclier=1;
    pthread_setspecific(cle_etat, Etat_bouclier);

    // Allocation dynamique + passage de la position du bouclier en variable spécifique

    int * numero_bouclier = (int*) malloc(sizeof(int));
    *numero_bouclier=numeroBouc;
    pthread_setspecific(cle_pos, numero_bouclier);

    pthread_mutex_unlock(&mutexBouclier);

    // Dessine le bouclier sur base du numéro reçu en param

    pthread_mutex_lock(&mutexGrille);
    setTab(NB_LIGNE - 2, 11+ numeroBouc, BOUCLIER1, pthread_self());
    DessineBouclier(NB_LIGNE - 2, 11+ numeroBouc, 1);
    pthread_mutex_unlock(&mutexGrille);

    if (*Etat_bouclier==2)
    {
        pthread_mutex_lock(&mutexGrille);
        setTab(NB_LIGNE - 2, 11+ numeroBouc, BOUCLIER2, pthread_self());
        DessineBouclier(NB_LIGNE - 2, 11+ numeroBouc, 2);
        pthread_mutex_unlock(&mutexGrille);
    }

    pause();
    pause();

    pthread_exit(NULL);
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

// Handler des Signaux

void HandlerSIGUSR1(int sig)        // Mouvement pour la direction de gauche
{
    if (colonne>8)      // Vérifie les dépassements + on pourrait vérifier avec un kill si le vaisseau est tjr en vie ou pas
    {
        pthread_mutex_lock(&mutexGrille);
        EffaceCarre(17, colonne);
        setTab(17,colonne,VIDE,0);
        colonne--;
        setTab(17,colonne,VAISSEAU,pthread_self());
        DessineVaisseau(17, colonne);
        pthread_mutex_unlock(&mutexGrille);
    }
    return;
}

void HandlerSIGUSR2(int sig)        // Mouvement pour la direction de droite
{
    if (colonne<22)     // Vérifie les dépassements
    {
        pthread_mutex_lock(&mutexGrille);
        EffaceCarre(17, colonne);
        setTab(17,colonne,VIDE,0);
        colonne++;
        setTab(17,colonne,VAISSEAU,pthread_self());
        DessineVaisseau(17, colonne);
        pthread_mutex_unlock(&mutexGrille);
    }
    return;
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

    return;
}

void HandlerSIGINT(int sig)
{
    printf("SIGINT\n");
    pthread_exit(NULL);

    return;
}

void HandlerSIGQUIT(int sig)
{
    printf("SIGQUIT\n");

    // Efface le vaisseau

    pthread_mutex_lock(&mutexGrille);
    EffaceCarre(NB_LIGNE-1, colonne);
    setTab(NB_LIGNE-1, colonne, VIDE, 0);
    pthread_mutex_unlock(&mutexGrille);

    pthread_exit(NULL);
    return;
}

void HandlerSIGALRM(int sig)
{
    printf("SIGALRM\n");

    OK=false;
    alarm(0);

    return;
}

void HandlerSIGCHLD(int sig)
{
    printf("SIGCHLD\n");

    OK=false;
    alarm(0);
    
    return;
}

void HandlerSIGPIPE(int sig)
{
    printf("SIGPIPE\n");

    int *num = (int*) pthread_getspecific(cle_pos);
    int *etat = (int*) pthread_getspecific(cle_etat);

    printf("Position du bouclier touché : %d\nEtat du bouclier touché : %d\n", *num, *etat);
    
    if (*etat==1)
    {
        printf("Tire sur un intact\n");
        setTab(NB_LIGNE - 2, 11+(*num), BOUCLIER2, pthread_self());
        DessineBouclier(NB_LIGNE - 2, 11+(*num), BOUCLIER2);
        *etat=2;
        pthread_setspecific(cle_etat, etat);
    }
    else if (*etat==2)
    {
        printf("Passage abimé\n");
        setTab(NB_LIGNE - 2, 11+(*num), VIDE, 0);
        EffaceCarre(NB_LIGNE - 2, 11+(*num));
    }
    

    return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

// Fonctions utiles

int RandomNumberPairImpair(bool PP, int compteur, int* current_alien)
{
    int rdm;
    if ((compteur % 2)==PP)     // PP = Pair ou impair
    {
        rdm=(rand() % (nbAliens+1));
        *current_alien=0;
    }

    return rdm;
}

void DecrementNbVies(void *p)
{
    printf("Fonction de decrementation !\n");

    // Paradigme de réveil

    pthread_mutex_lock(&mutexVies);
    nbVies--;
    pthread_mutex_unlock(&mutexVies);
    pthread_cond_signal(&condVies);
    printf("Salut à tous les gens\n");
}

void Destructeur(void *p)
{
    printf("---- (Destructeur) Libération zone spécifique ----\n");
    free(p);
}
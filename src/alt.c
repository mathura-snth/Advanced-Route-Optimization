#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <time.h>  // Pour clock_gettime (remplace sys/time.h)
#include <math.h>  // Pour la valeur absolue (fabs)
#include "algos.h"
#include "tas.h"
#include "graph.h"

//précalcul spécifique à alt

// avant de calculer l'heuristique d'alt, on doit connaitre la distance exacte entre chaque landmarks (points repères) et tous les autresz noeuds du graphe.
// on fait pour ça, dijkstra sur chaque landmark

/* Attention pour que le projet soit propre, cette fonction doit être appelée une 1 fois au début du programme pour chaque landmark.
Pcq le pré-calcul est très lent car il lance un Dijkstra complet sur tout le graphe
Donc une fois qu'on a les tableaux de distances, on peut répondre à des milliers de requêtes ALT presque instantanément sans jamais relancer dijkstra_pour_landmark.*/
double* dijkstra_pour_landmark(csr_graph_t *graphe, int landmark) {
    double *distances = malloc(graphe->nb_noeuds * sizeof(double));
    for (int i = 0; i < graphe->nb_noeuds; i++) {
        distances[i] = DBL_MAX;
    }
    tas_binaire_t *tas = tas_create(graphe->nb_aretes);
    distances[landmark] = 0.0;
    
    // dijkstra sans destination ni heurisitque
    tas_ajout(tas, landmark, 0.0, 0.0);

    while (tas->size > 0) {
        element_tas_t courant = tas_extraire_min(tas);
        int u = courant.sommet;

        if (courant.cout_reel > distances[u]) continue;

        int debut_aretes = graphe->first_edge[u];
        int fin_aretes = graphe->first_edge[u + 1];

        for (int i = debut_aretes; i < fin_aretes; i++) {
            int v = graphe->edges[i].cible;
            double poids = graphe->edges[i].poids;

            if (distances[u] + poids < distances[v]) {
                distances[v] = distances[u] + poids;
                tas_ajout(tas, v, distances[v], distances[v]);
            }
        }
    }
    tas_destroy(tas);
    
    // on retourne un tableau qui contient les distances du landmark vers tout le reste
    return distances; 
}

// heuristique alt : inégalité triangulaire
// dist(U,V) >= |dist(U,L) - dist(V,L)| => donne heuristique sans avoir besoin des coordonnées
double heuristique_alt(int u, int arrivee, int nb_landmarks, double **distances_landmarks) {
    double max_h = 0.0;
    // on vérifie tous les landmarks et on garde la plus grande estimation
    for (int i = 0; i < nb_landmarks; i++) {
        double dist_u_L = distances_landmarks[i][u];
        double dist_arrivee_L = distances_landmarks[i][arrivee];

        // on applique ineg triangulaire si les deux noeuds atteignent landmark
        if (dist_u_L != DBL_MAX && dist_arrivee_L != DBL_MAX) {
            double h = fabs(dist_u_L - dist_arrivee_L);
            // val max des landmarks pour augmenter précision
            if (h > max_h) {
                max_h = h;
            }
        }
    }
    return max_h;
}

resultat_t alt(csr_graph_t *graphe, int depart, int arrivee, int nb_landmarks, double **distances_landmarks) {
    double *distances = malloc(graphe->nb_noeuds * sizeof(double));
    int *predecesseurs = malloc(graphe->nb_noeuds * sizeof(int));

    for (int i = 0; i < graphe->nb_noeuds; i++) {
        distances[i] = DBL_MAX; 
        predecesseurs[i] = -1;      
    }

    struct timespec before, after;
    clockid_t clk_id = CLOCK_REALTIME;
    clock_gettime(clk_id, &before);

    tas_binaire_t * tas = tas_create(graphe->nb_aretes); 
    distances[depart] = 0.0;
    
    // heuristique initiale
    double h_depart = heuristique_alt(depart, arrivee, nb_landmarks, distances_landmarks);
    tas_ajout(tas, depart, h_depart, 0.0);

    long long nb_extractions = 0;
    long long nb_relaxations = 0;

    while (tas->size > 0) {
        element_tas_t courant = tas_extraire_min(tas);
        int u = courant.sommet;

        if (courant.cout_reel > distances[u]) continue;

        nb_extractions++;

        // early exit
        if (u == arrivee) break; 

        int debut_aretes = graphe->first_edge[u];
        int fin_aretes = graphe->first_edge[u + 1];

        for (int i = debut_aretes; i < fin_aretes; i++) {
            int v = graphe->edges[i].cible;
            double poids = graphe->edges[i].poids;

            if (distances[u] + poids < distances[v]) {
                nb_relaxations++; 
                distances[v] = distances[u] + poids;
                predecesseurs[v] = u;
                
                // nouvelle heurisitique basée sur landmarks
                double h = heuristique_alt(v, arrivee, nb_landmarks, distances_landmarks);
                
                // nouveua score
                double f = distances[v] + h;
                
                tas_ajout(tas, v, f, distances[v]);
            }
        }
    }

    clock_gettime(clk_id, &after);
    double temps_sec = (after.tv_sec - before.tv_sec) + (after.tv_nsec - before.tv_nsec) / 1e9;

    resultat_t res = {distances[arrivee], nb_extractions, nb_relaxations, temps_sec};
    free(distances);
    free(predecesseurs);
    tas_destroy(tas);
    return res;
}
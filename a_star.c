#include <stdio.h>
#include <stdlib.h>
#include <float.h> 
#include <time.h> 
#include <math.h>  // pour la formule de Haversine (sin, cos, sqrt, atan2)
#include "algos.h"
#include "tas.h"
#include "graph.h"

// fonction Haversine : calcule la distance à vol d'oiseau entre deux points elle sera h(v) pour A*
// en "static" car elle est utile que dans ce fichier
static double haversine(double lat1, double lon1, double lat2, double lon2) {
    double R = 6371000.0; // Rayon moyen de la Terre en mètres
    double dLat = (lat2 - lat1) * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    lat1 = lat1 * M_PI / 180.0;
    lat2 = lat2 * M_PI / 180.0;

    double a = sin(dLat/2) * sin(dLat/2) +
               cos(lat1) * cos(lat2) * sin(dLon/2) * sin(dLon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return R * c;
}

// Algo A* :

resultat_t algo_a_star(csr_graph_t *graphe, coordonnees_t *coords, int depart, int arrivee) {
    printf("\nRecherche A* de %d vers %d...\n", depart, arrivee);

    // comme dijkstra, on garde vraie distance parcourue g :
    // on fait deux tableaux :
    // 1 pour garder en mémoire le plus court chemin trouvé jusqu'à présent pour chaque noeud
    // 2 permet de retracer le chemin à l'envers une fois arrivé
    double *distances = malloc(graphe->nb_noeuds * sizeof(double));
    int *predecesseurs = malloc(graphe->nb_noeuds * sizeof(int));

    for (int i = 0; i < graphe->nb_noeuds; i++) {
        distances[i] = DBL_MAX; 
        predecesseurs[i] = -1;      
    }
    // temps de l'algo
    struct timespec before, after;
    clockid_t clk_id = CLOCK_REALTIME;
    clock_gettime(clk_id, &before);

    tas_binaire_t * tas = tas_create(graphe->nb_aretes); 
    distances[depart] = 0.0;
    
    // Calcul de l'heuristique initiale h : départ
    double h_depart = haversine(coords[depart].lat, coords[depart].lon, 
                                coords[arrivee].lat, coords[arrivee].lon);
    
    // on insère le noeud de départ, f = h_depart et distance réelle g = 0
    tas_ajout(tas, depart, h_depart, 0.0);

    // performance de l'algo
    long long nb_extractions = 0;
    long long nb_relaxations = 0;

    while (tas->size > 0) {

        element_tas_t courant = tas_extraire_min(tas); // on extrait le noeud le plus proche du point de départ
        int u = courant.sommet;

        // Modification de lazy deletion pour A* :
        // on compare g sauvegardée dans le tas avec la meilleure distance conneu dans le tableau.
        if (courant.cout_reel > distances[u]) continue;

        nb_extractions++;

        // Modification du early exit pour A* :
        // Comme h ne surestime JAMAIS, quand la destination sort du tas, on sait qu'on a trouvé le plus court chemin
        if (u == arrivee) break; 

        // recupération des voisins de u (en O(1) grace CSR)
        int debut_aretes = graphe->first_edge[u]; // indice de dep
        int fin_aretes = graphe->first_edge[u + 1]; // indice de fin

        // parcourt arêtes sortantes du sommet u
        for (int i = debut_aretes; i < fin_aretes; i++) {
            int v = graphe->edges[i].cible;
            double poids = graphe->edges[i].poids; // cout de l'arete entre u et v


            // relaxation : si on passe par u, est ce que le chemin pour atteindre v est plus court que l'ancienne distance qu'on connaissait pr v ?
            if (distances[u] + poids < distances[v]) { // oui
                nb_relaxations++; 
                distances[v] = distances[u] + poids;
                predecesseurs[v] = u;
                
                // calcul de la nouvelle heuristique h(v) pour le voisin
                double h = haversine(coords[v].lat, coords[v].lon, 
                                     coords[arrivee].lat, coords[arrivee].lon);
                
                // nouveau score f(v) = vraie distance g + estimation h
                double f = distances[v] + h;
                
                // on insère dans le tas le score f pour trier et g pour lazy deletion
                tas_ajout(tas, v, f, distances[v]);
            }
        }
    }
    // fin de chrono
    clock_gettime(clk_id, &after);
    double temps_sec = (after.tv_sec - before.tv_sec) + (after.tv_nsec - before.tv_nsec) / 1e9;
    // si toujjours infini alors que tas vidé, alors les 2 points ne sont pas connectés dans le graphe

    resultat_t res = {distances[arrivee], nb_extractions, nb_relaxations, temps_sec};
    if (distances[arrivee] == DBL_MAX) {
        fprintf(stderr, "Erreur : Aucun chemin trouvé.\n");
    } else {
        printf("- Distance totale (g) : %.2f metres\n", distances[arrivee]);
        printf("- Extractions (Noeuds visites) : %lld\n", nb_extractions);
        printf("- Relaxations                  : %lld\n", nb_relaxations);
        printf("- Temps d'execution            : %lf secondes\n", temps_sec);
    }

    free(distances);
    free(predecesseurs);
    tas_destroy(tas);
    return res;
}
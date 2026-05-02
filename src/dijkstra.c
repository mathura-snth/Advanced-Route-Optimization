#include <stdio.h>
#include <stdlib.h>
#include <float.h> // Pour INFINI (l'infini)
#include <time.h>  // Pour clock_gettime
#include "algos.h"
#include "tas.h"
#include "graph.h"

resultat_t dijkstra(csr_graph_t *graphe, int depart, int arrivee) {
    // on fait deux tableaux :
    // 1 pour garder en mémoire le plus court chemin trouvé jusqu'à présent pour chaque noeud
    // 2 permet de retracer le chemin à l'envers une fois arrivé
    double *distances = malloc(graphe->nb_noeuds * sizeof(double));
    int *predecesseurs = malloc(graphe->nb_noeuds * sizeof(int));

    for (int i = 0; i < graphe->nb_noeuds; i++) {
        distances[i] = INFINI; 
        predecesseurs[i] = -1;      
    }
    // temps de l'algo
    struct timespec before, after;
    clockid_t clk_id = CLOCK_REALTIME;
    clock_gettime(clk_id, &before);

    //choix stratégie paresseuse
    tas_binaire_t * tas = tas_create(graphe->nb_aretes);

    distances[depart] = 0.0;
    tas_ajout(tas, depart, 0.0, 0.0);

    // performance de l'algo
    long long nb_extractions = 0;
    long long nb_relaxations = 0;

    while (tas->size > 0) {

        element_tas_t courant = tas_extraire_min(tas); // on extrait le noeud le plus proche du point de départ
        int u = courant.sommet;

        // on insère des doublons au lieu de supp la distance obsolète
        if (courant.cout_reel > distances[u]) continue; 
        
        // noeud validé et exploré => incrémentation du compteur
        nb_extractions++;

        // early exit -> propriété du dijkstra : si on réussit à extraire le noeud de destination
        // c'est qu'on a trouvé le plus court chemin définitif pour l'atteindre => stop recherche peu importe le nb de noeuds restants
        if (u == arrivee) break;

        // recupération des voisins de u (en O(1) grace CSR)
        int debut_aretes = graphe->first_arete[u]; // indice de dep
        int fin_aretes = graphe->first_arete[u + 1]; // indice de fin

        // parcourt arêtes sortantes du sommet u
        for (int i = debut_aretes; i < fin_aretes; i++) {
            int v = graphe->aretes[i].cible;
            double poids = graphe->aretes[i].poids; // cout de l'arete entre u et v

            // relaxation : si on passe par u, est ce que le chemin pour atteindre v est plus court que l'ancienne distance qu'on connaissait pr v ?
            if (distances[u] + poids < distances[v]) { // oui
                nb_relaxations++; // on a trouvé raccourci
                distances[v] = distances[u] + poids; // maj nouvelle distance dans tableau résultat
                predecesseurs[v] = u; // memo qu'on est passé par u pour aller vers v
                tas_ajout(tas, v, distances[v], distances[v]); // ajout de la paire dans tas binaire
            }
        }
    }
    // fin de chrono
    clock_gettime(clk_id, &after);
    double temps_sec = (after.tv_sec - before.tv_sec) + (after.tv_nsec - before.tv_nsec) / 1000000000.0;

    free(distances);
    free(predecesseurs);
    tas_destroy(tas);
    resultat_t res = {distances[arrivee], nb_extractions, nb_relaxations, temps_sec};
    return res;
}
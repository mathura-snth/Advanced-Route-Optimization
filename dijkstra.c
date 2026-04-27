#include <stdio.h>
#include <stdlib.h>
#include <float.h> // Pour DBL_MAX (l'infini)
#include <time.h>  // Pour clock_gettime
#include "algos.h"
#include "tas.h"
#include "graph.h"

resultat_t dijkstra(csr_graph_t *graphe, int depart, int arrivee) {
    printf("\nRecherche de %d vers %d\n", depart, arrivee);

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


    // DÉCISION DE CONCEPTION CRITIQUE : PLUTÔT QUE DE MODIFIER LES DISTANCES DANS LE TAS (CE QUI EST LENT ET COMPLEXE), 
    // ON PRÉFÈRE L'APPROCHE "LAZY DELETION" : ON AJOUTERA DES DOUBLONS.
    // LE PIRE CAS POSSIBLE EST QUE CHAQUE ARÊTE DU GRAPHE GÉNIÈRE UNE INSERTION. 
    // LA CAPACITÉ DU TAS EST DONC FIXÉE À nb_aretes POUR ÉVITER TOUT RISQUE DE DÉBORDEMENT.
    tas_binaire_t * tas = tas_create(graphe->nb_aretes);

    distances[depart] = 0.0;
    tas_ajout(tas, depart, 0.0);

    // performance de l'algo
    long long nb_extractions = 0;
    long long nb_relaxations = 0;

    while (tas->size > 0) {

        element_tas_t courant = tas_extraire_min(tas); // on extrait le noeud le plus proche du point de départ
        int u = courant.sommet;


        // C'EST ICI QU'OPÈRE LA MAGIE DE LA "LAZY DELETION" :
        // PUISQU'ON INSÈRE DES DOUBLONS PLUTÔT QUE DE METTRE À JOUR LE TAS, 
        // ON PEUT DÉPILER UN SOMMET DONT LA DISTANCE EST OBSOLÈTE (PLUS GRANDE QUE LA MEILLEURE TROUVÉE ENTRE TEMPS).
        // SI C'EST LE CAS, ON L'IGNORE ET ON PASSE AU SUIVANT DIRECTEMENT. ÇA ÉVITE DES CALCULS INUTILES.
        if (courant.distance > distances[u]) continue; 
        
        // noeud validé et exploré => incrémentation du compteur
        nb_extractions++;

        // early exit -> propriété du dijkstra : si on réussit à extraire le noeud de destination
        // c'est qu'on a trouvé le plus court chemin définitif pour l'atteindre => stop recherche peu importe le nb de noeuds restants
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
                nb_relaxations++; // on a trouvé raccourci
                distances[v] = distances[u] + poids; // maj nouvelle distance dans tableau résultat
                predecesseurs[v] = u; // memo qu'on est passé par u pour aller vers v
                tas_ajout(tas, v, distances[v]); // ajout de la paire dans tas binaire
            }
        }
    }
    // fin de chrono
    clock_gettime(clk_id, &after);
    double temps_sec = (after.tv_sec - before.tv_sec) + (after.tv_nsec - before.tv_nsec) / 1e9;
    // si toujjours infini alors que tas vidé, alors les 2 points ne sont pas connectés dans le graphe
    if (distances[arrivee] == DBL_MAX) {
        fprintf(stderr, "Erreur : Aucun chemin trouvé.\n");
    } else {
        printf("- Distance trouvee : %.2f\n", distances[arrivee]);
        printf("- Extractions      : %lld\n", nb_extractions);
        printf("- Relaxations      : %lld\n", nb_relaxations);
        printf("- Temps d'execution: %lf secondes\n", temps_sec);
    }

    free(distances);
    free(predecesseurs);
    tas_destroy(tas);
}
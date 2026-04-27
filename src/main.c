#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "graph.h"
#include "algos.h"

// Petite fonction utilitaire pour afficher les résultats de chaque algo proprement
void afficher_resultat(const char* nom_algo, resultat_t res) {
    printf("\n RESULTATS : %s\n", nom_algo);
    if (res.distance == __DBL_MAX__) {
        printf("-> ECHEC : Aucun chemin trouve.\n");
    } else {
        printf("- Distance trouvee : %.2f metres\n", res.distance);
        printf("- Extractions      : %lld noeuds\n", res.extractions);
        printf("- Relaxations      : %lld arêtes\n", res.relaxations);
        printf("- Temps d'execution: %.6lf secondes\n", res.temps_sec);
    }
}

int main() {
    // 1) CHARGEMENT DES DONNÉES
    printf("Chargement du graphe CSR\n");
    csr_graph_t *graphe = load_graph("data/edges.txt");
    if (!graphe) return EXIT_FAILURE;

    printf("Chargement des coordonnees (pour A*)\n");
    coordonnees_t *coords = charger_coordonnees("data/nodes.txt", graphe->nb_noeuds);
    if (!coords) {
        free_graph(graphe);
        return EXIT_FAILURE;
    }

    int depart = 15;
    int arrivee = 1466593; 

    printf("Lancement du moteur GPS : %d -> %d\n", depart, arrivee);

    // 2) DIJKSTRA (baseline)
    resultat_t res_dijkstra = dijkstra(graphe, depart, arrivee);
    afficher_resultat("DIJKSTRA (Classique)", res_dijkstra);

    // 3. A* (heuristique haversine)
    resultat_t res_astar = a_star(graphe, coords, depart, arrivee);
    afficher_resultat("A* (Vol d'oiseau)", res_astar);

    // 4. ALT (landmarks et inégalité triangulaire)
    int nb_landmarks = 10;
    int *landmarks = malloc(nb_landmarks * sizeof(int));
    
    // On fixe la seed => reproductibilité
    srand(42); 
    printf("\n Pré-calcul pour alt\n");
    printf("Sélection de %d landmarks\n", nb_landmarks);
    for (int i = 0; i < nb_landmarks; i++) {
        landmarks[i] = rand() % graphe->nb_noeuds;
    }
    
    struct timespec pre_before, pre_after;
    clock_gettime(CLOCK_REALTIME, &pre_before);
    
    // tableau de distances pour chaque landmarks
    double **distances_landmarks = malloc(nb_landmarks * sizeof(double*));
    for (int i = 0; i < nb_landmarks; i++) {
        distances_landmarks[i] = dijkstra_pour_landmark(graphe, landmarks[i]);
    }
    
    clock_gettime(CLOCK_REALTIME, &pre_after);
    double temps_precalc_alt = (pre_after.tv_sec - pre_before.tv_sec) + (pre_after.tv_nsec - pre_before.tv_nsec) / 1e9;
    printf("-> Pre-calculs ALT termines en %.2f secondes.\n", temps_precalc_alt);

    // requete alt
    resultat_t res_alt = alt(graphe, depart, arrivee, nb_landmarks, distances_landmarks);
    afficher_resultat("ALT (Landmarks)", res_alt);

    // 5. CONTRACTION HIERARCHIES
    ch_graph_t *ch = ch_preprocess(graphe); // Fait le Witness Search et crée le graphe Upward
    
    resultat_t res_ch = ch_search(ch, depart, arrivee);
    afficher_resultat("CONTRACTION HIERARCHIES", res_ch);

    // 6. LIBÉRATION DE MÉMOIRE
    for (int i = 0; i < nb_landmarks; i++) {
        free(distances_landmarks[i]);
    }
    free(distances_landmarks);
    free(landmarks);
    
    free_ch(ch);
    free(coords);
    free_graph(graphe);

    return EXIT_SUCCESS;
}
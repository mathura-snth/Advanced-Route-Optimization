#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <float.h>
#include "graph.h"
#include "algos.h"
#include "tas.h"
#include "analyzer.h" // Module du TP

#define NB_REQUETES 1000
#define SEED 42

void run_evaluation(csr_graph_t *graphe, coordonnees_t *coords, ch_graph_t *ch, double **distances_landmarks, int nb_landmarks) {
    // Initialisation des analyseurs pour le temps (inspiré du TP)
    analyzer_t *t_dijkstra = analyzer_create();
    analyzer_t *t_astar    = analyzer_create();
    analyzer_t *t_alt      = analyzer_create();
    analyzer_t *t_ch       = analyzer_create();

    // Initialisation des analyseurs pour l'espace de recherche (extractions)
    analyzer_t *e_dijkstra = analyzer_create();
    analyzer_t *e_astar    = analyzer_create();
    analyzer_t *e_alt      = analyzer_create();
    analyzer_t *e_ch       = analyzer_create();

    srand(SEED);
    int success_count = 0;

    printf("Lancement de %d requêtes aléatoires...\n", NB_REQUETES);

    for (int i = 0; i < NB_REQUETES; i++) {
        int s = rand() % graphe->nb_noeuds;
        int t = rand() % graphe->nb_noeuds;

        // 1. Dijkstra (Baseline)
        resultat_t res_d = dijkstra(graphe, s, t);
        if (res_d.distance == DBL_MAX) continue; // On ignore les points non connectés

        // 2. A*
        resultat_t res_a = a_star(graphe, coords, s, t);
        
        // 3. ALT
        resultat_t res_l = alt(graphe, s, t, nb_landmarks, distances_landmarks);

        // 4. CH
        resultat_t res_c = ch_search(ch, s, t);

        // Vérification de la correction (Strict)
        double epsilon = 0.01;
        if (fabs(res_a.distance - res_d.distance) > epsilon || 
            fabs(res_c.distance - res_d.distance) > epsilon) {
            fprintf(stderr, "[ERREUR] Divergence de distance au trajet %d->%d\n", s, t);
            continue;
        }

        // Enregistrement des données dans les analyseurs
        analyzer_append(t_dijkstra, res_d.temps_sec);
        analyzer_append(e_dijkstra, (double)res_d.extractions);

        analyzer_append(t_astar, res_a.temps_sec);
        analyzer_append(e_astar, (double)res_a.extractions);

        analyzer_append(t_alt, res_l.temps_sec);
        analyzer_append(e_alt, (double)res_l.extractions);

        analyzer_append(t_ch, res_c.temps_sec);
        analyzer_append(e_ch, (double)res_c.extractions);

        success_count++;
    }

    // Sauvegarde des fichiers .plot pour analyse ultérieure (Python/Gnuplot)
    save_values(t_dijkstra, "results/time_dijkstra.plot");
    save_values(t_astar,    "results/time_astar.plot");
    save_values(t_alt,      "results/time_alt.plot");
    save_values(t_ch,       "results/time_ch.plot");

    // Affichage du bilan statistique (Moyennes et Écart-types)
    printf("\n--- BILAN SUR %d REQUETES REUSSIES ---\n", success_count);
    printf("Algo      | Temps Moyen (ms) | Extractions Moy. | Ecart-type Temps\n");
    printf("Dijkstra  | %12.4Lf | %16.0Lf | %12.4Lf\n", get_average_cost(t_dijkstra)*1000, get_average_cost(e_dijkstra), get_standard_deviation(t_dijkstra));
    printf("A*        | %12.4Lf | %16.0Lf | %12.4Lf\n", get_average_cost(t_astar)*1000, get_average_cost(e_astar), get_standard_deviation(t_astar));
    printf("ALT       | %12.4Lf | %16.0Lf | %12.4Lf\n", get_average_cost(t_alt)*1000, get_average_cost(e_alt), get_standard_deviation(t_alt));
    printf("CH        | %12.4Lf | %16.0Lf | %12.4Lf\n", get_average_cost(t_ch)*1000, get_average_cost(e_ch), get_standard_deviation(t_ch));

    // Nettoyage
    analyzer_destroy(t_dijkstra); analyzer_destroy(e_dijkstra);
    analyzer_destroy(t_astar);    analyzer_destroy(e_astar);
    analyzer_destroy(t_alt);      analyzer_destroy(e_alt);
    analyzer_destroy(t_ch);       analyzer_destroy(e_ch);
}
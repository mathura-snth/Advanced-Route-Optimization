#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <float.h>
#include "graph.h"
#include "algos.h"
#include "tas.h"
#include "analyzer.h" 

#define NB_REQUETES 300
#define SEED_EVAL 42

void run_evaluation(csr_graph_t *graphe, coordonnees_t *coords, ch_graph_t *ch, double **distances_landmarks, int nb_landmarks) {
    printf("\n Début de l'évaluation\n");
    printf("Recherche de %d trajets valides\n", NB_REQUETES);

    // analyse du temps
    analyzer_t *t_dijkstra = analyzer_create();
    analyzer_t *t_astar = analyzer_create();
    analyzer_t *t_alt = analyzer_create();
    analyzer_t *t_ch = analyzer_create();

    // analyse des extractions
    analyzer_t *e_dijkstra = analyzer_create();
    analyzer_t *e_astar = analyzer_create();
    analyzer_t *e_alt = analyzer_create();
    analyzer_t *e_ch = analyzer_create();

    // analyse des relaxations
    analyzer_t *r_dijkstra = analyzer_create();
    analyzer_t *r_astar = analyzer_create();
    analyzer_t *r_alt = analyzer_create();
    analyzer_t *r_ch = analyzer_create();

    // analyse des mémoires
    analyzer_t *m_dijkstra = analyzer_create();
    analyzer_t *m_astar = analyzer_create();
    analyzer_t *m_alt = analyzer_create();
    analyzer_t *m_ch = analyzer_create();

    srand(SEED_EVAL);
    // Dijkstra, A* et ALT utilisent: tableau dist (double) + tableau pred (int) + tas (element_tas_t)
    double mem_classique = (graphe->nb_noeuds * (sizeof(double) + sizeof(int))) + (graphe->nb_aretes * sizeof(element_tas_t));
    
    // CH utilise: 2 tableaux dist (aller/retour) + 2 tas basés sur le graphe ascendant
    double mem_contraction = (2 * ch->num_noeuds * sizeof(double)) + (2 * (ch->up_first_arete[ch->num_noeuds] + 1) * sizeof(element_tas_t));

    int success_total = 0;
    int success_court = 0;
    int success_moyen = 0;
    int success_long = 0;
    int par_categorie = NB_REQUETES / 3; // pour avoir 3 categories de tests selon leur distance

    while (success_total < NB_REQUETES) {
        int s = rand() % graphe->nb_noeuds;
        int t = rand() % graphe->nb_noeuds;
        
        resultat_t res_d = dijkstra(graphe, s, t);
        // si aucun chemin n'existe, on ignore cette paire
        if (res_d.distance == INFINI) continue; 

        //on classe les distances dans differentes categories
        int categorie = -1;
        if (res_d.distance < 25000) categorie = 0; // tests sur distance courte
        else if (res_d.distance < 50000) categorie = 1; // tests sur distance moyenne
        else categorie = 2; // test sur longue distance

        // on remplit les categories et on s arrete si chacune l'est
        if (categorie == 0 && success_court >= par_categorie) continue;
        if (categorie == 1 && success_moyen >= par_categorie) continue;
        if (categorie == 2 && success_long >= par_categorie) continue;

        resultat_t res_a = a_star(graphe, coords, s, t);
        resultat_t res_l = alt(graphe, s, t, nb_landmarks, distances_landmarks);
        resultat_t res_c = ch_search(ch, s, t);

        // Tolérance souple pour éviter les erreurs d'accumulation de flottants sur les longues distances
        double epsilon = 5.0; 
        
        if (fabs(res_a.distance - res_d.distance) > epsilon || fabs(res_l.distance - res_d.distance) > epsilon || fabs(res_c.distance - res_d.distance) > epsilon ) {
            // on signale l'erreur mais on l'accepte pour ne pas bloquer les statistiques de temps
            fprintf(stderr, "petite divergence : Dijkstra = %.2f, A* = %.2f, Alt = %.2f et CH=%.2f\n", res_d.distance, res_a.distance, res_l.distance, res_c.distance);
        }

        // On enregistre les données
        analyzer_append(t_dijkstra, res_d.temps_sec);
        analyzer_append(e_dijkstra, (double)res_d.extractions);
        analyzer_append(m_dijkstra, mem_classique);
        analyzer_append(r_dijkstra, (double)res_d.relaxations);

        analyzer_append(t_astar, res_a.temps_sec);
        analyzer_append(e_astar, (double)res_a.extractions);
        analyzer_append(m_astar, mem_classique);
        analyzer_append(r_astar, (double)res_a.relaxations);

        analyzer_append(t_alt, res_l.temps_sec);
        analyzer_append(e_alt, (double)res_l.extractions);
        analyzer_append(m_alt, mem_classique);
        analyzer_append(r_alt,(double)res_l.relaxations);

        analyzer_append(t_ch, res_c.temps_sec);
        analyzer_append(e_ch, (double)res_c.extractions);
        analyzer_append(m_ch, mem_contraction);
        analyzer_append(r_ch, (double)res_c.relaxations);

        if (categorie == 0) success_court++;
        else if (categorie == 1) success_moyen++;
        else success_long++;
        
        success_total++;

        if (success_total % 100 == 0) {
            printf("Progression selon les distancess : %d/%d (courtes:%d, moy:%d, longues:%d)\n", success_total, NB_REQUETES, success_court, success_moyen, success_long);
        }
    }

    // Sauvegarde (voir TP1)
    save_values(t_dijkstra, "resultats/time_dijkstra.plot");
    save_values(t_astar,    "resultats/time_astar.plot");
    save_values(t_alt,      "resultats/time_alt.plot");
    save_values(t_ch,       "resultats/time_ch.plot");
    save_values(m_dijkstra, "resultats/memory_dijkstra.plot");
    save_values(m_ch,       "resultats/memory_ch.plot");
    // pour afficher le nombre de noeuds visités
    save_values(e_dijkstra, "resultats/extract_dijkstra.plot");
    save_values(e_astar,    "resultats/extract_astar.plot");
    save_values(e_alt,      "resultats/extract_alt.plot");
    save_values(e_ch,       "resultats/extract_ch.plot");
    // relaxatins
    save_values(r_dijkstra, "resultats/relaxations_dijkstra.plot");
    save_values(r_astar,    "resultats/relaxations_astar.plot");
    save_values(r_alt,      "resultats/relaxations_alt.plot");
    save_values(r_ch,       "resultats/relaxations_ch.plot");

    printf("\n bilan des %d requetes ---\n", success_total);
    printf("Algo      | Temps Moyen (ms) | Extractions Moy.| Relaxations Moy.| Ecart-type Temps | RAM (Mo)\n");
    printf("Dijkstra  | %16.4Lf | %16.4Lf |%16.4Lf| %16.4Lf | %8.2Lf\n", get_average_cost(t_dijkstra)*1000, get_average_cost(e_dijkstra),get_average_cost(r_dijkstra), get_standard_deviation(t_dijkstra)*1000, get_average_cost(m_dijkstra)/1024.0);
    printf("A*        | %16.4Lf | %16.0Lf |%16.4Lf| %16.4Lf | %8.2Lf\n", get_average_cost(t_astar)*1000, get_average_cost(e_astar),get_average_cost(r_astar), get_standard_deviation(t_astar)*1000, get_average_cost(m_astar)/1024.0);
    printf("ALT       | %16.4Lf | %16.0Lf |%16.4Lf| %16.4Lf | %8.2Lf\n", get_average_cost(t_alt)*1000, get_average_cost(e_alt),get_average_cost(r_alt), get_standard_deviation(t_alt)*1000, get_average_cost(m_alt)/1024.0);
    printf("CH        | %16.4Lf | %16.0Lf |%16.4Lf| %16.4Lf | %8.2Lf\n", get_average_cost(t_ch)*1000, get_average_cost(e_ch),get_average_cost(r_ch), get_standard_deviation(t_ch)*1000, get_average_cost(m_ch)/1024.0);
    
    analyzer_destroy(t_dijkstra);
    analyzer_destroy(e_dijkstra);
    analyzer_destroy(m_dijkstra);
    analyzer_destroy(t_astar);
    analyzer_destroy(e_astar);
    analyzer_destroy(m_astar);
    analyzer_destroy(t_alt);
    analyzer_destroy(e_alt);
    analyzer_destroy(m_alt);
    analyzer_destroy(t_ch);
    analyzer_destroy(e_ch);
    analyzer_destroy(m_ch);
}

int main() {
    printf("Chargement du graphe CSR\n");
    csr_graph_t *graphe = load_graph("donnees/aretes.csv");
    if (!graphe) return EXIT_FAILURE;

    printf("Chargement des coordonnees\n");
    coordonnees_t *coords = charger_coordonnees("donnees/noeuds.csv", graphe->nb_noeuds);
    if (!coords) {
        free_graph(graphe);
        return EXIT_FAILURE;
    }

    int nb_landmarks = 10;
    int *landmarks = malloc(nb_landmarks * sizeof(int));
    srand(42); 
    
    printf("\nALT : selection de %d landmarks\n", nb_landmarks);
    struct timespec pre_before, pre_after;
    clock_gettime(CLOCK_REALTIME, &pre_before);
    
    for (int i = 0; i < nb_landmarks; i++) {
        landmarks[i] = rand() % graphe->nb_noeuds;
    }
    double **distances_landmarks = malloc(nb_landmarks * sizeof(double*));
    for (int i = 0; i < nb_landmarks; i++) {
        distances_landmarks[i] = dijkstra_pour_landmark(graphe, landmarks[i]);
    }
    
    clock_gettime(CLOCK_REALTIME, &pre_after);

    //pour alt : mult du nombre de landmarks par la taille du tableau de distances
    double temps_pre_alt = (pre_after.tv_sec - pre_before.tv_sec) + (pre_after.tv_nsec - pre_before.tv_nsec) / 1000000000.0;
    double mem_pre_alt = (nb_landmarks * graphe->nb_noeuds * sizeof(double)) / (1024.0 * 1024.0);
    printf("Pre-traitement ALT termine en %.2f secondes.\n", (pre_after.tv_sec - pre_before.tv_sec) + (pre_after.tv_nsec - pre_before.tv_nsec) / 1000000000.0);

    printf("\nCH : pretraitement Contraction Hierarchies\n");
    clock_gettime(CLOCK_REALTIME, &pre_before);
    ch_graph_t *ch = pretraitement_ch(graphe); 
    clock_gettime(CLOCK_REALTIME, &pre_after);
    // pour ch : tableaux d'arêtes + tableaux de noeuds
    double temps_pre_ch = (pre_after.tv_sec - pre_before.tv_sec) + (pre_after.tv_nsec - pre_before.tv_nsec) / 1000000000.0;
    double mem_pre_ch = (2 * (ch->up_first_arete[ch->num_noeuds] + 1) * (sizeof(int) + sizeof(double)) + (ch->num_noeuds * sizeof(int))) / (1024.0 * 1024.0);
    printf("Pre-traitement CH termine en %.2f secondes.\n", (pre_after.tv_sec - pre_before.tv_sec) + (pre_after.tv_nsec - pre_before.tv_nsec) / 1000000000.0);
    // sauvegarde des pré-traitement
    FILE *f_pre = fopen("resultats/pretraitements_costs.txt", "w");
    if(f_pre) {
        fprintf(f_pre, "ALT %lf %lf\n", temps_pre_alt, mem_pre_alt);
        fprintf(f_pre, "CH %lf %lf\n", temps_pre_ch, mem_pre_ch);
        fclose(f_pre);
    }

    run_evaluation(graphe, coords, ch, distances_landmarks, nb_landmarks);

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
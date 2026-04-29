#ifndef ALGOS_H
#define ALGOS_H
#include "graph.h"

// Structure unique pour récupérer les stats de n'importe quel algo
typedef struct {
    double distance;
    long long extractions;
    long long relaxations;
    double temps_sec;
} resultat_t;

// Prototypes des algorithmes
resultat_t dijkstra(csr_graph_t *g, int start, int end);
resultat_t a_star(csr_graph_t *g, coordonnees_t *coords, int start, int end);
resultat_t alt(csr_graph_t *g, int start, int end, int nb_l, double **dist_l);
double* dijkstra_pour_landmark(csr_graph_t *graphe, int landmark);

typedef struct {
    int num_nodes;
    int *rank;
    int *up_first_edge;
    arete_t *up_edges;
} ch_graph_t;

ch_graph_t* pretraitement_ch(csr_graph_t *graphe);
resultat_t ch_search(ch_graph_t *ch, int start, int end);
void free_ch(ch_graph_t *ch);
#endif
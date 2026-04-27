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

#endif
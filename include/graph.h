#ifndef GRAPH_H
#define GRAPH_H
#include <stdio.h>

// structure de données
// on ne stocke pas le noeud de départ car CSR nous donne à quel noeud appartient l'arrête
typedef struct {
    int cible;
    double poids;
} arete_t;

// toutes les arêtes sont dans un énorme tableau contigu
typedef struct {
    int nb_noeuds;
    int nb_aretes;
    int *first_arete; // tableau des offsets = index
    arete_t *aretes;  // tableau de toutes les arêtes les unes à la suite des autres
} csr_graph_t;

// coordonnées + heuristique :
// Pour A* on doit connaitre les coordonnées pour calculer la distance à vol d'oiseau :
typedef struct {
    double lat;
    double lon;
} coordonnees_t;

csr_graph_t* load_graph(const char *filename);
coordonnees_t* charger_coordonnees(const char *filename, int nb_noeuds);
void free_graph(csr_graph_t *g);

#endif
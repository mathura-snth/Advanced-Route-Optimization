#ifndef TAS_H
#define TAS_H

// Structure du tas binaire : on stocke la paire (sommet, distance) pour savoir qui extraire
// dans dijkstra, le tas binaire doit trier les nombres mais aussi savoir a quel somemt appartient la distance pour l'extraire
typedef struct {
    int sommet; // num du noeud dans graphe
    double distance; // clé de tri = distance cumulée depuis le départ
} element_tas_t;

typedef struct {
    element_tas_t *data; // tableau dynamique contenant les paires (sommet, dist)
    int size; // nb d'éléments actuellement dans le tas
    int capacity; // taille max tableau
} tas_binaire_t;

// Prototypes des fonctions
tas_binaire_t* tas_create(int capacity);
void tas_destroy(tas_binaire_t *tas);
void tas_ajout(tas_binaire_t *tas, int sommet, double distance);
element_tas_t tas_extraire_min(tas_binaire_t *tas);

#endif
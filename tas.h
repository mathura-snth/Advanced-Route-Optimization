#ifndef TAS_H
#define TAS_H


// File de priorité (Tas binaire) :
// File de priorité différente pour A* : on doit ici connaitre le score global f 
// pour trier les noeuds et on doit garder la vraie distance g pour le lazy deletion.
// (Pour Dijkstra, score et cout_reel vaudront simplement la même valeur).
// dans dijkstra, le tas binaire doit trier les nombres mais aussi savoir a quel somemt appartient la distance pour l'extraire
// La structure peut maintenant servir pour Dijkstra (sans heuristique) et A* (avec heuristique)
typedef struct {
    int sommet; // num du noeud dans graphe
    double score; // clé de tri (pour Dijkstra ça reste la distance et pour A* ça devient f=g+h)
    double cout_reel; // Vraie distance depuis le départ (Dijkstra: distance | A*: g) pour la lazy deletion
} element_tas_t;

typedef struct {
    element_tas_t *data; // tableau dynamique contenant les paires (sommet, dist)
    int size; // nb d'éléments actuellement dans le tas
    int capacity; // taille max tableau
} tas_binaire_t;

// Prototypes des fonctions
tas_binaire_t* tas_create(int capacity);
void tas_destroy(tas_binaire_t *tas);
void tas_ajout(tas_binaire_t *tas, int sommet, double score, double cout_reel);
element_tas_t tas_extraire_min(tas_binaire_t *tas);

#endif
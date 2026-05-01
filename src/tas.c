#include "tas.h"
#include <stdlib.h>

tas_binaire_t* tas_create(int capacity) {
    tas_binaire_t *tas = malloc(sizeof(tas_binaire_t));
    tas->capacity = capacity;
    tas->size = 0;
    tas->data = malloc(capacity * sizeof(element_tas_t));
    return tas;
}

void tas_destroy(tas_binaire_t * tas) {
    if(tas != NULL) {
        if(tas->data != NULL) free(tas->data);
        free(tas);
    }
}

void tas_ajout(tas_binaire_t * tas, int sommet, double score, double cout_reel) {
    if (tas->size >= tas->capacity) return; // si le tas est plein on abandonne, on ne devrait pas être confronté à ça comme capacity = nb arêtes
    
    // etape 1 : nouvel élément tout à la fin de l'arbre (à l'indice size)
    int i = tas->size;
    tas->data[i].sommet = sommet;
    tas->data[i].score = score;
    tas->data[i].cout_reel = cout_reel;
    tas->size++;
    
    // tape 2 : la remontée (binaire diminuer) : la parent est plus petit que ses enfants
    // donc on échange si pas le cas de l'élément qu'on vient d'ajouter
    while (i > 0) {
        int parent = (i - 1) / 2; // dans arbre binaire
        if (tas->data[i].score < tas->data[parent].score) {
            element_tas_t temp = tas->data[i];
            tas->data[i] = tas->data[parent];
            tas->data[parent] = temp;
            i = parent; // enfant nouveau point de départ
        } else {
            break; // si enfant plus grande que parent arbre valide donc stop boucle
        }
    }
}

element_tas_t tas_extraire_min(tas_binaire_t * tas) {
    if (tas->size <= 0) return (element_tas_t){-1, -1.0, -1.0}; // structure d'erreur si arbre vide
    
    element_tas_t racine = tas->data[0]; // comme min à la racine on peut le sauvegarder
    tas->size--; // on baisse la taille du tas
    tas->data[0] = tas->data[tas->size]; // on place le tout dernier élément à la racine de l'arbre pour boçucher le trou
    
    // etape 3 : descente : on replace la nouvelle racine (trop grande pour être à la racine) à sa bonne position en faisant des comparaisons avec les enfants et en des échanges si nécessaires
    int i = 0;
    while (1) {
        int gauche = 2 * i + 1;
        int droit = 2 * i + 2;
        int min = i;
        
        // si enfant gauche existe et est plus petit que le noeud actuel
        if (gauche < tas->size && tas->data[gauche].score < tas->data[min].score)
            min = gauche;
        if (droit < tas->size && tas->data[droit].score < tas->data[min].score)
            min = droit;
        // echange
        if (min != i) {
            element_tas_t temp = tas->data[i];
            tas->data[i] = tas->data[min];
            tas->data[min] = temp;
            i = min;
        } else {
            // cas où parent est plus petit que les enfants donc arbre équilibré donc stop la boucle
            break;
        }
    }
    return racine;
}
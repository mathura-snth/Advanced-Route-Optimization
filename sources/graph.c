#include <stdio.h>
#include <stdlib.h>
#include "graph.h"

csr_graph_t* load_graph(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Erreur : Impossible d'ouvrir le fichier %s\n", filename);
        return NULL;
    }

    int u, v;
    double w;
    int max_noeud_id = -1;
    int arete_count = 0;

    // etape 1 : lecture de tout le fichier une 1ère fois pour trouver id max = (N)
    // et compter le nombre d'arêtes total.
    while (fscanf(file, "%d,%d,%lf", &u, &v, &w) == 3) {
        if (u > max_noeud_id) {
            max_noeud_id = u;
        }
        if (v > max_noeud_id) {
            max_noeud_id = v;
        }
        arete_count += 2; // pour que Dijkstra aille dans les deux sens on considère le graphe comme non orienté donc bidirectionnel donc 
    }
    int nb_noeuds = max_noeud_id + 1;
    printf("-> %d noeuds et %d aretes trouves.\n", nb_noeuds, arete_count);

    // maintenant qu'on connait les tailles, on peut allouer de la mémoire pour la structure CSR
    csr_graph_t *graphe = malloc(sizeof(csr_graph_t));
    graphe->nb_noeuds = nb_noeuds;
    graphe->nb_aretes = arete_count;
    // calloc pour first_arete -> mettre tout initialement à 0
    graphe->first_arete = calloc(nb_noeuds + 1, sizeof(int));
    graphe->aretes = malloc(arete_count * sizeof(arete_t));

    // etape 2 : on revient au debut du fichier, pour compte le nombre de voisins par noeud (pour chaque (u,v) -> +1 pour u et pour v)
    rewind(file);
    while (fscanf(file, "%d,%d,%lf", &u, &v, &w) == 3) {
        graphe->first_arete[u]++;
        graphe->first_arete[v]++; 
    }

    // transformation en offsets (index)
    // si le noeud 0 a 3 voisins alors les voisins du noeud 1 commenceront à l'indice 3 du tableau
    int sum = 0;
    for (int i = 0; i <= nb_noeuds; i++) {
        int degree = graphe->first_arete[i];
        graphe->first_arete[i] = sum;
        // décalade de l'index pour le noeud suivant
        sum += degree;
    }

    // etape 3 : Remplir le tableau des aretes -> ranger les arêtes dans le bon ordre sans écraser nos repères
    // on crée une copie temporaire des index pour savoir où écrire
    int *current_offset = malloc((nb_noeuds + 1) * sizeof(int));
    for (int i = 0; i <= nb_noeuds; i++) {
        current_offset[i] = graphe->first_arete[i];
    }

    rewind(file);
    while (fscanf(file, "%d,%d,%lf", &u, &v, &w) == 3) {
        // Sens u -> v
        int index_u = current_offset[u]++;
        graphe->aretes[index_u].cible = v;
        graphe->aretes[index_u].poids = w;

        // Sens v -> u (bidirectionnel)
        int index_v = current_offset[v]++;
        graphe->aretes[index_v].cible = u;
        graphe->aretes[index_v].poids = w;
    }

    free(current_offset);
    fclose(file);
    printf("succès du chargement en mémoire\n");
    
    return graphe;
}

// test pour verifier que les donnees sont bien la
void afficher_infos_noeud(csr_graph_t *graphe, int id_noeud) {
    if (id_noeud >= graphe->nb_noeuds) return; // on verifie que noeud existe
    
    // indice de depart dans : first_arete[i], indice de fin dans : first_arete[i+1].
    int debut = graphe->first_arete[id_noeud];
    int fin = graphe->first_arete[id_noeud + 1];
    
    printf("\nLe noeud %d est relie a %d autres noeuds :\n", id_noeud, fin - debut);
    
    // on parcourt directement le tableau d'arêtes entre ces deux bornes -> O(1)
    for (int i = debut; i < fin; i++) {
        printf(" - Noeud %d (Distance: %.2f metres)\n", graphe->aretes[i].cible, graphe->aretes[i].poids);
    }
}

// fnction pour charger noeuds.csv
coordonnees_t* charger_coordonnees(const char *filename, int nb_noeuds) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Erreur : Impossible d'ouvrir %s\n", filename);
        return NULL;
    }

    coordonnees_t *coords = malloc(nb_noeuds * sizeof(coordonnees_t));
    int id;
    double lat, lon;

    // chargement des coordonnees
    while (fscanf(file, "%d,%lf,%lf", &id, &lat, &lon) == 3) {
        if (id < nb_noeuds) {
            coords[id].lat = lat;
            coords[id].lon = lon;
        }
    }
    fclose(file);
    return coords;
}

void free_graph(csr_graph_t *g) {
    if (g) {
        free(g->first_arete);
        free(g->aretes);
        free(g);
    }
}
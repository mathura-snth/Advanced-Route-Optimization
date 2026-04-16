#include <stdio.h>
#include <stdlib.h>

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
    int *first_edge; // tableau des offsets = index
    arete_t *edges;  // tableau de toutes les arêtes les unes à la suite des autres
} csr_graph_t;


csr_graph_t* load_graph(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Erreur : Impossible d'ouvrir le fichier %s\n", filename);
        return NULL;
    }

    int u, v;
    double w;
    int max_node_id = -1;
    int edge_count = 0;

    // etape 1 : lecture de tout le fichier une 1ère fois pour trouver id max = (N) et compter le nombre d'arêtes total.
    while (fscanf(file, "%d %d %lf", &u, &v, &w) == 3) {
        if (u > max_node_id) max_node_id = u;
        if (v > max_node_id) max_node_id = v;
        edge_count += 2; // pour Dijkstra aille dans les deux sens on considère le graphe comme non orienté donc bidirectionnel donc 
    }
    int nb_noeuds = max_node_id + 1;
    printf("-> %d noeuds et %d aretes trouves.\n", nb_noeuds, edge_count);

    // maintenant qu'on connait les tailles, on peut allouer de la mémoire pour la structure CSR
    csr_graph_t *graphe = malloc(sizeof(csr_graph_t));
    graphe->nb_noeuds = nb_noeuds;
    graphe->nb_aretes = edge_count;
    // calloc pour first_edge -> mettre tout initialement à 0
    graphe->first_edge = calloc(nb_noeuds + 1, sizeof(int));
    graphe->edges = malloc(edge_count * sizeof(arete_t));

    // etape 2 : on revient au debut du fichier, pour chaque (u,v, poids) on ajoute +1 au nb de voisins de u
    rewind(file);
    while (fscanf(file, "%d %d %lf", &u, &v, &w) == 3) {
        graphe->first_edge[u]++;
        graphe->first_edge[v]++; 
    }

    // transformation en offsets (index)
    // si le noeud 0 a 3 voisins alors les voisins du noeud 1 commenceront à l'indice 3 du tableau
    int sum = 0;
    for (int i = 0; i <= nb_noeuds; i++) {
        int degree = graphe->first_edge[i];
        graphe->first_edge[i] = sum;
        // décalade de l'index pour le noeud suivant
        sum += degree;
    }

    // etape 3 : Remplir le tableau des aretes -> ranger les arêtes dans le bon ordre sans écraser nos repères
    // on crée une copie temporaire des index pour savoir où écrire
    int *current_offset = malloc((nb_noeuds + 1) * sizeof(int));
    for (int i = 0; i <= nb_noeuds; i++) {
        current_offset[i] = graphe->first_edge[i];
    }

    rewind(file);
    while (fscanf(file, "%d %d %lf", &u, &v, &w) == 3) {
        // Sens u -> v
        int index_u = current_offset[u]++;
        graphe->edges[index_u].cible = v;
        graphe->edges[index_u].poids = w;

        // Sens v -> u (bidirectionnel)
        int index_v = current_offset[v]++;
        graphe->edges[index_v].cible = u;
        graphe->edges[index_v].poids = w;
    }

    free(current_offset);
    fclose(file);
    printf("succès du chargement en mémoire\n");
    
    return graphe;
}

// test pour verifier que les donnees sont bien la
void afficher_infos_noeud(csr_graph_t *graphe, int id_noeud) {
    if (id_noeud >= graphe->nb_noeuds) return; // on verifie que noeud existe
    
    // indice de depart dans : first_edge[i], indice de fin dans : first_edge[i+1].
    int debut = graphe->first_edge[id_noeud];
    int fin = graphe->first_edge[id_noeud + 1];
    
    printf("\nLe noeud %d est relie a %d autres noeuds :\n", id_noeud, fin - debut);
    
    // on parcourt directement le tableau d'arêtes entre ces deux bornes -> O(1)
    for (int i = debut; i < fin; i++) {
        printf(" - Noeud %d (Distance: %.2f metres)\n", graphe->edges[i].cible, graphe->edges[i].poids);
    }
}

int main() {
    csr_graph_t *graphe = load_graph("edges.txt");

    if (graphe) {
        // test affichage voisins du noeud d'id 0
        afficher_infos_noeud(graphe, 0);
        
        free(graphe->first_edge);
        free(graphe->edges);
        free(graphe);
    }

    return 0;
}
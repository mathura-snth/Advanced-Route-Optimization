#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int target;
    double weight;
} Edge;

typedef struct {
    int num_nodes;
    int num_edges;
    int *first_edge; // Tableau des offsets (taille : num_nodes + 1)
    Edge *edges;     // Tableau de toutes les arêtes (taille : num_edges)
} CSRGraph;

// --- FONCTIONS ---

CSRGraph* load_graph(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Erreur : Impossible d'ouvrir le fichier %s\n", filename);
        return NULL;
    }

    printf("1. Analyse du fichier pour compter les noeuds et les aretes...\n");
    int u, v;
    double w;
    int max_node_id = -1;
    int edge_count = 0;

    // Premier passage : trouver l'ID maximum pour connaitre le nombre de noeuds
    while (fscanf(file, "%d %d %lf", &u, &v, &w) == 3) {
        if (u > max_node_id) max_node_id = u;
        if (v > max_node_id) max_node_id = v;
        edge_count++;
    }

    int num_nodes = max_node_id + 1;
    printf("-> %d noeuds et %d aretes trouves.\n", num_nodes, edge_count);

    // Allocation de la structure CSR
    CSRGraph *graph = malloc(sizeof(CSRGraph));
    graph->num_nodes = num_nodes;
    graph->num_edges = edge_count;
    graph->first_edge = calloc(num_nodes + 1, sizeof(int));
    graph->edges = malloc(edge_count * sizeof(Edge));

    // Deuxieme passage : Compter le nombre d'aretes sortantes pour chaque noeud
    rewind(file);
    while (fscanf(file, "%d %d %lf", &u, &v, &w) == 3) {
        graph->first_edge[u]++;
    }

    // Calcul des offsets (Somme prefixe)
    printf("2. Construction de la structure CSR...\n");
    int sum = 0;
    for (int i = 0; i <= num_nodes; i++) {
        int degree = graph->first_edge[i];
        graph->first_edge[i] = sum;
        sum += degree;
    }

    // Troisieme passage : Remplir le tableau des aretes
    int *current_offset = malloc((num_nodes + 1) * sizeof(int));
    for (int i = 0; i <= num_nodes; i++) {
        current_offset[i] = graph->first_edge[i];
    }

    rewind(file);
    while (fscanf(file, "%d %d %lf", &u, &v, &w) == 3) {
        int index = current_offset[u];
        graph->edges[index].target = v;
        graph->edges[index].weight = w;
        current_offset[u]++;
    }

    free(current_offset);
    fclose(file);
    printf("-> Graphe charge en memoire avec succes !\n");
    
    return graph;
}

// Fonction de test pour verifier que les donnees sont bien la
void print_node_info(CSRGraph *graph, int node_id) {
    if (node_id >= graph->num_nodes) return;
    
    int start = graph->first_edge[node_id];
    int end = graph->first_edge[node_id + 1];
    
    printf("\nLe noeud %d est relie a %d autres noeuds :\n", node_id, end - start);
    for (int i = start; i < end; i++) {
        printf("  -> Noeud %d (Distance: %.2f metres)\n", graph->edges[i].target, graph->edges[i].weight);
    }
}

// --- MAIN ---

int main() {
    // On charge le fichier généré par le script Python
    CSRGraph *graph = load_graph("edges.txt");

    if (graph) {
        // Testons avec le tout premier noeud (ID 0)
        print_node_info(graph, 0);

        // Liberation propre de la memoire a la fin
        free(graph->first_edge);
        free(graph->edges);
        free(graph);
    }

    return 0;
}
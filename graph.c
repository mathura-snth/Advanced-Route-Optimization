#include <stdio.h>
#include <stdlib.h>

// on ne stocke pas le noeud de départ car CSR nous donne à quel noeud appartient l'arrête
typedef struct {
    int target;
    double weight;
} Edge;

// toutes les arêtes sont dans un énorme tableau contigu
typedef struct {
    int num_nodes;
    int num_edges;

    // tableau des offsets = index
    int *first_edge;

    //tableau de toutes les arêtes les unes à la suite des autres
    Edge *edges;
} CSRGraph;


CSRGraph* load_graph(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Erreur : Impossible d'ouvrir le fichier %s\n", filename);
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
        edge_count++;
    }

    int num_nodes = max_node_id + 1;
    printf("-> %d noeuds et %d aretes trouves.\n", num_nodes, edge_count);

    // maintenant qu'on connait les tailles, on peut malloc pour la structure CSR
    CSRGraph *graph = malloc(sizeof(CSRGraph));
    graph->num_nodes = num_nodes;
    graph->num_edges = edge_count;
    // calloc pour first_edge -> mettre tout initialement à 0
    graph->first_edge = calloc(num_nodes + 1, sizeof(int));
    graph->edges = malloc(edge_count * sizeof(Edge));


    // etape 2 : on revient au debut du fichier, pour chaque (u,v, poids) on ajoute +1 au nb de voisins de u
    rewind(file);
    while (fscanf(file, "%d %d %lf", &u, &v, &w) == 3) {
        graph->first_edge[u]++;
    }

    // transformation en offsets
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
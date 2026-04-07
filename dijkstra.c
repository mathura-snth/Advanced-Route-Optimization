#include <stdio.h>
#include <stdlib.h>
#include <float.h> // Pour DBL_MAX (l'infini)
#include <sys/time.h>

// ==========================================
// 1. STRUCTURES DE DONNÉES (Graphe CSR)
// ==========================================

typedef struct {
    int target;
    double weight;
} Edge;

typedef struct {
    int num_nodes;
    int num_edges;
    int *first_edge; 
    Edge *edges;     
} CSRGraph;

// ==========================================
// 2. CHARGEMENT DU GRAPHE (BIDIRECTIONNEL)
// ==========================================

CSRGraph* load_graph(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Erreur : Impossible d'ouvrir le fichier %s\n", filename);
        return NULL;
    }

    printf("1. Chargement du reseau routier (Bidirectionnel)...\n");
    int u, v;
    double w;
    int max_node_id = -1;
    int edge_count = 0;

    // Premier passage : trouver l'ID maximum et compter (double sens)
    while (fscanf(file, "%d %d %lf", &u, &v, &w) == 3) {
        if (u > max_node_id) max_node_id = u;
        if (v > max_node_id) max_node_id = v;
        edge_count += 2; // NOUVEAU: Graphe non-orienté
    }

    int num_nodes = max_node_id + 1;
    printf("-> %d noeuds et %d aretes trouves.\n", num_nodes, edge_count);

    // Allocation CSR
    CSRGraph *graph = malloc(sizeof(CSRGraph));
    graph->num_nodes = num_nodes;
    graph->num_edges = edge_count;
    graph->first_edge = calloc(num_nodes + 1, sizeof(int));
    graph->edges = malloc(edge_count * sizeof(Edge));

    // Deuxieme passage : degré sortant (dans les deux sens)
    rewind(file);
    while (fscanf(file, "%d %d %lf", &u, &v, &w) == 3) {
        graph->first_edge[u]++;
        graph->first_edge[v]++; // NOUVEAU
    }

    // Calcul des offsets
    printf("2. Construction de la structure CSR...\n");
    int sum = 0;
    for (int i = 0; i <= num_nodes; i++) {
        int degree = graph->first_edge[i];
        graph->first_edge[i] = sum;
        sum += degree;
    }

    // Troisieme passage : remplissage
    int *current_offset = malloc((num_nodes + 1) * sizeof(int));
    for (int i = 0; i <= num_nodes; i++) {
        current_offset[i] = graph->first_edge[i];
    }

    rewind(file);
    while (fscanf(file, "%d %d %lf", &u, &v, &w) == 3) {
        // Sens u -> v
        int index_u = current_offset[u]++;
        graph->edges[index_u].target = v;
        graph->edges[index_u].weight = w;

        // Sens v -> u
        int index_v = current_offset[v]++;
        graph->edges[index_v].target = u;
        graph->edges[index_v].weight = w;
    }

    free(current_offset);
    fclose(file);
    printf("-> Graphe charge en memoire avec succes !\n");
    
    return graph;
}

// ==========================================
// 3. FILE DE PRIORITÉ (TAS BINAIRE / MIN-HEAP)
// ==========================================

typedef struct {
    int node;
    double dist;
} HeapNode;

typedef struct {
    HeapNode *data;
    int size;
    int capacity;
} MinHeap;

MinHeap* create_heap(int capacity) {
    MinHeap *heap = malloc(sizeof(MinHeap));
    heap->capacity = capacity;
    heap->size = 0;
    heap->data = malloc(capacity * sizeof(HeapNode));
    return heap;
}

void swap(HeapNode *a, HeapNode *b) {
    HeapNode temp = *a;
    *a = *b;
    *b = temp;
}

void push(MinHeap *heap, int node, double dist) {
    if (heap->size == heap->capacity) return;
    int i = heap->size++;
    heap->data[i].node = node;
    heap->data[i].dist = dist;
    while (i != 0 && heap->data[(i - 1) / 2].dist > heap->data[i].dist) {
        swap(&heap->data[i], &heap->data[(i - 1) / 2]);
        i = (i - 1) / 2;
    }
}

HeapNode pop(MinHeap *heap) {
    if (heap->size <= 0) return (HeapNode){-1, -1.0};
    if (heap->size == 1) return heap->data[--heap->size];
    HeapNode root = heap->data[0];
    heap->data[0] = heap->data[--heap->size];
    int i = 0;
    while (1) {
        int left = 2 * i + 1;
        int right = 2 * i + 2;
        int smallest = i;
        if (left < heap->size && heap->data[left].dist < heap->data[smallest].dist)
            smallest = left;
        if (right < heap->size && heap->data[right].dist < heap->data[smallest].dist)
            smallest = right;
        if (smallest != i) {
            swap(&heap->data[i], &heap->data[smallest]);
            i = smallest;
        } else {
            break;
        }
    }
    return root;
}

void free_heap(MinHeap *heap) {
    free(heap->data);
    free(heap);
}

// ==========================================
// 4. ALGORITHME DE DIJKSTRA (BASELINE INSTRUMENTÉE)
// ==========================================

double get_time_in_seconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

void dijkstra(CSRGraph *graph, int start_node, int target_node) {
    printf("\n=== Lancement de Dijkstra (Baseline) ===\n");
    printf("Recherche de chemin : %d -> %d\n", start_node, target_node);

    double *dist = malloc(graph->num_nodes * sizeof(double));
    int *prev = malloc(graph->num_nodes * sizeof(int));

    for (int i = 0; i < graph->num_nodes; i++) {
        dist[i] = DBL_MAX; 
        prev[i] = -1;      
    }

    MinHeap *heap = create_heap(graph->num_edges); 
    dist[start_node] = 0.0;
    push(heap, start_node, 0.0);

    long long extractions = 0;
    long long relaxations = 0;
    
    double start_time = get_time_in_seconds();

    while (heap->size > 0) {
        HeapNode current = pop(heap);
        int u = current.node;

        if (current.dist > dist[u]) continue;

        extractions++;

        if (u == target_node) break;

        int start_edge = graph->first_edge[u];
        int end_edge = graph->first_edge[u + 1];

        for (int i = start_edge; i < end_edge; i++) {
            int v = graph->edges[i].target;
            double weight = graph->edges[i].weight;

            if (dist[u] + weight < dist[v]) {
                relaxations++; 
                dist[v] = dist[u] + weight;
                prev[v] = u;
                push(heap, v, dist[v]);
            }
        }
    }

    double end_time = get_time_in_seconds();
    double time_total = end_time - start_time;

    if (dist[target_node] == DBL_MAX) {
        printf("-> Aucun chemin trouve.\n");
    } else {
        printf("-> Succes !\n");
        printf("-> Distance totale : %.2f metres\n", dist[target_node]);
        printf("\n--- METRIQUES D'EVALUATION DIJKSTRA ---\n");
        printf("1. Nombre d'extractions : %lld\n", extractions);
        printf("2. Nombre de relaxations  : %lld\n", relaxations);
        printf("3. Temps total d'execution: %.6f secondes\n", time_total);
    }
    printf("========================================\n\n");

    free(dist);
    free(prev);
    free_heap(heap);
}

// ==========================================
// 5. MAIN
// ==========================================

int main() {
    // On charge le graphe en bidirectionnel
    CSRGraph *graph = load_graph("edges.txt");

    if (graph) {
        // Le test exact sur lequel tu as fait tourner A*
        int depart = 15;
        int arrivee = 1466593;

        dijkstra(graph, depart, arrivee);

        free(graph->first_edge);
        free(graph->edges);
        free(graph);
    }

    return 0;
}
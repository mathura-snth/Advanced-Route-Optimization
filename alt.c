#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <sys/time.h>
#include <math.h> // Pour la valeur absolue (fabs)

// ==========================================
// 1. STRUCTURES DU GRAPHE CSR
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
    if (!file) return NULL;

    printf("1. Chargement du reseau routier (Bidirectionnel)...\n");
    int u, v;
    double w;
    int max_node_id = -1;
    int edge_count = 0;

    while (fscanf(file, "%d %d %lf", &u, &v, &w) == 3) {
        if (u > max_node_id) max_node_id = u;
        if (v > max_node_id) max_node_id = v;
        edge_count += 2; // Graphe non-orienté
    }

    int num_nodes = max_node_id + 1;
    CSRGraph *graph = malloc(sizeof(CSRGraph));
    graph->num_nodes = num_nodes;
    graph->num_edges = edge_count;
    graph->first_edge = calloc(num_nodes + 1, sizeof(int));
    graph->edges = malloc(edge_count * sizeof(Edge));

    rewind(file);
    while (fscanf(file, "%d %d %lf", &u, &v, &w) == 3) {
        graph->first_edge[u]++;
        graph->first_edge[v]++;
    }

    int sum = 0;
    for (int i = 0; i <= num_nodes; i++) {
        int degree = graph->first_edge[i];
        graph->first_edge[i] = sum;
        sum += degree;
    }

    int *current_offset = malloc((num_nodes + 1) * sizeof(int));
    for (int i = 0; i <= num_nodes; i++) {
        current_offset[i] = graph->first_edge[i];
    }

    rewind(file);
    while (fscanf(file, "%d %d %lf", &u, &v, &w) == 3) {
        int index_u = current_offset[u]++;
        graph->edges[index_u].target = v;
        graph->edges[index_u].weight = w;

        int index_v = current_offset[v]++;
        graph->edges[index_v].target = u;
        graph->edges[index_v].weight = w;
    }

    free(current_offset);
    fclose(file);
    printf("-> Graphe charge : %d noeuds, %d aretes.\n", graph->num_nodes, graph->num_edges);
    return graph;
}

// ==========================================
// 3. FILE DE PRIORITÉ (Pour A* et Dijkstra)
// ==========================================

typedef struct {
    int node;
    double f; // Score total (tri du tas)
    double g; // Vraie distance
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
    HeapNode temp = *a; *a = *b; *b = temp;
}

void push(MinHeap *heap, int node, double f, double g) {
    if (heap->size == heap->capacity) return;
    int i = heap->size++;
    heap->data[i].node = node;
    heap->data[i].f = f;
    heap->data[i].g = g;
    while (i != 0 && heap->data[(i - 1) / 2].f > heap->data[i].f) {
        swap(&heap->data[i], &heap->data[(i - 1) / 2]);
        i = (i - 1) / 2;
    }
}

HeapNode pop(MinHeap *heap) {
    if (heap->size <= 0) return (HeapNode){-1, -1.0, -1.0};
    if (heap->size == 1) return heap->data[--heap->size];
    HeapNode root = heap->data[0];
    heap->data[0] = heap->data[--heap->size];
    int i = 0;
    while (1) {
        int left = 2 * i + 1;
        int right = 2 * i + 2;
        int smallest = i;
        if (left < heap->size && heap->data[left].f < heap->data[smallest].f)
            smallest = left;
        if (right < heap->size && heap->data[right].f < heap->data[smallest].f)
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
// 4. PRÉ-CALCUL DES LANDMARKS (ALT)
// ==========================================

double get_time_in_seconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

// Fait un Dijkstra classique depuis un Landmark vers tous les autres noeuds
double* dijkstra_for_landmark(CSRGraph *graph, int landmark_node) {
    double *dist = malloc(graph->num_nodes * sizeof(double));
    for (int i = 0; i < graph->num_nodes; i++) dist[i] = DBL_MAX;

    MinHeap *heap = create_heap(graph->num_edges);
    dist[landmark_node] = 0.0;
    push(heap, landmark_node, 0.0, 0.0);

    while (heap->size > 0) {
        HeapNode current = pop(heap);
        int u = current.node;

        if (current.g > dist[u]) continue;

        int start_edge = graph->first_edge[u];
        int end_edge = graph->first_edge[u + 1];

        for (int i = start_edge; i < end_edge; i++) {
            int v = graph->edges[i].target;
            double weight = graph->edges[i].weight;

            if (dist[u] + weight < dist[v]) {
                dist[v] = dist[u] + weight;
                push(heap, v, dist[v], dist[v]);
            }
        }
    }
    free_heap(heap);
    return dist; // Retourne le tableau contenant les distances vers toute la carte
}

// ==========================================
// 5. HEURISTIQUE ALT
// ==========================================

// Calcule l'inegalite triangulaire maximale
double alt_heuristic(int u, int target, int num_landmarks, double **landmark_dists) {
    double max_h = 0.0;
    for (int i = 0; i < num_landmarks; i++) {
        // Distance du noeud u au Landmark L
        double dist_u_L = landmark_dists[i][u];
        // Distance de la cible au Landmark L
        double dist_target_L = landmark_dists[i][target];

        // Si l'un des deux noeuds n'est pas connecte au Landmark, on l'ignore
        if (dist_u_L != DBL_MAX && dist_target_L != DBL_MAX) {
            double h = fabs(dist_u_L - dist_target_L); // Valeur absolue de la difference
            if (h > max_h) {
                max_h = h;
            }
        }
    }
    return max_h;
}

// ==========================================
// 6. ALGORITHME ALT (La Requête)
// ==========================================

void alt_search(CSRGraph *graph, int start_node, int target_node, int num_landmarks, double **landmark_dists) {
    printf("\n=== Lancement de la requete ALT ===\n");
    printf("Recherche : %d -> %d\n", start_node, target_node);

    double *dist = malloc(graph->num_nodes * sizeof(double));
    for (int i = 0; i < graph->num_nodes; i++) dist[i] = DBL_MAX;

    MinHeap *heap = create_heap(graph->num_edges);
    dist[start_node] = 0.0;
    
    // Heuristique ALT de depart
    double h_start = alt_heuristic(start_node, target_node, num_landmarks, landmark_dists);
    push(heap, start_node, h_start, 0.0);

    long long extractions = 0;
    long long relaxations = 0;
    double start_time = get_time_in_seconds();

    while (heap->size > 0) {
        HeapNode current = pop(heap);
        int u = current.node;

        if (current.g > dist[u]) continue;

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
                
                // Nouvelle Heuristique ALT
                double h = alt_heuristic(v, target_node, num_landmarks, landmark_dists);
                double f = dist[v] + h;
                
                push(heap, v, f, dist[v]);
            }
        }
    }

    double time_total = get_time_in_seconds() - start_time;

    if (dist[target_node] == DBL_MAX) {
        printf("-> Echec : Aucun chemin trouve.\n");
    } else {
        printf("-> Succes !\n");
        printf("-> Distance totale : %.2f metres\n", dist[target_node]);
        printf("\n--- METRIQUES D'EVALUATION ALT ---\n");
        printf("1. Extractions : %lld\n", extractions);
        printf("2. Relaxations : %lld\n", relaxations);
        printf("3. Temps total : %.6f secondes\n", time_total);
    }
    printf("========================================\n");

    free(dist);
    free_heap(heap);
}

// ==========================================
// 7. MAIN
// ==========================================

int main() {
    CSRGraph *graph = load_graph("edges.txt");
    if (!graph) return 1;

    // 1. CHOIX DES LANDMARKS
    int num_landmarks = 10;
    int *landmarks = malloc(num_landmarks * sizeof(int));
    
    // Choix aléatoire des Landmarks (Fixe la seed pour avoir toujours les memes)
    srand(42); 
    printf("\n2. Selection de %d Landmarks (Aleatoire)...\n", num_landmarks);
    for (int i = 0; i < num_landmarks; i++) {
        landmarks[i] = rand() % graph->num_nodes;
        printf("   -> Landmark %d : Noeud %d\n", i+1, landmarks[i]);
    }

    // 2. PHASE DE PRÉ-CALCUL
    printf("\n3. Lancement des pre-calculs (Ca peut prendre quelques secondes)...\n");
    double precalc_start = get_time_in_seconds();
    
    // Tableau de 10 pointeurs (un pour chaque Landmark) contenant les distances
    double **landmark_dists = malloc(num_landmarks * sizeof(double*));
    for (int i = 0; i < num_landmarks; i++) {
        landmark_dists[i] = dijkstra_for_landmark(graph, landmarks[i]);
    }
    
    printf("-> Pre-calculs termines en %.2f secondes.\n", get_time_in_seconds() - precalc_start);

    // 3. LA REQUÊTE ALT
    // Teste avec ton meme noeud de depart et d'arrivee que dans a_star.c
    int depart = 15;
    int arrivee = 1466593; 

    alt_search(graph, depart, arrivee, num_landmarks, landmark_dists);

    // 4. LIBÉRATION MÉMOIRE
    for (int i = 0; i < num_landmarks; i++) free(landmark_dists[i]);
    free(landmark_dists);
    free(landmarks);
    free(graph->first_edge);
    free(graph->edges);
    free(graph);

    return 0;
}
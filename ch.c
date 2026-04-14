#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <sys/time.h>

// ==========================================
// 1. STRUCTURES DU GRAPHE DYNAMIQUE
// ==========================================

typedef struct {
    int target;
    double weight;
} Edge;

typedef struct {
    Edge *edges;
    int size;
    int capacity;
} DynNode;

typedef struct {
    int num_nodes;
    DynNode *nodes;
} DynGraph;

// ==========================================
// 2. STRUCTURE CH-CSR (Uniquement le graphe UP)
// ==========================================

typedef struct {
    int num_nodes;
    int *rank; 
    int *up_first_edge;
    Edge *up_edges;
} CHGraph;

// ==========================================
// 3. FILE DE PRIORITÉ (TAS BINAIRE)
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
    HeapNode temp = *a; *a = *b; *b = temp;
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
        int left = 2 * i + 1; int right = 2 * i + 2; int smallest = i;
        if (left < heap->size && heap->data[left].dist < heap->data[smallest].dist) smallest = left;
        if (right < heap->size && heap->data[right].dist < heap->data[smallest].dist) smallest = right;
        if (smallest != i) {
            swap(&heap->data[i], &heap->data[smallest]);
            i = smallest;
        } else break;
    }
    return root;
}

void free_heap(MinHeap *heap) { free(heap->data); free(heap); }

double get_time_in_seconds() {
    struct timeval tv; gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

// ==========================================
// 4. CHARGEMENT DANS LE GRAPHE DYNAMIQUE
// ==========================================

void add_dyn_edge(DynGraph *graph, int u, int v, double w) {
    if (graph->nodes[u].size == graph->nodes[u].capacity) {
        graph->nodes[u].capacity = (graph->nodes[u].capacity == 0) ? 4 : graph->nodes[u].capacity * 2;
        graph->nodes[u].edges = realloc(graph->nodes[u].edges, graph->nodes[u].capacity * sizeof(Edge));
    }
    graph->nodes[u].edges[graph->nodes[u].size].target = v;
    graph->nodes[u].edges[graph->nodes[u].size].weight = w;
    graph->nodes[u].size++;
}

DynGraph* load_dyn_graph(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) return NULL;

    int u, v; double w; int max_node = -1;
    while (fscanf(file, "%d %d %lf", &u, &v, &w) == 3) {
        if (u > max_node) max_node = u;
        if (v > max_node) max_node = v;
    }

    DynGraph *graph = malloc(sizeof(DynGraph));
    graph->num_nodes = max_node + 1;
    graph->nodes = calloc(graph->num_nodes, sizeof(DynNode));

    rewind(file);
    while (fscanf(file, "%d %d %lf", &u, &v, &w) == 3) {
        add_dyn_edge(graph, u, v, w);
        add_dyn_edge(graph, v, u, w);
    }
    fclose(file);
    return graph;
}

// ==========================================
// 5. PHASE DE PRÉ-CALCUL (CONTRACTION)
// ==========================================

typedef struct { int node; int degree; } NodeDegree;
int compare_degree(const void *a, const void *b) {
    return ((NodeDegree*)a)->degree - ((NodeDegree*)b)->degree;
}

CHGraph* contract_and_build(DynGraph *dyn_graph) {
    printf("\n--- DEBUT DU PRE-CALCUL CH ---\n");
    int num_nodes = dyn_graph->num_nodes;
    CHGraph *ch = malloc(sizeof(CHGraph));
    ch->num_nodes = num_nodes;
    ch->rank = malloc(num_nodes * sizeof(int));

    printf("1. Classement des noeuds par importance...\n");
    NodeDegree *nd = malloc(num_nodes * sizeof(NodeDegree));
    for (int i = 0; i < num_nodes; i++) {
        nd[i].node = i;
        nd[i].degree = dyn_graph->nodes[i].size;
    }
    qsort(nd, num_nodes, sizeof(NodeDegree), compare_degree);
    for (int i = 0; i < num_nodes; i++) ch->rank[nd[i].node] = i;

    printf("2. Contraction et creation des raccourcis...\n");
    long long shortcuts_added = 0;
    int *order = malloc(num_nodes * sizeof(int));
    for (int i = 0; i < num_nodes; i++) order[i] = nd[i].node;
    free(nd);

    for (int i = 0; i < num_nodes; i++) {
        int u = order[i];
        
        int active_count = 0;
        int active_neighbors[200]; 
        double active_weights[200];

        // On cherche les voisins plus importants et on filtre les doublons
        for (int j = 0; j < dyn_graph->nodes[u].size; j++) {
            int v = dyn_graph->nodes[u].edges[j].target;
            double w = dyn_graph->nodes[u].edges[j].weight;
            
            if (ch->rank[v] > ch->rank[u]) {
                int found = -1;
                for(int k = 0; k < active_count; k++) {
                    if(active_neighbors[k] == v) { found = k; break; }
                }
                if(found != -1) {
                    if(w < active_weights[found]) active_weights[found] = w; // Garde le plus court
                } else if (active_count < 200) {
                    active_neighbors[active_count] = v;
                    active_weights[active_count] = w;
                    active_count++;
                }
            }
        }

        // Création garantie de tous les raccourcis nécessaires
        for (int a = 0; a < active_count; a++) {
            for (int b = a + 1; b < active_count; b++) {
                double total_w = active_weights[a] + active_weights[b];
                add_dyn_edge(dyn_graph, active_neighbors[a], active_neighbors[b], total_w);
                add_dyn_edge(dyn_graph, active_neighbors[b], active_neighbors[a], total_w);
                shortcuts_added += 2;
            }
        }
        
        if (i > 0 && i % 300000 == 0) printf("   -> %d / %d noeuds traites...\n", i, num_nodes);
    }
    printf("   -> %lld raccourcis virtuels ajoutes !\n", shortcuts_added);
    free(order);

    printf("3. Aplatissement dans la structure CSR (Up Graph uniquement)...\n");
    int *up_counts = calloc(num_nodes, sizeof(int));

    for (int u = 0; u < num_nodes; u++) {
        for (int i = 0; i < dyn_graph->nodes[u].size; i++) {
            int v = dyn_graph->nodes[u].edges[i].target;
            if (ch->rank[u] < ch->rank[v]) up_counts[u]++;
        }
    }

    ch->up_first_edge = calloc(num_nodes + 1, sizeof(int));
    int sum_up = 0;
    for (int i = 0; i <= num_nodes; i++) {
        ch->up_first_edge[i] = sum_up; 
        sum_up += up_counts[i];
    }

    ch->up_edges = malloc(sum_up * sizeof(Edge));
    int *cur_up = malloc((num_nodes + 1) * sizeof(int));
    for (int i = 0; i <= num_nodes; i++) cur_up[i] = ch->up_first_edge[i];

    for (int u = 0; u < num_nodes; u++) {
        for (int i = 0; i < dyn_graph->nodes[u].size; i++) {
            int v = dyn_graph->nodes[u].edges[i].target;
            double w = dyn_graph->nodes[u].edges[i].weight;
            if (ch->rank[u] < ch->rank[v]) {
                ch->up_edges[cur_up[u]].target = v;
                ch->up_edges[cur_up[u]].weight = w;
                cur_up[u]++;
            }
        }
    }

    free(cur_up); free(up_counts);
    printf("-> Pre-calcul termine ! Graphe CH-CSR pret.\n");
    return ch;
}

// ==========================================
// 6. LA REQUÊTE : DIJKSTRA BIDIRECTIONNEL CH
// ==========================================

void ch_search(CHGraph *ch, int start, int target) {
    printf("\n=== Lancement de la requete Contraction Hierarchies ===\n");
    printf("Trajet : %d -> %d\n", start, target);
    double start_time = get_time_in_seconds();

    double *dist_fwd = malloc(ch->num_nodes * sizeof(double));
    double *dist_bwd = malloc(ch->num_nodes * sizeof(double));
    for (int i = 0; i < ch->num_nodes; i++) {
        dist_fwd[i] = DBL_MAX;
        dist_bwd[i] = DBL_MAX;
    }

    MinHeap *heap_fwd = create_heap(500000);
    MinHeap *heap_bwd = create_heap(500000);

    dist_fwd[start] = 0.0; push(heap_fwd, start, 0.0);
    dist_bwd[target] = 0.0; push(heap_bwd, target, 0.0);

    double best_path = DBL_MAX;
    long long extractions = 0;

    while (heap_fwd->size > 0 || heap_bwd->size > 0) {
        double min_fwd = (heap_fwd->size > 0) ? heap_fwd->data[0].dist : DBL_MAX;
        double min_bwd = (heap_bwd->size > 0) ? heap_bwd->data[0].dist : DBL_MAX;
        
        // Arrêt si les deux recherches ont dépassé le meilleur chemin trouvé
        if (min_fwd >= best_path && min_bwd >= best_path) break;

        // --- Avancée Forward ---
        if (heap_fwd->size > 0) {
            HeapNode cur_fwd = pop(heap_fwd);
            int u = cur_fwd.node;
            if (cur_fwd.dist <= dist_fwd[u] && cur_fwd.dist <= best_path) {
                extractions++;
                // Check point de rencontre
                if (dist_bwd[u] != DBL_MAX && dist_fwd[u] + dist_bwd[u] < best_path) {
                    best_path = dist_fwd[u] + dist_bwd[u];
                }
                int start_edge = ch->up_first_edge[u], end_edge = ch->up_first_edge[u + 1];
                for (int i = start_edge; i < end_edge; i++) {
                    int v = ch->up_edges[i].target;
                    double w = ch->up_edges[i].weight;
                    if (dist_fwd[u] + w < dist_fwd[v]) {
                        dist_fwd[v] = dist_fwd[u] + w;
                        push(heap_fwd, v, dist_fwd[v]);
                    }
                }
            }
        }

        // --- Avancée Backward ---
        // Dans un graphe bidirectionnel, la recherche arrière monte AUSSI dans le graphe UP !
        if (heap_bwd->size > 0) {
            HeapNode cur_bwd = pop(heap_bwd);
            int u = cur_bwd.node;
            if (cur_bwd.dist <= dist_bwd[u] && cur_bwd.dist <= best_path) {
                extractions++;
                // Check point de rencontre
                if (dist_fwd[u] != DBL_MAX && dist_fwd[u] + dist_bwd[u] < best_path) {
                    best_path = dist_fwd[u] + dist_bwd[u];
                }
                int start_edge = ch->up_first_edge[u], end_edge = ch->up_first_edge[u + 1];
                for (int i = start_edge; i < end_edge; i++) {
                    int v = ch->up_edges[i].target;
                    double w = ch->up_edges[i].weight;
                    if (dist_bwd[u] + w < dist_bwd[v]) {
                        dist_bwd[v] = dist_bwd[u] + w;
                        push(heap_bwd, v, dist_bwd[v]);
                    }
                }
            }
        }
    }

    double total_time = get_time_in_seconds() - start_time;

    if (best_path == DBL_MAX) {
        printf("-> Echec : Aucun chemin trouve.\n");
    } else {
        printf("-> Succes !\n");
        printf("-> Distance totale (CH) : %.2f metres\n", best_path);
        printf("\n--- METRIQUES D'EVALUATION CH ---\n");
        printf("1. Extractions Bidirectionnelles : %lld\n", extractions);
        printf("2. Temps total d'execution       : %.6f secondes\n", total_time);
    }
    printf("========================================================\n\n");

    free(dist_fwd); free(dist_bwd);
    free_heap(heap_fwd); free_heap(heap_bwd);
}

// ==========================================
// 7. MAIN
// ==========================================

int main() {
    printf("1. Chargement de edges.txt dans le graphe dynamique...\n");
    DynGraph *dyn = load_dyn_graph("edges.txt");
    if (!dyn) return 1;

    CHGraph *ch = contract_and_build(dyn);

    for (int i = 0; i < dyn->num_nodes; i++) free(dyn->nodes[i].edges);
    free(dyn->nodes); free(dyn);

    int depart = 15;
    int arrivee = 1466593;

    ch_search(ch, depart, arrivee);

    free(ch->rank);
    free(ch->up_first_edge); free(ch->up_edges);
    free(ch);

    return 0;
}
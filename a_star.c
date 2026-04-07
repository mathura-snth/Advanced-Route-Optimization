#include <stdio.h>
#include <stdlib.h>
#include <float.h> 
#include <sys/time.h>
#include <math.h> // Indispensable pour la formule de Haversine (sin, cos, sqrt, atan2)

// ======================================================================
// 1. STRUCTURES DE DONNÉES DE BASE (Communes avec Dijkstra)
// ======================================================================

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

// ======================================================================
// 2. COORDONNÉES ET HEURISTIQUE (Spécifique à A*)
// ======================================================================

typedef struct {
    double lat;
    double lon;
} NodeCoord;

// Fonction pour charger nodes.txt
NodeCoord* load_coords(const char *filename, int num_nodes) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Erreur : Impossible d'ouvrir %s\n", filename);
        return NULL;
    }

    NodeCoord *coords = malloc(num_nodes * sizeof(NodeCoord));
    int id;
    double lat, lon;

    printf("Chargement des coordonnees GPS...\n");
    while (fscanf(file, "%d %lf %lf", &id, &lat, &lon) == 3) {
        if (id < num_nodes) {
            coords[id].lat = lat;
            coords[id].lon = lon;
        }
    }
    
    fclose(file);
    return coords;
}

// Fonction Haversine : Calcule la distance à vol d'oiseau entre deux points GPS
// Cette distance sert d'heuristique admissible h(v) pour A*
double haversine(double lat1, double lon1, double lat2, double lon2) {
    double R = 6371000.0; // Rayon de la Terre en mètres
    double dLat = (lat2 - lat1) * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    lat1 = lat1 * M_PI / 180.0;
    lat2 = lat2 * M_PI / 180.0;

    double a = sin(dLat/2) * sin(dLat/2) +
               cos(lat1) * cos(lat2) * sin(dLon/2) * sin(dLon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return R * c;
}

// ======================================================================
// 3. CHARGEMENT DU GRAPHE (Bidirectionnel)
// ======================================================================

CSRGraph* load_graph(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) return NULL;

    printf("Chargement du reseau routier (Bidirectionnel)...\n");
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
    return graph;
}

// ======================================================================
// 4. FILE DE PRIORITÉ (Adaptée pour A*)
// ======================================================================

// Pour A*, le tas doit connaître f (le score total estimé) pour se trier
// et g (la vraie distance parcourue) pour la vérification Lazy.
typedef struct {
    int node;
    double f; // f(v) = g(v) + h(v)
    double g; // g(v) = distance parcourue depuis le départ
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
    // On remonte dans le tas en comparant la valeur f
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

// ======================================================================
// 5. CHRONOMÉTRAGE
// ======================================================================

double get_time_in_seconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

// ======================================================================
// 6. ALGORITHME A*
// ======================================================================

void a_star(CSRGraph *graph, NodeCoord *coords, int start_node, int target_node) {
    printf("\n=== Lancement de A* ===\n");
    printf("Recherche du plus court chemin : %d -> %d\n", start_node, target_node);

    double *dist = malloc(graph->num_nodes * sizeof(double));
    int *prev = malloc(graph->num_nodes * sizeof(int));

    for (int i = 0; i < graph->num_nodes; i++) {
        dist[i] = DBL_MAX; 
        prev[i] = -1;      
    }

    MinHeap *heap = create_heap(graph->num_edges); 
    dist[start_node] = 0.0;
    
    // Calcul de l'heuristique initiale h(start)
    double h_start = haversine(coords[start_node].lat, coords[start_node].lon, 
                               coords[target_node].lat, coords[target_node].lon);
    
    // On insère le noeud de départ. f = h_start, g = 0.0
    push(heap, start_node, h_start, 0.0);

    // Variables d'instrumentation requises par le cahier des charges
    long long extractions = 0;
    long long relaxations = 0;
    double start_time = get_time_in_seconds();

    while (heap->size > 0) {
        HeapNode current = pop(heap);
        int u = current.node;

        // Approche Lazy : on utilise current.g pour vérifier si le chemin est obsolète
        if (current.g > dist[u]) continue;

        extractions++;
        if (u == target_node) break; // Arrêt précoce essentiel pour les performances

        int start_edge = graph->first_edge[u];
        int end_edge = graph->first_edge[u + 1];

        for (int i = start_edge; i < end_edge; i++) {
            int v = graph->edges[i].target;
            double weight = graph->edges[i].weight;

            // Si on trouve un meilleur chemin pour atteindre v
            if (dist[u] + weight < dist[v]) {
                relaxations++; 
                dist[v] = dist[u] + weight;
                prev[v] = u;
                
                // Calcul de l'heuristique h(v)
                double h = haversine(coords[v].lat, coords[v].lon, 
                                     coords[target_node].lat, coords[target_node].lon);
                
                // f(v) = g(v) + h(v)
                double f = dist[v] + h;
                
                // On insère dans le tas avec f pour le tri, et g pour la vérification future
                push(heap, v, f, dist[v]);
            }
        }
    }

    double time_total = get_time_in_seconds() - start_time;

    // Affichage formaté pour les métriques
    if (dist[target_node] == DBL_MAX) {
        printf("-> Echec : Aucun chemin trouve.\n");
    } else {
        printf("-> Succes !\n");
        printf("-> Distance totale (g) : %.2f metres\n", dist[target_node]);
        printf("\n--- METRIQUES D'EVALUATION A* ---\n");
        printf("1. Nombre d'extractions (noeuds visites) : %lld\n", extractions);
        printf("2. Nombre de relaxations                 : %lld\n", relaxations);
        printf("3. Temps total d'execution               : %.6f secondes\n", time_total);
    }
    printf("========================================\n\n");

    free(dist);
    free(prev);
    free_heap(heap);
}

// ======================================================================
// 7. PROGRAMME PRINCIPAL (Test)
// ======================================================================

int main() {
    // 1. Charger le graphe et les coordonnées
    CSRGraph *graph = load_graph("edges.txt");
    if (!graph) return 1;

    NodeCoord *coords = load_coords("nodes.txt", graph->num_nodes);
    if (!coords) {
        free(graph->first_edge);
        free(graph->edges);
        free(graph);
        return 1;
    }

    // 2. Lancer la requête A* // Rappel: change l'arrivee pour le noeud lointain que tu as trouvé avec Dijkstra
    int depart = 15;
    int arrivee = 1466593; 

    a_star(graph, coords, depart, arrivee);

    // 3. Libérer la mémoire
    free(coords);
    free(graph->first_edge);
    free(graph->edges);
    free(graph);

    return 0;
}
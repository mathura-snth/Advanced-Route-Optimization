#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <time.h>  // Pour clock_gettime (remplace sys/time.h)
#include <math.h>  // Pour la valeur absolue (fabs)

// structures comme a_star
typedef struct {
    int cible;
    double poids;
} arete_t;

typedef struct {
    int nb_noeuds;
    int nb_aretes;
    int *first_edge; 
    arete_t *edges;  
} csr_graph_t;


// chargement comme a_star

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

    while (fscanf(file, "%d %d %lf", &u, &v, &w) == 3) {
        if (u > max_node_id) max_node_id = u;
        if (v > max_node_id) max_node_id = v;
        edge_count += 2; // bidirectionnel
    }
    int nb_noeuds = max_node_id + 1;
    printf("-> %d noeuds et %d aretes trouves.\n", nb_noeuds, edge_count);

    csr_graph_t *graphe = malloc(sizeof(csr_graph_t));
    graphe->nb_noeuds = nb_noeuds;
    graphe->nb_aretes = edge_count;
    graphe->first_edge = calloc(nb_noeuds + 1, sizeof(int));
    graphe->edges = malloc(edge_count * sizeof(arete_t));

    rewind(file);
    while (fscanf(file, "%d %d %lf", &u, &v, &w) == 3) {
        graphe->first_edge[u]++;
        graphe->first_edge[v]++; 
    }

    int sum = 0;
    for (int i = 0; i <= nb_noeuds; i++) {
        int degree = graphe->first_edge[i];
        graphe->first_edge[i] = sum;
        sum += degree;
    }

    int *current_offset = malloc((nb_noeuds + 1) * sizeof(int));
    for (int i = 0; i <= nb_noeuds; i++) {
        current_offset[i] = graphe->first_edge[i];
    }

    rewind(file);
    while (fscanf(file, "%d %d %lf", &u, &v, &w) == 3) {
        int index_u = current_offset[u]++;
        graphe->edges[index_u].cible = v;
        graphe->edges[index_u].poids = w;

        int index_v = current_offset[v]++;
        graphe->edges[index_v].cible = u;
        graphe->edges[index_v].poids = w;
    }

    free(current_offset);
    fclose(file);
    printf("succès du chargement du graphe CSR en mémoire\n");
    
    return graphe;
}


// même file de priorité mais différente heuristique

// on utilise, comme a_star, le score global f pour trier et la distance réelle g pour la lazy deletion
typedef struct {
    int sommet;
    double f; // f(v) = g(v) + h(v) (Clé de tri)
    double g; // Vraie distance parcourue
} element_tas_t;

typedef struct {
    element_tas_t *data;
    int size;
    int capacity;
} tas_binaire_t;

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

void tas_ajout(tas_binaire_t * tas, int sommet, double f, double g) {
    if (tas->size >= tas->capacity) return;
    
    int i = tas->size;
    tas->data[i].sommet = sommet;
    tas->data[i].f = f;
    tas->data[i].g = g;
    tas->size++;
    
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (tas->data[i].f < tas->data[parent].f) {
            element_tas_t temp = tas->data[i];
            tas->data[i] = tas->data[parent];
            tas->data[parent] = temp;
            i = parent;
        } else {
            break;
        }
    }
}

element_tas_t tas_extraire_min(tas_binaire_t * tas) {
    if (tas->size <= 0) return (element_tas_t){-1, -1.0, -1.0};
    
    element_tas_t racine = tas->data[0];
    tas->size--;
    tas->data[0] = tas->data[tas->size]; 
    
    int i = 0;
    while (1) {
        int gauche = 2 * i + 1;
        int droit = 2 * i + 2;
        int min = i;
        
        if (gauche < tas->size && tas->data[gauche].f < tas->data[min].f)
            min = gauche;
        if (droit < tas->size && tas->data[droit].f < tas->data[min].f)
            min = droit;
            
        if (min != i) {
            element_tas_t temp = tas->data[i];
            tas->data[i] = tas->data[min];
            tas->data[min] = temp;
            i = min;
        } else {
            break;
        }
    }
    return racine;
}

//précalcul spécifique à alt

// avant de calculer l'heuristique d'alt, on doit connaitre la distance exacte entre chaque landmarks (points repères) et tous les autresz noeuds du graphe.
// on fait pour ça, dijkstra sur chaque landmark
double* dijkstra_pour_landmark(csr_graph_t *graphe, int landmark) {
    double *distances = malloc(graphe->nb_noeuds * sizeof(double));
    for (int i = 0; i < graphe->nb_noeuds; i++) distances[i] = DBL_MAX;

    tas_binaire_t *tas = tas_create(graphe->nb_aretes);
    distances[landmark] = 0.0;
    
    // dijkstra sans destination ni heurisitque
    tas_ajout(tas, landmark, 0.0, 0.0);

    while (tas->size > 0) {
        element_tas_t courant = tas_extraire_min(tas);
        int u = courant.sommet;

        if (courant.g > distances[u]) continue;

        int debut_aretes = graphe->first_edge[u];
        int fin_aretes = graphe->first_edge[u + 1];

        for (int i = debut_aretes; i < fin_aretes; i++) {
            int v = graphe->edges[i].cible;
            double poids = graphe->edges[i].poids;

            if (distances[u] + poids < distances[v]) {
                distances[v] = distances[u] + poids;
                tas_ajout(tas, v, distances[v], distances[v]);
            }
        }
    }
    tas_destroy(tas);
    
    // on retourne un tableau qui contient les distances du landmark vers tout le reste
    return distances; 
}

// heuristique alt : inégalité triangulaire
// dist(U,V) >= |dist(U,L) - dist(V,L)| => donne heuristique sans avoir besoin des coordonnées
double heuristique_alt(int u, int arrivee, int nb_landmarks, double **distances_landmarks) {
    double max_h = 0.0;
    
    for (int i = 0; i < nb_landmarks; i++) {
        double dist_u_L = distances_landmarks[i][u];
        double dist_arrivee_L = distances_landmarks[i][arrivee];

        // on applique ineg triangulaire si les deux noeuds atteignent landmark
        if (dist_u_L != DBL_MAX && dist_arrivee_L != DBL_MAX) {
            double h = fabs(dist_u_L - dist_arrivee_L);
            
            // val max des landmarks pour augmenter précision
            if (h > max_h) {
                max_h = h;
            }
        }
    }
    return max_h;
}

void alt(csr_graph_t *graphe, int depart, int arrivee, int nb_landmarks, double **distances_landmarks) {
    printf("\nRecherche ALT de %d vers %d\n", depart, arrivee);

    double *distances = malloc(graphe->nb_noeuds * sizeof(double));
    int *predecesseurs = malloc(graphe->nb_noeuds * sizeof(int));

    for (int i = 0; i < graphe->nb_noeuds; i++) {
        distances[i] = DBL_MAX; 
        predecesseurs[i] = -1;      
    }

    struct timespec before, after;
    clockid_t clk_id = CLOCK_REALTIME;
    clock_gettime(clk_id, &before);

    tas_binaire_t * tas = tas_create(graphe->nb_aretes); 
    distances[depart] = 0.0;
    
    // heuristique initiale
    double h_depart = heuristique_alt(depart, arrivee, nb_landmarks, distances_landmarks);
    tas_ajout(tas, depart, h_depart, 0.0);

    long long nb_extractions = 0;
    long long nb_relaxations = 0;

    while (tas->size > 0) {
        element_tas_t courant = tas_extraire_min(tas);
        int u = courant.sommet;

        if (courant.g > distances[u]) continue;

        nb_extractions++;

        // early exit
        if (u == arrivee) break; 

        int debut_aretes = graphe->first_edge[u];
        int fin_aretes = graphe->first_edge[u + 1];

        for (int i = debut_aretes; i < fin_aretes; i++) {
            int v = graphe->edges[i].cible;
            double poids = graphe->edges[i].poids;

            if (distances[u] + poids < distances[v]) {
                nb_relaxations++; 
                distances[v] = distances[u] + poids;
                predecesseurs[v] = u;
                
                // nouvelle heurisitique basée sur landmarks
                double h = heuristique_alt(v, arrivee, nb_landmarks, distances_landmarks);
                
                // nouveua score
                double f = distances[v] + h;
                
                tas_ajout(tas, v, f, distances[v]);
            }
        }
    }

    clock_gettime(clk_id, &after);
    double temps_sec = (after.tv_sec - before.tv_sec) + (after.tv_nsec - before.tv_nsec) / 1e9;

    if (distances[arrivee] == DBL_MAX) {
        fprintf(stderr, "Erreur : Aucun chemin trouvé.\n");
    } else {
        printf("Succes !\n");
        printf("- Distance totale (g) : %.2f metres\n", distances[arrivee]);
        printf("- Extractions (Noeuds visites) : %lld\n", nb_extractions);
        printf("- Relaxations                  : %lld\n", nb_relaxations);
        printf("- Temps d'execution            : %lf secondes\n", temps_sec);
    }

    free(distances);
    free(predecesseurs);
    tas_destroy(tas);
}

int main() {
    csr_graph_t *graphe = load_graph("edges.txt");
    if (!graphe) return EXIT_FAILURE;

    // on definit le nombre de landmarks
    int nb_landmarks = 10;
    int *landmarks = malloc(nb_landmarks * sizeof(int));
    
    // On fixe la seed => reproductibilité
    srand(42); 
    printf("\nSelection de %d Landmarks \n", nb_landmarks);
    for (int i = 0; i < nb_landmarks; i++) {
        landmarks[i] = rand() % graphe->nb_noeuds;
        printf("   -> Landmark %d : Noeud %d\n", i+1, landmarks[i]);
    }
    printf("\nLancement des pre-calculs\n");
    
    struct timespec pre_before, pre_after;
    clock_gettime(CLOCK_REALTIME, &pre_before);
    
    // tableau de distances pour chaque landmarks
    double **distances_landmarks = malloc(nb_landmarks * sizeof(double*));
    for (int i = 0; i < nb_landmarks; i++) {
        distances_landmarks[i] = dijkstra_pour_landmark(graphe, landmarks[i]);
    }
    
    clock_gettime(CLOCK_REALTIME, &pre_after);
    double temps_precalc = (pre_after.tv_sec - pre_before.tv_sec) + (pre_after.tv_nsec - pre_before.tv_nsec) / 1e9;
    printf("-> Pre-calculs termines en %.2f secondes.\n", temps_precalc);

    // requete alt
    int depart = 15;
    int arrivee = 1466593; 

    alt(graphe, depart, arrivee, nb_landmarks, distances_landmarks);

    for (int i = 0; i < nb_landmarks; i++) {
        free(distances_landmarks[i]);
    }
    free(distances_landmarks);
    free(landmarks);
    
    free(graphe->first_edge);
    free(graphe->edges);
    free(graphe);

    return EXIT_SUCCESS;
}
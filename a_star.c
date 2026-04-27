#include <stdio.h>
#include <stdlib.h>
#include <float.h> 
#include <time.h> 
#include <math.h>  // pour la formule de Haversine (sin, cos, sqrt, atan2)
#include "algos.h"
#include "tas.h"
#include "graph.h"

// comme dijkstra :

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


// coordonnées + heuristique :
// Pour A* on doit connaitre les coordonnées pour calculer la distance à vol d'oiseau :
typedef struct {
    double lat;
    double lon;
} coordonnees_t;


// Fonction pour charger nodes.txt
coordonnees_t* charger_coordonnees(const char *filename, int nb_noeuds) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Erreur : Impossible d'ouvrir %s\n", filename);
        return NULL;
    }

    coordonnees_t *coords = malloc(nb_noeuds * sizeof(coordonnees_t));
    int id;
    double lat, lon;

    printf("Chargement des coordonnees\n");
    while (fscanf(file, "%d %lf %lf", &id, &lat, &lon) == 3) {
        if (id < nb_noeuds) {
            coords[id].lat = lat;
            coords[id].lon = lon;
        }
    }
    fclose(file);
    return coords;
}

// Fonction Haversine : Calcule la distance à vol d'oiseau entre deux points elle sera h(v) pour A*
double haversine(double lat1, double lon1, double lat2, double lon2) {
    double R = 6371000.0; // Rayon moyen de la Terre en mètres
    double dLat = (lat2 - lat1) * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    lat1 = lat1 * M_PI / 180.0;
    lat2 = lat2 * M_PI / 180.0;

    double a = sin(dLat/2) * sin(dLat/2) +
               cos(lat1) * cos(lat2) * sin(dLon/2) * sin(dLon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return R * c;
}

// comme dijkstra :

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
        edge_count += 2;
    }
    int nb_noeuds = max_node_id + 1;
    printf("-> %d noeuds et %d aretes trouves.\n", nb_noeuds, edge_count);

    // maintenant qu'on connait les tailles, on peut allouer de la mémoire pour la structure CSR
    csr_graph_t *graphe = malloc(sizeof(csr_graph_t));
    graphe->nb_noeuds = nb_noeuds;
    graphe->nb_aretes = edge_count;
    graphe->first_edge = calloc(nb_noeuds + 1, sizeof(int));
    graphe->edges = malloc(edge_count * sizeof(arete_t));

    // etape 2 : on revient au debut du fichier, pour chaque (u,v, poids) on ajoute +1 au nb de voisins de u
    rewind(file);
    while (fscanf(file, "%d %d %lf", &u, &v, &w) == 3) {
        graphe->first_edge[u]++;
        graphe->first_edge[v]++; 
    }

    // transformation en offsets (index)
    int sum = 0;
    for (int i = 0; i <= nb_noeuds; i++) {
        int degree = graphe->first_edge[i];
        graphe->first_edge[i] = sum;
        sum += degree;
    }

    // etape 3 : Remplir le tableau des aretes -> ranger les arêtes dans le bon ordre sans écraser nos repères
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
    printf("succès du chargement du graphe CSR en mémoire\n");
    
    return graphe;
}

// File de priorité

// différente pour A* : on doit ici connaitre le score global f pour trier les noeuds et on doit garder la vraie distance g pour le lazy deletion

typedef struct {
    int sommet;
    double f; // f(v) = g(v) + h(v) : c'est ça qui servira de clé de tri
    double g; // g(v) = vraie distance parcourue depuis le départ
} element_tas_t;

typedef struct {
    element_tas_t *data;// tableau dynamique contenant les paires (sommet, dist)
    int size; // nb d'éléments actuellement dans le tas
    int capacity; // taille max tableau
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
    if (tas->size >= tas->capacity) return; // si le tas est plein on abandonne, on ne devrait pas être confronté à ça comme capacity = nb arêtes

    // etape 1 : nouvel élément tout à la fin de l'arbre (à l'indice size)
    int i = tas->size;
    tas->data[i].sommet = sommet;
    tas->data[i].f = f;
    tas->data[i].g = g;
    tas->size++;
    
    // etape 2 : la remontée  : le parent est plus petit que ses enfants
    // donc on échange si pas le cas de l'élément qu'on vient d'ajouter
    // MAIS cette fois on se base du le score heuristique f et non que la distance
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (tas->data[i].f < tas->data[parent].f) {
            element_tas_t temp = tas->data[i];
            tas->data[i] = tas->data[parent];
            tas->data[parent] = temp;
            i = parent;
        } else {
            break; // si enfant plus grande que parent arbre valide donc stop boucle
        }
    }
}

element_tas_t tas_extraire_min(tas_binaire_t * tas) {
    if (tas->size <= 0) return (element_tas_t){-1, -1.0, -1.0}; // structure d'erreur si arbre vide
    
    element_tas_t racine = tas->data[0];
    tas->size--; // on baisse la taille du tas
    tas->data[0] = tas->data[tas->size];  // on place le tout dernier élément à la racine de l'arbre pour boçucher le trou
    
    // etape 3 : descente : on replace la nouvelle racine (trop grande pour être à la racine) à sa bonne position en faisant des comparaisons avec les enfants et en des échanges si nécessaires
    // MAIS cette fois basée sur score f et non juste la distance
    int i = 0;
    while (1) {
        int gauche = 2 * i + 1;
        int droit = 2 * i + 2;
        int min = i;
        
        if (gauche < tas->size && tas->data[gauche].f < tas->data[min].f)
            min = gauche;
        if (droit < tas->size && tas->data[droit].f < tas->data[min].f)
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

// Algo A* :

void algo_a_star(csr_graph_t *graphe, coordonnees_t *coords, int depart, int arrivee) {
    printf("\nRecherche A* de %d vers %d...\n", depart, arrivee);

    // comme dijkstra, on garde vraie distance parcourue g :
    // on fait deux tableaux :
    // 1 pour garder en mémoire le plus court chemin trouvé jusqu'à présent pour chaque noeud
    // 2 permet de retracer le chemin à l'envers une fois arrivé
    double *distances = malloc(graphe->nb_noeuds * sizeof(double));
    int *predecesseurs = malloc(graphe->nb_noeuds * sizeof(int));

    for (int i = 0; i < graphe->nb_noeuds; i++) {
        distances[i] = DBL_MAX; 
        predecesseurs[i] = -1;      
    }
    // temps de l'algo
    struct timespec before, after;
    clockid_t clk_id = CLOCK_REALTIME;
    clock_gettime(clk_id, &before);

    tas_binaire_t * tas = tas_create(graphe->nb_aretes); 
    distances[depart] = 0.0;
    
    // Calcul de l'heuristique initiale h : départ
    double h_depart = haversine(coords[depart].lat, coords[depart].lon, 
                                coords[arrivee].lat, coords[arrivee].lon);
    
    // on insère le noeud de départ, f = h_depart et distance réelle g = 0
    tas_ajout(tas, depart, h_depart, 0.0);

    // performance de l'algo
    long long nb_extractions = 0;
    long long nb_relaxations = 0;

    while (tas->size > 0) {

        element_tas_t courant = tas_extraire_min(tas); // on extrait le noeud le plus proche du point de départ
        int u = courant.sommet;

        // Modification de lazy deletion pour A* :
        // on compare g sauvegardée dans le tas avec la meilleure distance conneu dans le tableau.
        if (courant.g > distances[u]) continue;

        nb_extractions++;

        // Modification du early exit pour A* :
        // Comme h ne surestime JAMAIS, quand la destination sort du tas, on sait qu'on a trouvé le plus court chemin
        if (u == arrivee) break; 

        // recupération des voisins de u (en O(1) grace CSR)
        int debut_aretes = graphe->first_edge[u]; // indice de dep
        int fin_aretes = graphe->first_edge[u + 1]; // indice de fin

        // parcourt arêtes sortantes du sommet u
        for (int i = debut_aretes; i < fin_aretes; i++) {
            int v = graphe->edges[i].cible;
            double poids = graphe->edges[i].poids; // cout de l'arete entre u et v


            // relaxation : si on passe par u, est ce que le chemin pour atteindre v est plus court que l'ancienne distance qu'on connaissait pr v ?
            if (distances[u] + poids < distances[v]) { // oui
                nb_relaxations++; 
                distances[v] = distances[u] + poids;
                predecesseurs[v] = u;
                
                // calcul de la nouvelle heuristique h(v) pour le voisin
                double h = haversine(coords[v].lat, coords[v].lon, 
                                     coords[arrivee].lat, coords[arrivee].lon);
                
                // nouveau score f(v) = vraie distance g + estimation h
                double f = distances[v] + h;
                
                // on insère dans le tas le score f pour trier et g pour lazy deletion
                tas_ajout(tas, v, f, distances[v]);
            }
        }
    }
    // fin de chrono
    clock_gettime(clk_id, &after);
    double temps_sec = (after.tv_sec - before.tv_sec) + (after.tv_nsec - before.tv_nsec) / 1e9;
    // si toujjours infini alors que tas vidé, alors les 2 points ne sont pas connectés dans le graphe
    if (distances[arrivee] == DBL_MAX) {
        fprintf(stderr, "Erreur : Aucun chemin trouvé.\n");
    } else {
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

    coordonnees_t *coords = charger_coordonnees("nodes.txt", graphe->nb_noeuds);
    if (!coords) {
        free(graphe->first_edge);
        free(graphe->edges);
        free(graphe);
        return EXIT_FAILURE;
    }

    int depart = 15;
    int arrivee = 1466593; 

    algo_a_star(graphe, coords, depart, arrivee);

    free(coords);
    free(graphe->first_edge);
    free(graphe->edges);
    free(graphe);

    return EXIT_SUCCESS;
}
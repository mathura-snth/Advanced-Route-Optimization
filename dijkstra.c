#include <stdio.h>
#include <stdlib.h>
#include <float.h> // Pour DBL_MAX (l'infini)
#include <time.h>  // Pour clock_gettime

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
    arete_t *edges;  //tableau de toutes les arêtes les unes à la suite des autres
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
        // chaque arête est lue une fois mais comptée dreux fois car on devra la stocker pour l'aller et le retour
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

// structure du tas binaire : on stocke la paire (sommet, distance) pour savoir qui extraire
// tas binaire : file de priorité
// dans dijkstra, le tas binaire doit trier les nombres mais aussi savoir a quel somemt appartient la distance pour l'extraire
typedef struct {
    int sommet; // num du noeud dans graphe
    double distance; // clé de tri = distance cumulée depuis le départ
} element_tas_t;

typedef struct {
    element_tas_t *data; // tableau dynamique contenant les paires (sommet, dist)
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

void tas_ajout(tas_binaire_t * tas, int sommet, double dist) {
    if (tas->size >= tas->capacity) return; // si le tas est plein on abandonne, on ne devrait pas être confronté à ça comme capacity = nb arêtes
    
    // etape 1 : nouvel élément tout à la fin de l'arbre (à l'indice size)
    int i = tas->size;
    tas->data[i].sommet = sommet;
    tas->data[i].distance = dist;
    tas->size++;
    
    // tape 2 : la remontée (binaire diminuer) : la parent est plus petit que ses enfants
    // donc on échange si pas le cas de l'élément qu'on vient d'ajouter
    while (i > 0) {
        int parent = (i - 1) / 2; // dans arbre binaire
        if (tas->data[i].distance < tas->data[parent].distance) {
            element_tas_t temp = tas->data[i];
            tas->data[i] = tas->data[parent];
            tas->data[parent] = temp;
            i = parent; // enfant nouveau point de départ
        } else {
            break; // si enfant plus grande que parent arbre valide donc stop boucle
        }
    }
}

element_tas_t tas_extraire_min(tas_binaire_t * tas) {
    if (tas->size <= 0) return (element_tas_t){-1, -1.0}; // structure d'erreur si arbre vide
    
    element_tas_t racine = tas->data[0]; // comme min à la racine on peut le sauvegarder
    tas->size--; // on baisse la taille du tas
    tas->data[0] = tas->data[tas->size]; // on place le tout dernier élément à la racine de l'arbre pour boçucher le trou
    
    // etape 3 : descente : on replace la nouvelle racine (trop grande pour être à la racine) à sa bonne position en faisant des comparaisons avec les enfants et en des échanges si nécessaires
    int i = 0;
    while (1) {
        int gauche = 2 * i + 1;
        int droit = 2 * i + 2;
        int min = i;
        
        // si enfant gauche existe et est plus petit que le noeud actuel
        if (gauche < tas->size && tas->data[gauche].distance < tas->data[min].distance)
            min = gauche;
        if (droit < tas->size && tas->data[droit].distance < tas->data[min].distance)
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

void dijkstra(csr_graph_t *graphe, int depart, int arrivee) {
    printf("\nRecherche de %d vers %d\n", depart, arrivee);

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


    // DÉCISION DE CONCEPTION CRITIQUE : PLUTÔT QUE DE MODIFIER LES DISTANCES DANS LE TAS (CE QUI EST LENT ET COMPLEXE), 
    // ON PRÉFÈRE L'APPROCHE "LAZY DELETION" : ON AJOUTERA DES DOUBLONS.
    // LE PIRE CAS POSSIBLE EST QUE CHAQUE ARÊTE DU GRAPHE GÉNIÈRE UNE INSERTION. 
    // LA CAPACITÉ DU TAS EST DONC FIXÉE À nb_aretes POUR ÉVITER TOUT RISQUE DE DÉBORDEMENT.
    tas_binaire_t * tas = tas_create(graphe->nb_aretes);

    distances[depart] = 0.0;
    tas_ajout(tas, depart, 0.0);

    // performance de l'algo
    long long nb_extractions = 0;
    long long nb_relaxations = 0;

    while (tas->size > 0) {

        element_tas_t courant = tas_extraire_min(tas); // on extrait le noeud le plus proche du point de départ
        int u = courant.sommet;


        // C'EST ICI QU'OPÈRE LA MAGIE DE LA "LAZY DELETION" :
        // PUISQU'ON INSÈRE DES DOUBLONS PLUTÔT QUE DE METTRE À JOUR LE TAS, 
        // ON PEUT DÉPILER UN SOMMET DONT LA DISTANCE EST OBSOLÈTE (PLUS GRANDE QUE LA MEILLEURE TROUVÉE ENTRE TEMPS).
        // SI C'EST LE CAS, ON L'IGNORE ET ON PASSE AU SUIVANT DIRECTEMENT. ÇA ÉVITE DES CALCULS INUTILES.
        if (courant.distance > distances[u]) continue; 
        
        // noeud validé et exploré => incrémentation du compteur
        nb_extractions++;

        // early exit -> propriété du dijkstra : si on réussit à extraire le noeud de destination
        // c'est qu'on a trouvé le plus court chemin définitif pour l'atteindre => stop recherche peu importe le nb de noeuds restants
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
                nb_relaxations++; // on a trouvé raccourci
                distances[v] = distances[u] + poids; // maj nouvelle distance dans tableau résultat
                predecesseurs[v] = u; // memo qu'on est passé par u pour aller vers v
                tas_ajout(tas, v, distances[v]); // ajout de la paire dans tas binaire
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
        printf("Succes !\n");
        printf("- Distance trouvee : %.2f\n", distances[arrivee]);
        printf("- Extractions      : %lld\n", nb_extractions);
        printf("- Relaxations      : %lld\n", nb_relaxations);
        printf("- Temps d'execution: %lf secondes\n", temps_sec);
    }

    free(distances);
    free(predecesseurs);
    tas_destroy(tas);
}

int main() {
    csr_graph_t *graphe = load_graph("edges.txt");

    if (graphe) {
        int depart = 15;
        int arrivee = 1466593;

        dijkstra(graphe, depart, arrivee);

        free(graphe->first_edge);
        free(graphe->edges);
        free(graphe);
    }

    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <time.h>
#include "algos.h"
#include "tas.h"
#include "graph.h"

// structure dynamique pour le graphe pendant la phase de contraction
typedef struct edge_node {
    int cible;
    double poids;
    struct edge_node *next;
} edge_node_t;

// pour éviter de créer des arêtes en double
void add_edge_if_better(edge_node_t **adj, int u, int v, double w) {
    for (edge_node_t *e = adj[u]; e != NULL; e = e->next) {
        if (e->cible == v) {
            if (w < e->poids) e->poids = w; // Garde le plus court si doublon
            return;
        }
    }
    edge_node_t *new_e = malloc(sizeof(edge_node_t));
    new_e->cible = v;
    new_e->poids = w;
    new_e->next = adj[u];
    adj[u] = new_e;
}

// contraction d'un noeud
// retourne l'Edge Difference = (Raccourcis créés) - (Arêtes supprimées)
int process_node(int v, edge_node_t **adj, ch_graph_t *ch, double *dist, int *visited, tas_binaire_t *tas, int real_contraction) {
    int num_neighbors = 0;
    int max_neighbors = 2000; 
    int *neighbors = malloc(max_neighbors * sizeof(int));
    double *weights = malloc(max_neighbors * sizeof(double));

    int edges_removed = 0;

    // Collecter les voisins NON contractés
    for (edge_node_t *e = adj[v]; e != NULL; e = e->next) {
        if (ch->rank[e->cible] == ch->num_nodes) { 
            if (num_neighbors < max_neighbors) {
                neighbors[num_neighbors] = e->cible;
                weights[num_neighbors] = e->poids;
                num_neighbors++;
                edges_removed++;
            }
        }
    }

    int shortcuts_added = 0;

    for (int i = 0; i < num_neighbors; i++) {
        for (int j = i + 1; j < num_neighbors; j++) {
            int u = neighbors[i];
            int w = neighbors[j];
            double cout_via_v = weights[i] + weights[j];

            // Recherche Témoin u -> w
            int num_visited = 0;
            dist[u] = 0.0;
            visited[num_visited++] = u;
            tas_ajout(tas, u, 0.0, 0.0);
            double cout_sans_v = DBL_MAX;

            while (tas->size > 0) {
                element_tas_t curr = tas_extraire_min(tas);
                int curr_node = curr.sommet;

                if (curr.cout_reel > dist[curr_node]) continue;
                if (curr_node == w) {
                    cout_sans_v = dist[w];
                    break;
                }
                // Si la distance devient plus grande que le cout via V, ça sert à rien de continuer
                if (curr.cout_reel > cout_via_v) break; 

                for (edge_node_t *e = adj[curr_node]; e != NULL; e = e->next) {
                    int neighbor = e->cible;
                    if (neighbor == v) continue; // v est le noeud interdit
                    if (ch->rank[neighbor] != ch->num_nodes) continue; // Uniquement non contractés
                    
                    if (dist[neighbor] == DBL_MAX) {
                        visited[num_visited++] = neighbor;
                    }
                    if (dist[curr_node] + e->poids < dist[neighbor]) {
                        dist[neighbor] = dist[curr_node] + e->poids;
                        tas_ajout(tas, neighbor, dist[neighbor], dist[neighbor]);
                    }
                }
            }
            
            // nettoyage tas et distances
            while(tas->size > 0) tas_extraire_min(tas);
            for(int k = 0; k < num_visited; k++) dist[visited[k]] = DBL_MAX;

            if (cout_via_v < cout_sans_v) {
                shortcuts_added++;
                if (real_contraction) {
                    add_edge_if_better(adj, u, w, cout_via_v);
                    add_edge_if_better(adj, w, u, cout_via_v); // Bidirectionnel
                }
            }
        }
    }
    free(neighbors);
    free(weights);
    return shortcuts_added - edges_removed;
}


// PRE-TRAITEMENT (CONTRACTION HIERARCHIES)
ch_graph_t* pretraitement_ch(csr_graph_t *graphe) {
    int N = graphe->nb_noeuds;
    ch_graph_t *ch = malloc(sizeof(ch_graph_t));
    ch->num_nodes = N;
    ch->rank = malloc(N * sizeof(int));
    
    // Initialisation : tous les noeuds à N (infini = non contracté)
    for (int i = 0; i < N; i++) ch->rank[i] = N; 
    
    edge_node_t **adj = calloc(N, sizeof(edge_node_t*));
    for (int u = 0; u < N; u++) {
        for (int i = graphe->first_edge[u]; i < graphe->first_edge[u+1]; i++) {
            add_edge_if_better(adj, u, graphe->edges[i].cible, graphe->edges[i].poids);
        }
    }
    
    double *dist = malloc(N * sizeof(double));
    for (int i = 0; i < N; i++) dist[i] = DBL_MAX;
    int *visited = malloc(N * sizeof(int));
    tas_binaire_t *tas_ws = tas_create(graphe->nb_aretes + N); 

    // STRATEGIE DE CONTRACTION PARESSEUSE (EDGE DIFFERENCE)
    printf("[CH] Calcul de l'Edge Difference initiale...\n");
    double *current_ed = malloc(N * sizeof(double));
    tas_binaire_t *tas_ed = tas_create(N * 10); // Capacité large pour la lazy deletion

    int pas_affichage = (N > 100) ? (N / 100) : 1;
    
    for (int v = 0; v < N; v++) {
        int ed = process_node(v, adj, ch, dist, visited, tas_ws, 0); // Simulate=0
        current_ed[v] = ed;
        tas_ajout(tas_ed, v, ed, ed); // Le score est utilisé pour trier
        
        if (v % pas_affichage == 0) {
            printf("\r[CH] Initialisation : %3d%%", (v * 100) / N);
            fflush(stdout);
        }
    }
    printf("\n");

    printf("[CH] Debut de la contraction (Heuristique Edge Difference)...\n");
    int current_rank = 0;
    
    while (tas_ed->size > 0 && current_rank < N) {
        element_tas_t curr = tas_extraire_min(tas_ed);
        int v = curr.sommet;

        if (ch->rank[v] != N) continue; // Déjà contracté
        if (curr.score > current_ed[v]) continue; // Doublon obsolète (Lazy Update)

        // Ré-évaluation du noeud (car ses voisins ont peut-être été contractés)
        int new_ed = process_node(v, adj, ch, dist, visited, tas_ws, 0);
        int contract_now = 0;

        if (tas_ed->size == 0) {
            contract_now = 1;
        } else {
            // Trouver le vrai prochain noeud valide dans le tas
            element_tas_t next;
            int found_next = 0;
            while (tas_ed->size > 0) {
                next = tas_extraire_min(tas_ed);
                if (ch->rank[next.sommet] != N) continue;
                if (next.score > current_ed[next.sommet]) continue;
                found_next = 1;
                break;
            }

            if (!found_next) {
                contract_now = 1;
            } else {
                // Si notre nouveau score est toujours <= au suivant, on contracte
                if (new_ed <= next.score) {
                    contract_now = 1;
                    tas_ajout(tas_ed, next.sommet, next.score, next.score); // Remet le suivant
                } else {
                    // Sinon, on met à jour et on reporte la contraction (paresse)
                    tas_ajout(tas_ed, next.sommet, next.score, next.score); 
                    tas_ajout(tas_ed, v, new_ed, new_ed); 
                    current_ed[v] = new_ed;
                }
            }
        }

        if (contract_now) {
            process_node(v, adj, ch, dist, visited, tas_ws, 1); // 1 = Vraie contraction avec création raccourcis
            ch->rank[v] = current_rank++;
            
            if (current_rank % pas_affichage == 0) {
                printf("\r[CH] Progression : %3d%% (%8d / %8d noeuds)", (current_rank * 100) / N, current_rank, N);
                fflush(stdout);
            }
        }
    }
    printf("\n");
    
    // Libération des outils de contraction
    free(dist); free(visited); free(current_ed);
    tas_destroy(tas_ws); tas_destroy(tas_ed);
    
    // CREATION DU GRAPHE ASCENDANT (Identique à avant)
    int up_edges_count = 0;
    for (int u = 0; u < N; u++) {
        for (edge_node_t *e = adj[u]; e != NULL; e = e->next) {
            if (ch->rank[u] < ch->rank[e->cible]) up_edges_count++;
        }
    }
    
    ch->up_first_edge = calloc(N + 1, sizeof(int));
    ch->up_edges = malloc(up_edges_count * sizeof(arete_t));
    
    int current_idx = 0;
    for (int u = 0; u < N; u++) {
        ch->up_first_edge[u] = current_idx;
        for (edge_node_t *e = adj[u]; e != NULL; e = e->next) {
            if (ch->rank[u] < ch->rank[e->cible]) {
                ch->up_edges[current_idx].cible = e->cible;
                ch->up_edges[current_idx].poids = e->poids;
                current_idx++;
            }
        }
    }
    ch->up_first_edge[N] = current_idx;
    
    // Libération graphe temporaire
    for (int u = 0; u < N; u++) {
        edge_node_t *curr = adj[u];
        while (curr != NULL) {
            edge_node_t *next = curr->next;
            free(curr);
            curr = next;
        }
    }
    free(adj);
    
    return ch;
}

// --- PHASE 2 : RECHERCHE (DIJKSTRA BIDIRECTIONNEL) ---
// Reste strictement identique, la logique de recherche sur graphe ascendant fonctionne parfaitement
resultat_t ch_search(ch_graph_t *ch, int start, int end) {
    int N = ch->num_nodes;
    
    double *dist_aller = malloc(N * sizeof(double));
    double *dist_retour = malloc(N * sizeof(double));
    for (int i = 0; i < N; i++) {
        dist_aller[i] = DBL_MAX;
        dist_retour[i] = DBL_MAX;
    }
    
    tas_binaire_t *tas_aller = tas_create(ch->up_first_edge[N] + 1);
    tas_binaire_t *tas_retour = tas_create(ch->up_first_edge[N] + 1);
    
    dist_aller[start] = 0.0;
    tas_ajout(tas_aller, start, 0.0, 0.0);
    
    dist_retour[end] = 0.0;
    tas_ajout(tas_retour, end, 0.0, 0.0);
    
    double best_path = DBL_MAX;
    long long nb_extractions = 0;
    long long nb_relaxations = 0;
    
    struct timespec before, after;
    clock_gettime(CLOCK_REALTIME, &before);
    
    while (tas_aller->size > 0 || tas_retour->size > 0) {
        double min_aller = tas_aller->size > 0 ? tas_aller->data[0].cout_reel : DBL_MAX;
        double min_retour = tas_retour->size > 0 ? tas_retour->data[0].cout_reel : DBL_MAX;
        
        if (min_aller >= best_path && min_retour >= best_path) break; 
        
        if (tas_aller->size > 0 && (tas_retour->size == 0 || min_aller <= min_retour)) {
            element_tas_t curr = tas_extraire_min(tas_aller);
            int u = curr.sommet;
            
            if (curr.cout_reel > dist_aller[u]) continue; 
            nb_extractions++;
            
            if (dist_aller[u] != DBL_MAX && dist_retour[u] != DBL_MAX) {
                if (dist_aller[u] + dist_retour[u] < best_path) best_path = dist_aller[u] + dist_retour[u];
            }
            
            for (int i = ch->up_first_edge[u]; i < ch->up_first_edge[u+1]; i++) {
                int v = ch->up_edges[i].cible;
                double w = ch->up_edges[i].poids;
                if (dist_aller[u] + w < dist_aller[v]) {
                    dist_aller[v] = dist_aller[u] + w;
                    tas_ajout(tas_aller, v, dist_aller[v], dist_aller[v]);
                    nb_relaxations++;
                }
            }
        } else {
            element_tas_t curr = tas_extraire_min(tas_retour);
            int u = curr.sommet;
            
            if (curr.cout_reel > dist_retour[u]) continue; 
            nb_extractions++;
            
            if (dist_aller[u] != DBL_MAX && dist_retour[u] != DBL_MAX) {
                if (dist_aller[u] + dist_retour[u] < best_path) best_path = dist_aller[u] + dist_retour[u];
            }
            
            for (int i = ch->up_first_edge[u]; i < ch->up_first_edge[u+1]; i++) {
                int v = ch->up_edges[i].cible;
                double w = ch->up_edges[i].poids;
                if (dist_retour[u] + w < dist_retour[v]) {
                    dist_retour[v] = dist_retour[u] + w;
                    tas_ajout(tas_retour, v, dist_retour[v], dist_retour[v]);
                    nb_relaxations++;
                }
            }
        }
    }
    
    clock_gettime(CLOCK_REALTIME, &after);
    double temps_sec = (after.tv_sec - before.tv_sec) + (after.tv_nsec - before.tv_nsec) / 1e9;
    
    resultat_t res = {best_path, nb_extractions, nb_relaxations, temps_sec};
    
    free(dist_aller); free(dist_retour);
    tas_destroy(tas_aller); tas_destroy(tas_retour);
    
    return res;
}

void free_ch(ch_graph_t *ch) {
    if (ch) {
        free(ch->rank);
        free(ch->up_first_edge);
        free(ch->up_edges);
        free(ch);
    }
}
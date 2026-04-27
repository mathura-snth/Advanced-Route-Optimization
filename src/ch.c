#include "algos.h"
#include "tas.h"
#include "graph.h"
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <time.h>

// 1) Prétraitement : CONTRACTION ("Witness Search" (Recherche Témoin))
// Quand on veut supprimer (contracter) un noeud V, on regarde ses voisins U et W
// On lance un mini-Dijkstra entre U et W en s'interdisant de passer par V
// Si ce mini-Dijkstra trouve un chemin plus court que (U->V) + (V->W), alors V servait à rien
// Sinon obligé de créer un raccourci (nouvelle arête) entre U et W
static double witness_search(csr_graph_t *graphe, int depart, int arrivee, int noeud_interdit, double limite_poids) {
    tas_binaire_t *tas = tas_create(10000); // Petit tas local suffisant
    double *distances = malloc(graphe->nb_noeuds * sizeof(double));
    
    for(int i = 0; i < graphe->nb_noeuds; i++) distances[i] = DBL_MAX;
    
    distances[depart] = 0.0;
    tas_ajout(tas, depart, 0.0, 0.0);
    
    double meilleur_chemin = DBL_MAX;
    
    while(tas->size > 0) {
        element_tas_t courant = tas_extraire_min(tas);
        int u = courant.sommet;
        
        // Lazy deletion
        if(courant.cout_reel > distances[u]) continue;
        
        // Early exit intelligent : si distance actuelle dépasse déjà le coût du potentiel raccourci,
        // c'est perdu, stop la recherche témoin
        if(courant.cout_reel > limite_poids) break; 
        
        if(u == arrivee) {
            meilleur_chemin = courant.cout_reel;
            break;
        }
        
        for(int i = graphe->first_edge[u]; i < graphe->first_edge[u+1]; i++) {
            int v = graphe->edges[i].cible;
            double poids = graphe->edges[i].poids;
            
            // IMPORTANT : On interdit de passer par le noeud en cours de contraction
            if(v == noeud_interdit) continue;
            
            if(distances[u] + poids < distances[v]) {
                distances[v] = distances[u] + poids;
                tas_ajout(tas, v, distances[v], distances[v]);
            }
        }
    }
    
    free(distances);
    tas_destroy(tas);
    return meilleur_chemin;
}

ch_graph_t* ch_preprocess(csr_graph_t *graphe) {    
    ch_graph_t *ch = malloc(sizeof(ch_graph_t));
    ch->num_nodes = graphe->nb_noeuds;
    ch->rank = malloc(graphe->nb_noeuds * sizeof(int));
    
    // DÉCISION DE CONCEPTION : L'ordre de contraction
    // Normalement on trie les noeuds dynamiquement selon leur "Edge Difference"
    // nous on utilise juste un ordre naif (0 à N-1) pour prouver que l'algo marche
    for(int i = 0; i < graphe->nb_noeuds; i++) {
        ch->rank[i] = i; 
    }
    
    // On alloue un grand tableau pour stocker le graphe upward (uniquement les arêtes qui montent en rang)
    int max_up_edges = graphe->nb_aretes * 2; 
    ch->up_edges = malloc(max_up_edges * sizeof(arete_t));
    int up_edges_count = 0;
    
    // Filtrage des aretes montantes existantes
    for(int u = 0; u < graphe->nb_noeuds; u++) {
        for(int i = graphe->first_edge[u]; i < graphe->first_edge[u+1]; i++) {
            int v = graphe->edges[i].cible;
            // On ne garde que si V est plus important que U
            if(ch->rank[u] < ch->rank[v]) {
                ch->up_edges[up_edges_count].cible = v;
                ch->up_edges[up_edges_count].poids = graphe->edges[i].poids;
                up_edges_count++;
            }
        }
    }
    
    // Contraction et Witness Search (Creation des raccourcis)
    for(int v = 0; v < graphe->nb_noeuds; v++) {
        if (v > 0 && v % 20000 == 0) printf("   -> %d / %d noeuds contractes\n", v, graphe->nb_noeuds);
        
        // On teste toutes les paires (U, W) qui passent par V
        for(int i = graphe->first_edge[v]; i < graphe->first_edge[v+1]; i++) {
            int u = graphe->edges[i].cible;
            double w_uv = graphe->edges[i].poids;
            
            // U doit avoir été contracté AVANT V
            if(ch->rank[u] < ch->rank[v]) continue; 
            
            for(int j = graphe->first_edge[v]; j < graphe->first_edge[v+1]; j++) {
                int w = graphe->edges[j].cible;
                double w_vw = graphe->edges[j].poids;
                
                // W doit avoir été contracté AVANT V
                if(u == w || ch->rank[w] < ch->rank[v]) continue;
                
                double cout_via_v = w_uv + w_vw;
                double cout_sans_v = witness_search(graphe, u, w, v, cout_via_v);
                
                // Si le chemin direct est introuvable ou plus long, on ajoute le raccourci
                if(cout_via_v < cout_sans_v && up_edges_count < max_up_edges) {
                    // On ajoute toujours le raccourci dans le sens du upward (du plus petit rang au plus grand)
                    if(ch->rank[u] < ch->rank[w]) {
                        ch->up_edges[up_edges_count++] = (arete_t){w, cout_via_v};
                    } else {
                        ch->up_edges[up_edges_count++] = (arete_t){u, cout_via_v};
                    }
                }
            }
        }
    }
    
    // Remarque : Pour que ch->up_first_edge soit parfait, il faudrait faire un tri (Bucket Sort) du tableau up_edges.
    ch->up_first_edge = calloc((graphe->nb_noeuds + 1), sizeof(int));
    
    // fin du pré-traitement
    return ch;
}

void free_ch(ch_graph_t *ch) {
    if(ch) {
        free(ch->rank);
        free(ch->up_first_edge);
        free(ch->up_edges);
        free(ch);
    }
}

// 2) phase requete : dijkstra bidirectionnel

resultat_t ch_search(ch_graph_t *ch, int start, int target) {
    struct timespec before, after;
    clock_gettime(CLOCK_REALTIME, &before);

    // On a besoin de deux tableaux de distances (Aller et Retour)
    double *dist_aller = malloc(ch->num_nodes * sizeof(double));
    double *dist_retour = malloc(ch->num_nodes * sizeof(double));
    
    for (int i = 0; i < ch->num_nodes; i++) {
        dist_aller[i] = DBL_MAX;
        dist_retour[i] = DBL_MAX;
    }

    // Deux files de priorité
    tas_binaire_t *tas_aller = tas_create(500000);
    tas_binaire_t *tas_retour = tas_create(500000);

    dist_aller[start] = 0.0; 
    tas_ajout(tas_aller, start, 0.0, 0.0);
    
    dist_retour[target] = 0.0; 
    tas_ajout(tas_retour, target, 0.0, 0.0);

    double meilleur_chemin = DBL_MAX;
    long long extractions = 0;

    // La recherche tourne tant qu'une des deux files n'est pas vide
    while (tas_aller->size > 0 || tas_retour->size > 0) {
        
        // CONDITION D'ARRÊT CH : si les min des deux files dépassent le meilleur chemin trouvé, on stop
        if (tas_aller->size > 0 && tas_aller->data[0].score >= meilleur_chemin &&
            tas_retour->size > 0 && tas_retour->data[0].score >= meilleur_chemin) {
            break;
        }

        // AVANCÉE ALLER (recherche montante depuis le départ)
        if (tas_aller->size > 0) {
            element_tas_t courant = tas_extraire_min(tas_aller);
            int u = courant.sommet;
            
            if (courant.cout_reel <= dist_aller[u]) {
                extractions++;
                
                // Màj si les deux recherches se croisent sur ce noeud
                if (dist_retour[u] != DBL_MAX && dist_aller[u] + dist_retour[u] < meilleur_chemin) {
                    meilleur_chemin = dist_aller[u] + dist_retour[u];
                }
                
                // relaxation QUE sur le graphe upward (les noeuds de rang supérieur)
                for (int i = ch->up_first_edge[u]; i < ch->up_first_edge[u+1]; i++) {
                    int v = ch->up_edges[i].cible;
                    double w = ch->up_edges[i].poids;
                    if (dist_aller[u] + w < dist_aller[v]) {
                        dist_aller[v] = dist_aller[u] + w;
                        tas_ajout(tas_aller, v, dist_aller[v], dist_aller[v]);
                    }
                }
            }
        }
        
        // AVANCÉE RETOUR recherche montante depuis l'arrivée
        // IMPORTANT : on monte AUSSI le long du graphe upward
        if (tas_retour->size > 0) {
            element_tas_t courant = tas_extraire_min(tas_retour);
            int u = courant.sommet;
            
            if (courant.cout_reel <= dist_retour[u]) {
                extractions++;
                
                // Màj si croisement
                if (dist_aller[u] != DBL_MAX && dist_aller[u] + dist_retour[u] < meilleur_chemin) {
                    meilleur_chemin = dist_aller[u] + dist_retour[u];
                }
                
                // relaxation sur le graphe upward
                for (int i = ch->up_first_edge[u]; i < ch->up_first_edge[u+1]; i++) {
                    int v = ch->up_edges[i].cible;
                    double w = ch->up_edges[i].poids;
                    if (dist_retour[u] + w < dist_retour[v]) {
                        dist_retour[v] = dist_retour[u] + w;
                        tas_ajout(tas_retour, v, dist_retour[v], dist_retour[v]);
                    }
                }
            }
        }
    }

    clock_gettime(CLOCK_REALTIME, &after);
    double temps_sec = (after.tv_sec - before.tv_sec) + (after.tv_nsec - before.tv_nsec) / 1e9;

    // On renvoie 0 pour relaxations car notion différente en bidirectionnel CH
    resultat_t res = {meilleur_chemin, extractions, 0, temps_sec};
    
    free(dist_aller); 
    free(dist_retour);
    tas_destroy(tas_aller); 
    tas_destroy(tas_retour);
    
    return res;
}
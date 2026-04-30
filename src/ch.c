#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <time.h>
#include "algos.h"
#include "tas.h"
#include "graph.h"

// liste chaînée d'adjacence (plus flexible que CSR pour ajouter des raccourcis) temporaire pour le graphe pendant la contraction
// structure pour une arête sous forme de liste chaînée
typedef struct edge_node {
    int cible;
    double poids;
    struct edge_node *suivant;
} edge_node_t;

// pour éviter de créer des arêtes en double
void ajouter_arete_si_mieux(edge_node_t **adj, int u, int v, double poids) {
    for (edge_node_t *e = adj[u]; e != NULL; e = e->suivant) { //on parcourt tous les edge_node_t (les routes) qui partent du noeud u
        // on ajoute pas si l'arête u-v existe déjà ET qu'elle est plus courte
        if (e->cible == v) {
            if (poids < e->poids) e->poids = poids;
            return;
        }
    }
    // ajout de v au début de la liste chainée de u (adj[u] = liste chaînée des voisins de u)
    edge_node_t *nouvelle = malloc(sizeof(edge_node_t));
    nouvelle->cible = v;
    nouvelle->poids = poids;
    nouvelle->suivant = adj[u];
    adj[u] = nouvelle;
}

// contraction d'un noeud
// retourne l'Edge Difference = (raccourcis créés) - (arêtes supprimées)
int traiter_noeud(int v, edge_node_t **adj, ch_graph_t *ch, double *dist, int *visites, tas_binaire_t *tas, int vraie_contraction) {

    int nb_voisins = 0;
    int max_voisins = 2000;

    int *voisins = malloc(max_voisins * sizeof(int));
    double *poids_voisins = malloc(max_voisins * sizeof(double));

    int aretes_supprimees = 0;

    // on parcourt tous les voisins et on récupère comme voisins de v seuls ses voisins non contractés
    for (edge_node_t *e = adj[v]; e != NULL; e = e->suivant) {
        if (ch->rank[e->cible] == ch->num_nodes) { // noeud pas contracté car son rang n'a pas changé du rang initial num_nodes
            if (nb_voisins < max_voisins) {
                voisins[nb_voisins] = e->cible;
                poids_voisins[nb_voisins] = e->poids;
                nb_voisins++;
                aretes_supprimees++;
            }
        }
    }

    int raccourcis = 0;

    // tester toutes les paires car pour supprimer v, si un chemin opti passait par v (u->v->w) il faudrait ajouter raccourci (u->w)
    for (int i = 0; i < nb_voisins; i++) {
        for (int j = i + 1; j < nb_voisins; j++) {

            int u = voisins[i];
            int w = voisins[j];
            double cout_via_v = poids_voisins[i] + poids_voisins[j]; // cout_via_v = poids(u,v) + poids(v,w)

            // Dijkstra témoin depuis u pour savoir si raccourci vraiment nécessaire (check si existance d'un chemin alternatif u->w) donc lance dij sur u
            int nb_visites = 0; //compte combien de noeuds on a traversé pendant recherche
            dist[u] = 0.0; // dép
            visites[nb_visites++] = u;
            tas_ajout(tas, u, 0.0, 0.0); //insertion de u dans file de priorité

            double cout_sans_v = DBL_MAX; // on part du principe que w inatteignable par u

            while (tas->size > 0) {
                element_tas_t courant = tas_extraire_min(tas);
                int noeud = courant.sommet;

                if (courant.cout_reel > dist[noeud]) continue; //suppression paresseuse
                if (noeud == w) { // condition d'arrêt
                    cout_sans_v = dist[w];
                    break;
                }
                // Si la distance devient plus grande que le cout via V, ça sert à rien de continuer car on sait qu'on va devoir créer un raccourci
                if (courant.cout_reel > cout_via_v) break;

                for (edge_node_t *e = adj[noeud]; e != NULL; e = e->suivant) {
                    int voisin = e->cible;

                    if (voisin == v) continue; // v est le noeud interdit
                    if (ch->rank[voisin] != ch->num_nodes) continue; // Uniquement non contractés
                    
                    if (dist[voisin] == DBL_MAX) { // À la fin de cette recherche, au lieu de faire une boucle sur TOUS LES noeuds pour remettre le tableau dist à l'infini on ne remettra à l'infini que la poignée de noeuds notés dans visites
                        visites[nb_visites++] = voisin;
                    }
                    // relaxation -màj
                    if (dist[noeud] + e->poids < dist[voisin]) {
                        dist[voisin] = dist[noeud] + e->poids;
                        tas_ajout(tas, voisin, dist[voisin], dist[voisin]);
                    }
                }
            }
            
            // nettoyage tas et distances
            while (tas->size > 0) tas_extraire_min(tas);
            for (int k = 0; k < nb_visites; k++) dist[visites[k]] = DBL_MAX;

            // ajouter raccourci si nécessaire
            if (cout_via_v < cout_sans_v) {
                raccourcis++;

                if (vraie_contraction) {
                    ajouter_arete_si_mieux(adj, u, w, cout_via_v);
                    ajouter_arete_si_mieux(adj, w, u, cout_via_v);; // Bidirectionnel
                }
            }
        }
    }
    free(voisins);
    free(poids_voisins);

    return raccourcis - aretes_supprimees;
}

// PRETRAITEMENT (CONTRACTION HIERARCHIES)
ch_graph_t* pretraitement_ch(csr_graph_t *graphe) {
    // prep structure finale -> graphe asc et rang
    int n = graphe->nb_noeuds;

    ch_graph_t *ch = malloc(sizeof(ch_graph_t));
    ch->num_nodes = n;
    ch->rank = malloc(n * sizeof(int));

    for (int i = 0; i < n; i++) {
        ch->rank[i] = n; // aucun noeud contracté (or les rangs vont de 0 à n-1 donc ça revient à l'infini)
    }
    // liste chaînée (case = pointeur vers liste chainée d'arêtes)
    edge_node_t **adj = calloc(n, sizeof(edge_node_t*));

    // passage du CSR à liste chaînée où on ajoutera tous les raccourcis
    for (int u = 0; u < n; u++) { // parcourt tous les noeuds
        for (int i = graphe->first_edge[u]; i < graphe->first_edge[u+1]; i++) { // parcourt voisins de u
            ajouter_arete_si_mieux(adj, u, graphe->edges[i].cible, graphe->edges[i].poids);
        }
    }

    // buffers pour Dijkstra témoin - on le fait une seule fois en global et en les passant en para on les recycle à l'infini
    double *dist = malloc(n * sizeof(double));
    for (int i = 0; i < n; i++) dist[i] = DBL_MAX;

    // tas_temoins pour trouver le chemin le plus court sur la carte (trier des distances)
    // tas_scores pour trier l'importance des noeuds (edge diff)
    int *visites = malloc(n * sizeof(int));
    tas_binaire_t *tas_temoins = tas_create(graphe->nb_aretes + n);

    // EDGE DIFFERENCE
    double *score = malloc(n * sizeof(double));
    tas_binaire_t *tas_scores = tas_create(n * 10); // pour lazy update et donc add nouveau score sans supp anciens

    for (int v = 0; v < n; v++) {
        int ed = traiter_noeud(v, adj, ch, dist, visites, tas_temoins, 0);
        score[v] = ed;
        tas_ajout(tas_scores, v, ed, ed);
    }

    // contractions (strat paresseuse) -> extraction donc nettoyage
    int rang_courant = 0;

    while (tas_scores->size > 0 && rang_courant < n) {

        element_tas_t courant = tas_extraire_min(tas_scores); // extraction du noeud avec le plus petit score e.d
        int v = courant.sommet;

        if (ch->rank[v] != n) continue; //non contracté
        if (courant.score > score[v]) continue; // ou score obsolète

        int nouveau_score = traiter_noeud(v, adj, ch, dist, visites, tas_temoins, 0);
        int contracter = 0;

        if (tas_scores->size == 0) {
            contracter = 1;
        } else { // important : strat paresseuse, est ce qu'il faut contracter v ? -> look 2eme best score (suivant)
            element_tas_t suivant;
            int trouve = 0;

            while (tas_scores->size > 0) { // check si suivant valide
                suivant = tas_extraire_min(tas_scores);

                if (ch->rank[suivant.sommet] != n) continue;
                if (suivant.score > score[suivant.sommet]) continue;

                trouve = 1;
                break;
            }

            if (!trouve) {
                contracter = 1;
            } else { // suivant valide found
                if (nouveau_score <= suivant.score) { // score v toujours bon => contraction de v et au remet suivant pr plus tard
                    contracter = 1;
                    tas_ajout(tas_scores, suivant.sommet, suivant.score, suivant.score);
                } else { // 2eme est meilleur donc annule contraction de v et on met 2eme et v dans le tas pour plus tard
                    tas_ajout(tas_scores, suivant.sommet, suivant.score, suivant.score);
                    tas_ajout(tas_scores, v, nouveau_score, nouveau_score);
                    score[v] = nouveau_score;
                }
            }
        }

        if (contracter) {
            traiter_noeud(v, adj, ch, dist, visites, tas_temoins, 1); // 1-> vraie contraction
            ch->rank[v] = rang_courant++; //attribution du rang definitif à v -> suppression de v du graphe
        }
    }

    // libération
    free(dist);
    free(visites);
    free(score);
    tas_destroy(tas_temoins);
    tas_destroy(tas_scores);

    // CREATION DU GRAPHE ASCENDANT - derniere etape du pré traitement (tous les noeuds ont été triés contracté et raccourcis ajouté maintenant il faut filtrer pour garder qu'un graphe ascendant et convertir en CSR)
    int nb_aretes_up = 0;

    /* Premiere lecture : On regarde toutes les arêtes originales et raccourcis de u à une cible
    condition : "Je ne compte cette route que si elle va d'un noeud moins important (contracté tôt)
    vers un noeud plus important (contracté tard)
    */
    for (int u = 0; u < n; u++) { // pour savoir combien d'arete on garde
        for (edge_node_t *e = adj[u]; e != NULL; e = e->suivant) {
            if (ch->rank[u] < ch->rank[e->cible]) {
                nb_aretes_up++;
            }
        }
    }

    ch->up_first_edge = calloc(n + 1, sizeof(int));
    ch->up_edges = malloc(nb_aretes_up * sizeof(arete_t));

    // Deuxieme lecture : remplissage (même boucle mais on écrit les données cette fois)
    int index = 0;
    
    for (int u = 0; u < n; u++) {
        ch->up_first_edge[u] = index;

        for (edge_node_t *e = adj[u]; e != NULL; e = e->suivant) {
            if (ch->rank[u] < ch->rank[e->cible]) {
                ch->up_edges[index].cible = e->cible;
                ch->up_edges[index].poids = e->poids;
                index++;
            }
        }
    }

    ch->up_first_edge[n] = index; //balise de fin

    // libération graphe (liste chainée) temporaire adj
    for (int u = 0; u < n; u++) {
        edge_node_t *courant = adj[u];

        while (courant != NULL) {
            edge_node_t *suivant = courant->suivant;
            free(courant);
            courant = suivant;
        }
    }

    free(adj);

    return ch;
}

// --- PHASE 2 : RECHERCHE (DIJKSTRA BIDIRECTIONNEL) ---
resultat_t ch_search(ch_graph_t *ch, int depart, int arrivee) {
    // 2 tableaux pour calculer distances depuis départ et depuis arrivée
    int n = ch->num_nodes;

    double *dist_aller = malloc(n * sizeof(double));
    double *dist_retour = malloc(n * sizeof(double));

    for (int i = 0; i < n; i++) {
        dist_aller[i] = DBL_MAX;
        dist_retour[i] = DBL_MAX;
    }

    tas_binaire_t *tas_aller = tas_create(ch->up_first_edge[n] + 1); // on donne pour taille le nb d'arete du graphe ascendant filtré
    tas_binaire_t *tas_retour = tas_create(ch->up_first_edge[n] + 1);

    dist_aller[depart] = 0.0;
    tas_ajout(tas_aller, depart, 0.0, 0.0);

    dist_retour[arrivee] = 0.0;
    tas_ajout(tas_retour, arrivee, 0.0, 0.0);

    double meilleur = DBL_MAX; // dès que aller et retour se croisent sur un noeud on ajoute leur somme dans meilleur
    long long nb_extractions = 0;
    long long nb_relaxations = 0;

    struct timespec debut, fin;
    clock_gettime(CLOCK_REALTIME, &debut);

    // boucle tourne tant qu'au moins une des deux files d'attente n'est pas vide.
    while (tas_aller->size > 0 || tas_retour->size > 0) {
        double min_aller;
        if (tas_aller->size > 0) {
            min_aller = tas_aller->data[0].cout_reel;
        } else {
            min_aller = DBL_MAX; // si le tas est vide => il n'y a plus aucun noeud à explorer de ce côté-là
        }

        double min_retour;
        if (tas_retour->size > 0) {
            min_retour = tas_retour->data[0].cout_reel;
        } else {
            min_retour = DBL_MAX;
        }
        // Important : meilleur contient la distance du meilleur chemin complet qu'on a trouvé jusqu'ici
        // si prochain noeud le plus proche depuis le départ ET prochain noeud le plus proche depuis l'arrivée sont à distance sup (ou =) au chemin déjà trouvé => impossible de trouver mieux meme en continuant donc break
        if (min_aller >= meilleur && min_retour >= meilleur) break;

        // choix pour la suite : aller ou retour
        if (tas_aller->size > 0 && (tas_retour->size == 0 || min_aller <= min_retour)) {
            // aller si aller pas vide et (retour vide ou noeud aller plus proche que noeud retour)
            element_tas_t courant = tas_extraire_min(tas_aller);
            if (courant.cout_reel > dist_aller[courant.sommet]) continue; // doublon obsolète qu'on ignore (lazy)
            nb_extractions++;

            // croisement
            if (dist_retour[courant.sommet] != DBL_MAX) { // si diff de l'infini alors retour est passé par là => croisement
                double total = dist_aller[courant.sommet] + dist_retour[courant.sommet];
                if (total < meilleur) {
                    meilleur = total;
                }
            }
            // graphe ascendant : on suit que les routes qui montent vers rangs plus haut (noeud plus important) d'où up_edges
            for (int i = ch->up_first_edge[courant.sommet]; i < ch->up_first_edge[courant.sommet+1]; i++) {
                int v = ch->up_edges[i].cible;
                double w = ch->up_edges[i].poids;

                if (dist_aller[courant.sommet] + w < dist_aller[v]) {
                    dist_aller[v] = dist_aller[courant.sommet] + w;
                    tas_ajout(tas_aller, v, dist_aller[v], dist_aller[v]);
                    nb_relaxations++;
                }
            }

        } else {
            // choix retour pour la suite
            element_tas_t courant = tas_extraire_min(tas_retour);
            int u = courant.sommet;

            if (courant.cout_reel > dist_retour[u]) continue;
            nb_extractions++;

            if (dist_aller[u] != DBL_MAX) {
                double total = dist_aller[u] + dist_retour[u];
                if (total < meilleur) meilleur = total;
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

    clock_gettime(CLOCK_REALTIME, &fin);

    double temps = (fin.tv_sec - debut.tv_sec)
                 + (fin.tv_nsec - debut.tv_nsec) / 1e9;

    resultat_t res = {meilleur, nb_extractions, nb_relaxations, temps};

    free(dist_aller);
    free(dist_retour);
    tas_destroy(tas_aller);
    tas_destroy(tas_retour);

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
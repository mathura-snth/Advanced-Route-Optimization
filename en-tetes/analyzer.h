#ifndef __ANALYZER_H__
#define __ANALYZER_H__

#include <stddef.h>

// Structure pour stocker et analyser les performances (temps ou mémoire).
typedef struct analyzer_s{
  double * cost;                // Valeur brute (ex: temps d'une requête)
  long double * cumulative_cost; // Somme des coûts jusqu'à l'étape i
  long double cumulative_square; // Somme des carrés (pour la variance)
  size_t capacity;
  size_t size;   
} analyzer_t;

// Fonctions de base
analyzer_t * analyzer_create();
void analyzer_destroy(analyzer_t * a);
void analyzer_append(analyzer_t * a, double x);

// Fonctions de statistiques (Amorti / Moyenne)
long double get_total_cost(analyzer_t * a);
long double get_amortized_cost(analyzer_t * a, size_t pos);
long double get_average_cost(analyzer_t * a);
long double get_variance(analyzer_t * a);
long double get_standard_deviation(analyzer_t * a);

//sauvegarde les données dans un fichier CSV. au format : Requete, Cout, Cout_Cumule, Cout_Amorti
void save_values(analyzer_t * a, const char * path);

#endif
#include "analyzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

analyzer_t * analyzer_create(){
  analyzer_t * res = (analyzer_t *) malloc( sizeof(analyzer_t) );
  res->capacity = 4;
  res->cost = (double *) malloc( sizeof(double) * res->capacity );
  res->cumulative_cost = (long double *) malloc( sizeof(long double) * res->capacity );
  res->cumulative_square = 0.;
  res->size = 0;
  return res;
}

void analyzer_destroy(analyzer_t * a){
  if( a != NULL ){
    free( a->cost );
    free( a->cumulative_cost );
    free( a );
  }
}

void analyzer_append(analyzer_t * a, double x){
  if( a!=NULL ){
    if( a->size >= (a->capacity * 3)/4 ){
      a->capacity *= 2; 
      a->cost = (double *) realloc(a->cost, sizeof(double) * a->capacity);
      a->cumulative_cost = (long double *) realloc(a->cumulative_cost, sizeof(long double) * a->capacity);
    }
    a->cost[a->size] = x;    
    a->cumulative_cost[a->size] = (a->size) ? a->cumulative_cost[a->size-1]+x : x;
    a->cumulative_square += x*x;
    a->size++;
  }
}

long double get_total_cost(analyzer_t * a){
  return (a->size) ? a->cumulative_cost[a->size-1] : -1;
}

long double get_amortized_cost(analyzer_t * a, size_t pos){
  if(pos >= 0 && pos < a->size)
    return (pos)? a->cumulative_cost[pos]/pos : a->cumulative_cost[pos];
  return -1;
}

long double get_average_cost(analyzer_t * a){
  if(a->size)
    return a->cumulative_cost[a->size - 1]/a->size;
  return -1;
}

long double get_variance(analyzer_t * a){
  long double mean, mean_square;
  if(a->size){
    mean = get_average_cost(a);
    mean_square = mean * mean;
    return (a->cumulative_square / a->size) - mean_square; 
  }
  return -1;
}

long double get_standard_deviation(analyzer_t * a){
  if(a->size)
    return sqrt(get_variance(a));
  return -1;
}

void save_values(analyzer_t * a, const char * path){
  FILE * f;
  size_t i;
  if( (f = fopen(path, "w")) != NULL ){
    for (i = 0; i < a->size; ++i){
      fprintf(f,"%zu %lf %Lf\n", i, a->cost[i], get_amortized_cost(a, i));
    }
    fclose(f); 
  }else{
    fprintf(stderr, "Could not save values in file %s\n", path);
  }
}

void plot_values(analyzer_t * a){
  size_t i;
  for (i = 0; i < a->size; ++i){
    printf("%zu %lf %Lf\n", i, a->cost[i], get_amortized_cost(a, i));
  }
}
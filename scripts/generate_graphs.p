# 1. GRAPHIQUE DES TEMPS DE RÉPONSE
# Configuration de l'image de sortie (PNG, 1000x600 pixels)
set terminal pngcairo size 1000,600 enhanced font "Arial,12"
set output "resultats/comparison_graph_gnuplot.png"

set title "Évolution des performances par algorithme"
set xlabel "Index de la requête"
set ylabel "Temps de réponse (secondes)"

# Échelle logarithmique indispensable car CH est minuscule face à Dijkstra
set logscale y 10
set format y "10^{%L}"
set grid

# "smooth bezier" permet de lisser la courbe, comme on l'a fait en Python avec la moyenne mobile
plot "resultats/time_dijkstra.plot" using 1:2 smooth bezier title "Dijkstra" linewidth 2 linecolor rgb "blue", \
     "resultats/time_astar.plot" using 1:2 smooth bezier title "A*" linewidth 2 linecolor rgb "orange", \
     "resultats/time_alt.plot" using 1:2 smooth bezier title "ALT" linewidth 2 linecolor rgb "green", \
     "resultats/time_ch.plot" using 1:2 smooth bezier title "CH" linewidth 2 linecolor rgb "red"

# 2. GRAPHIQUE DE LA MÉMOIRE (RAM)
# On change juste le fichier de sortie et les titres, on garde la même fenêtre
set output "resultats/memory_graph_gnuplot.png"
set title "Comparatif de la consommation RAM"
set ylabel "Mémoire allouée par requête (Octets)"

unset logscale y
set format y "%g"
set yrange [0:*]

plot "resultats/memory_dijkstra.plot" using 1:2 with lines title "Dijkstra / A* / ALT" linewidth 2 linecolor rgb "blue", \
     "resultats/memory_ch.plot" using 1:2 with lines title "Contraction Hierarchies" linewidth 2 linecolor rgb "red"


# 3. GRAPHIQUE DES NOEUDS EXPLORÉS (EXTRACTIONS)
set output "resultats/extractions_graph_gnuplot.png"
set title "Espace de recherche : Nœuds extraits du tas"
set ylabel "Nombre de nœuds explorés"

# On remet l'échelle logarithmique car la différence est énorme
set logscale y 10
set format y "10^{%L}"
set yrange [*:*] # Laisse Gnuplot calculer la meilleure plage

plot "resultats/extract_dijkstra.plot" using 1:2 smooth bezier title "Dijkstra" linewidth 2 linecolor rgb "blue", \
     "resultats/extract_astar.plot" using 1:2 smooth bezier title "A*" linewidth 2 linecolor rgb "orange", \
     "resultats/extract_alt.plot" using 1:2 smooth bezier title "ALT" linewidth 2 linecolor rgb "green", \
     "resultats/extract_ch.plot" using 1:2 smooth bezier title "CH" linewidth 2 linecolor rgb "red"


# 4. GRAPHIQUE DES RELAXATIONS
set output "resultats/relaxations_graph_gnuplot.png"
set title "Espace de recherche : Nombre de relaxations"
set ylabel "Nombre de relaxations"

# On s'assure que l'échelle logarithmique est bien active
set logscale y 10
set format y "10^{%L}"
set yrange [*:*]

# On garde le lissage bezier et les mêmes couleurs pour la cohérence visuelle
plot "resultats/relaxations_dijkstra.plot" using 1:2 smooth bezier title "Dijkstra" linewidth 2 linecolor rgb "blue", \
     "resultats/relaxations_astar.plot" using 1:2 smooth bezier title "A*" linewidth 2 linecolor rgb "orange", \
     "resultats/relaxations_alt.plot" using 1:2 smooth bezier title "ALT" linewidth 2 linecolor rgb "green", \
     "resultats/relaxations_ch.plot" using 1:2 smooth bezier title "CH" linewidth 2 linecolor rgb "red"
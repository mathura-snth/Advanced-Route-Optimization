SDA : Comment encoder graphes
—— Choisir types de graphes -> par maps ou autre (Ou générer nous-mêmes (avantage : mieux contrôler / cas interessant))

Matrice d’adjacence, liste chaînée -> coder le graphe en matrice de liste d’adjacence
Implémenter dijkstra et A*
Réfléchir à des moyens d’optimiser les opérations

Avoir structure efficace sur les graphes (on veut pas changer la carte, on veut que ce soit compact)
Difficulté : comment accéder aux voisins etc

Grilles perturbées

Prendre tous les sommets -> garder les intersection (points valence de 2)
Garder les données comme ça et faire graphes etc pcq peut etre optimisation possible via ça


Trier les points
En fonction du nombre de routes qui passent par ces points, si une seule route par ces points alors pas d’intersection => on l’enlève mais garder les données des points retirés car sinon fausse la distance 

En gros : on stocke la distance entre les points reliant A et B et on les cumule pour avoir distance A-B par ces points là mais on supprime ces points pour dire que c’est pas A-g-g-h-j-j-k-i-u-y-B mais A-B


Ce que j’ai utilisé :
https://overpass-turbo.eu/#
[out:json][timeout:200];
(
  way(119894541);
  way(119894531);
  way(347935857);
  way(568745133);
  way(568745133);
  way(568745132);
);
out geom;

https://geodatamine.fr/
https://download.geofabrik.de/europe/france/ile-de-france.html
https://www.openstreetmap.org/export


[out:json][timeout:200];
(
  way(119894541);
  way(119894531);
  way(347935857);
  way(568745133);
  way(568745133);
  way(568745132);
);
out geom;


Attention il faut rendre le projet reproductible
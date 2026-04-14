# Projet : Moteur de Route Planning, *répondre vite à des requêtes de plus court chemin*

Ce projet vise à construire un moteur de calcul d'itinéraires sur des réseaux routiers.
L'objectif est d'implémenter et de comparer plusieurs algorithmes de plus court chemin (Dijkstra, A*, ALT, Contraction Hierarchies) en optimisant la représentation en mémoire et les temps de requête.

## 1. Préparation et Extraction des Données (Python)

Les données brutes proviennent d'extractions OpenStreetMap (fichiers `.osm.pbf`). Au lieu d'utiliser le format XML (.osm) traité directement en C, nous avons préféré utiliser une pipeline hybride **Python (Osmium/Pandas) + PBF** pour deux raisons :

1. D'après https://learnosm.org/fr/osm-data/file-formats/, le format binaire PBF est une version compressée, ce qui permet de manipuler des zones denses (comme l'Île-de-France) sans saturer la mémoire. Le passage par un format texte XML aurait eu un surcoût de parsing bien plus important. (Sachant qu'il s'agisse du format `.osm` ou `.osm.pbf`, les deux fichiers contiennent les mêmes informations).
   
2. Nous avons utilisé Python pour fournir au moteur C un graphe sous forme de fichiers. Cela augmente la vitesse de chargement et nous permet de nous concentrer sur l'optimisation algorithmique.

### Étapes de traitement

Pour transformer les données brutes d'OpenStreetMap en un graphe exploitable par notre moteur en C, nous avons basé notre script d'extraction sur la bibliothèque `pyosmium`.
- Nous avons appliqué le design pattern recommandé par la documentation officielle de pyosmium : le SimpleHandler (lecture en steaming -> appel de nos fonctions que si rencontre d'une entité géo).

// ce qu'on a apporté comme modifications :
- Structures des données des .txt générés :
  1) self.nodes sous forme de dictionnaire : pour stocker temporairement les intersections. (dico pour rechercher par ID en temps constant $O(1)$, indispensable pour récupérer instantanément les coordonnées GPS lors de la lecture des routes)
  2) self.edges sous forme de liste : pour accumuler les segments de route. L'ajout en fin de liste facilite la conversion finale vers un DataFrame pandas

- On a filtré pour ne conserver que les types de routes ('motorway', 'trunk' etc) pertinents. Enfin,la sortie a été entièrement restructurée, on a remappé les identifiants OSM pour n'avoir que des vameurs contigues entre 0 et N.

- Comme on a que des points GPS (longitute, latitude), on n'a pas la longueur des routes. On ajoute donc a notre script Python la formule Haversine pour calculer la longueur physique de chaque petit bout de rue entre deux intersections voisines. On calcule donc la distance entre un point A et son voisin direct B, qui devient le poids de l'arête dans le fichier edges.txt.

**Point sur la formule Haversine :**
C'est la distance du grand cercle entre deux points d'une sphère, à partir de leurs longitudes et latitudes (d'après Wikipedia).
d = 2R * arcin(sqrt(a))
  = 2R * arctan(sqrt(a) / sqrt(1 - a)) -> on a opté pour cette équivalence pour éviter des plantages : à cause des arrondis du processeur, le calcul de a peut parfois donner un résultat supérieur à 1 (ex: 1.000000000002) or la fonction arcsin ne supporte pas les valeurs supérieures à 1.
avec a = sin((lat2 - lat1)/2)^2 + cos(lat1) * cos(lat2) * sin((lon2 - lon1)/2)^2.

**Fichiers générés :**
* `nodes.txt` : contient les sommets (ID mappé, Latitude, Longitude). Nécessaire pour les heuristiques géométriques (comme A*).
* `edges.txt` : contient les arêtes (Source, Destination, Distance).





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
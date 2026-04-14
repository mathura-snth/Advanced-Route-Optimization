# Projet : Moteur de Route Planning, *répondre vite à des requêtes de plus court chemin*

Ce projet vise à construire un moteur de calcul d'itinéraires sur des réseaux routiers.
L'objectif est d'implémenter et de comparer plusieurs algorithmes de plus court chemin (Dijkstra, A*, ALT, Contraction Hierarchies) en optimisant la représentation en mémoire et les temps de requête.

## 1. Extraction des Données (Python)

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

## 2. Le Graphe CSR : Représentation en Mémoire

L'énoncé du projet souligne un point important : sur des réseaux routiers de grande taille (plusieurs centaines de milliers d'arêtes), la structure mémoire compte énormément. 

### Notre choix : La structure CSR (Compressed Sparse Row)
Elle consiste à "aplatir" le graphe dans deux tableaux contigus en mémoire.

Dans une approche classique, chaque arête stocke son point de départ, sa cible et son poids (u, v, w). Le CSR nous permet de supprimer le point de départ de chaque arête en regroupant les voisins d'un même nœud de manière contiguë.

Par exemple : nœud 0 relié au nœud 1 (poids 5) et au nœud 2 (poids 8).
  - Approche classique : On stocke (0, 1, 5) et (0, 2, 8) -> 6 valeurs.
  - Approche CSR : On stocke uniquement les cibles et poids (1, 5) et (2, 8) -> 4 valeurs.

Sur un graphe de plusieurs millions d'arêtes, nous économisons ainsi 33% de mémoire vive en supprimant la redondance du nœud source.

+ Accélération de l'accès aux voisins : pour retrouver ces voisins sans stocker la source, nous utilisons un tableau d'index appelé first_edge (= offsets). La contiguïté mémoire permet au processeur de charger les blocs de voisins directement dans son cache (Prefetching spatial), garantissant une itération sur les voisins extrêmement rapide et un accès direct en O(1). Dans notre exemple, first_edge[0]=1;first_edge[1]=2;first_edge[2]=3. 

Ainsi on a :
* `edges` : Un tableau unique regroupant **toutes** les arêtes du graphe (destination + poids).
* `first_edge` : Un tableau d'offsets (index). Pour accéder aux voisins d'un nœud U, l'algorithme lit le tableau `edges` de l'indice `first_edge[U]` à `first_edge[U+1]`.

----------  A FAIRE
***TESTER D'AUTRES FORMAT COMME tableau de pointeurs vers des listes chaînées (une liste par nœud contenant ses voisins). et trouver que ça a 2 défauts majeurs :
1. **Surcharge mémoire (Overhead) :** Chaque élément d'une liste chaînée nécessite le stockage d'un pointeur supplémentaire (`next`).
2. **Défaut de localité (Cache Miss) :** Les éléments alloués via de multiples appels à `malloc` sont dispersés de manière aléatoire dans la RAM. Lors du parcours des voisins (l'opération la plus fréquente dans Dijkstra), le processeur subit de constants "cache misses", ce qui effondre les performances.***

***TESTER SI ON ECONOMISE RÉELLEMENT 33% 
***

-----------


### Construction en 3 Passes
Puisque la taille des tableaux C doit être connue à la compilation ou allouée dynamiquement, le chargement du fichier s'effectue obligatoirement en trois passes optimisées :
1. **Évaluation :** Lecture du fichier pour identifier l'ID maximal ($N$ nœuds) et compter le total des arêtes, permettant une allocation mémoire (`malloc`) exacte et sans gaspillage.
2. **Calcul des Degrés :** Comptage du nombre d'arêtes sortantes pour chaque nœud, puis transformation de ces degrés en tableau d'index (offsets) via une somme préfixe.
3. **Peuplement :** Remplissage définitif du grand tableau `edges` en utilisant les offsets calculés.


# NOTES EN PLUS PENDANT LES TPs :
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
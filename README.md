# Projet : Moteur de Route Planning, *répondre vite à des requêtes de plus court chemin*

Ce projet vise à construire un moteur de calcul d'itinéraires sur des réseaux routiers.
L'objectif est d'implémenter et de comparer plusieurs algorithmes de plus court chemin (Dijkstra, A*, ALT, Contraction Hierarchies) en optimisant la représentation en mémoire et les temps de requête.

## 1. Extraction des Données (Python)

Les données brutes proviennent d'extractions OpenStreetMap (fichiers `.osm.pbf`). Au lieu d'utiliser le format XML (.osm) traité directement en C, nous avons préféré utiliser une pipeline hybride **Python (Osmium/Pandas) + PBF** pour deux raisons :

1. D'après https://learnosm.org/fr/osm-data/file-formats/, le format binaire PBF est une version compressée, ce qui permet de manipuler des zones denses (comme l'Île-de-France) sans saturer la mémoire. Le passage par un format texte XML aurait eu un surcoût de parsing bien plus important. (Sachant qu'il s'agisse du format `.osm` ou `.osm.pbf`, les deux fichiers contiennent les mêmes informations).
 
2. Nous avons utilisé Python pour fournir au moteur C un graphe sous forme de fichiers. Cela augmente la vitesse de chargement et nous permet de nous concentrer sur l'optimisation algorithmique.

### Étapes de traitement

Pour transformer les données brutes d'OpenStreetMap en un graphe exploitable par notre moteur en C, nous avons basé notre script d'extraction sur la bibliothèque `pyosmium`.
- Nous avons appliqué le design pattern recommandé par la documentation officielle de pyosmium : le SimpleHandler (lecture en streaming -> appel de nos fonctions que si rencontre d'une entité géo).

Ce qu'on a apporté comme modifications/optimisations :
- Structures des données des .txt générés :
 1) self.nodes sous forme de dictionnaire : pour stocker temporairement les intersections. (dico pour rechercher par ID en temps constant O(1), indispensable pour récupérer instantanément les coordonnées GPS lors de la lecture des routes)
 2) self.edges sous forme de liste : pour accumuler les segments de route. L'ajout en fin de liste facilite la conversion finale vers un DataFrame pandas

- On a filtré pour ne conserver que les types de routes ('motorway', 'trunk' etc) pertinents. Enfin,la sortie a été entièrement restructurée, on a remappé les identifiants OSM pour n'avoir que des valeurs contigues entre 0 et N.

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

Dans une approche classique, chaque arête stocke son point de départ, sa cible et son poids (u, v, w). Le CSR nous permet de supprimer le point de départ de chaque arête en regroupant les voisins d'un même noeud de manière contiguë.

Par exemple, admettons qu'on ait :
 1- noeud 0 relié au noeud 1 (poids 5) et au noeud 2 (poids 8) 
 2- noeud 1 relié au noeud 2 (poids 3)
 3- noeud 2 relié nulle part
 - Approche classique pour 0 : On stocke (0, 1, 5) et (0, 2, 8) -> 6 valeurs.
 - Approche CSR pour 0 : On stocke uniquement les cibles et poids (1, 5) et (2, 8) -> 4 valeurs.

Sur un graphe de plusieurs millions d'arêtes, nous économisons ainsi 33% de mémoire vive en supprimant la redondance du noeud source.

+ Accélération de l'accès aux voisins : pour retrouver ces voisins sans stocker la source, nous utilisons un tableau d'index appelé first_edge (= offsets). Il répond à la question "où commencent les voisins de mon noeud courant ?". La contiguïté mémoire permet au processeur de charger les blocs de voisins directement dans son cache, garantissant une itération sur les voisins rapide et un accès direct en O(1). Dans notre exemple, first_edge[0] = 0; first_edge[1] = 2; first_edge[2] = 3. Si on veut les voisins du noeud 0 :
 - Début : first_edge[0] = 0
 - Fin : first_edge[1] = 2
 - Donc on lit les cases 0 et 1 (on exclut la borne de fin 2). On obtient bien les deux arêtes du noeud 0.

Ainsi on a :
* `edges` : Un tableau unique regroupant **toutes** les arêtes du graphe (destination + poids).
* `first_edge` : Un tableau d'offsets (index). Pour accéder aux voisins d'un noeud U, l'algorithme lit le tableau `edges` de l'indice `first_edge[U]` à `first_edge[U+1]`.


### Construction du CSR
On doit résoudre 2 problèmes :
1. la taille des tableaux alloués dynamiquement doit être définie à l'avance
2. les données géographiques du fichier brut sont "dans le désordre" (les routes d'un même noeud ne sont pas écrites les unes à la suite des autres)

Pour cela on implémente en 3 lectures du fichier :
1. **Évaluation :** 1ère lecture du fichier pour identifier l'ID maximal (N noeuds) et compter le total des arêtes, permettant une allocation mémoire (`malloc`) sans gaspillage.
2. **Calcul des Degrés :** 2ème lecture du fichier pour compter le nombre d'arêtes sortantes pour chaque noeud (= nombre de voisins de chaque noeud), puis transformation de ces degrés en tableau d'index (offsets) via une somme (accumulation) qui permet de générer `first_edge`.
3. **Remplissage :** 3ème lecture pour parcourir le fichier désordonné, on utilise une copie temporaire (current_offset), chaque arête lue est insérée dans la case mémoire qui lui était réservée (bon nombre de résevation grâce à 1ère lecture). On a alors un remplissage groupé, contiguë et définitif du tableau `edges`.


## 3 Algorithme de Dijkstra et file de priorité
L'algorithme de Dijkstra est une exploration itérative du sommet le plus proche du point de départ.
Pour que cette recherche soit efficace, nous avons besoin d'une structure de données capable de nous renvoyer le minimum donc on a mis en place une file de priorité sous la forme d'un tas binaire.

### Le tas binaire (Min-Heap)
Au lieu d'utiliser des pointeurs et des allocations dynamiques pour chaque noeud de l'arbre ce qui ralentirait l'exécution, on modélise notre tas binaire dans un tableau.
La relation d'ordre fixée est l'inférieur ou égal (on parle de min-heap)
Pour un élément à l'indice i :
- Son parent se trouve à l'indice (i - 1) / 2
- Son enfant gauche se trouve à l'indice 2i + 1
- Son enfant droit se trouve à l'indice 2i + 2
Notre tas stocke des structures element_tas_t contenant : (sommet, distance). C'est la distance qui sert de clé de tri pour l'arbre.

### L'approche Lazy Deletion vs Decrease Key
C'est l'une des optimisations majeures de notre implémentation. En général quand on trouve un chemin plus court vers un sommet déjà présent dans le tas, on doit mettre à jour sa distance et le faire remonter (opération Decrease-Key).

Mais chercher un élément au milieu d'un tas binaire prend un temps linéaire O(V), à moins de maintenir un lourd tableau de pointeurs inversés (pos[]) qui consomme de la mémoire.

C'est pourquoi on a choisi la **Lazy Deletion** :
- Ajout de doublons : quand une meilleure distance est trouvée pour un noeud V, on insère une nouvelle paire (V, nouvelle_distance) dans le tas (sans supprimer l'ancienne).
- Filtrage à l'extraction : L'arbre garantit que la paire contenant la distance la plus courte remontera à la racine en premier. Donc quand l'ancienne paire (inutile et plus grande) finira par sortir du tas plus tard, notre algorithme l'ignorera (`if (courant.distance > distances[u]) continue;`).
Cette méthode augmente la taille maximale du tas (qui doit être bornée par le nombre total d'arêtes E, et non de sommets V), mais elle simplifie le code et accélère l'exécution.

### Déroulement de l'algorithme
- Création d'un tableau des distances (où tous les `sommets` sont initialisés à l'infini) et d'un tableau `predecesseurs` initialisé à -1 qui serbira à retracer l'itinéraire.
- La distance du noeud de départ est fixée à 0 et cette première paire est insérée dans le tas binaire pour faire la recherche.
- Tant que le tas n'est pas vide :
 - Extraction du minimum : sommet U ayant la plus petite distance cumulée
 - Early Exit : Si le sommet extrait U est notre destination, l'algorithme s'arrête. Le principe du tas binaire garantit que c'est le chemin le plus court définitif (propriété du min-heap).
 - Relaxation via CSR : on prend les arêtes sortantes de U en utilisant la structure CSR (de first_edge[U] à first_edge[U+1]). Pour chaque voisin V, si la distance pour l'atteindre en passant par U est inférieure à sa distance actuellement connue, on met à jour le tableau distances, on insère la nouvelle paire dans le tas et on recommence.

Complexité : Avec notre implémentation via tas binaire et format CSR, l'algorithme s'exécute avec une complexité temporelle de O((V + E) \log V) dans le pire des cas, ce qui permet de traiter des réseaux routiers de la taille de l'Île-de-France très rapidement, en quelques secondes.

## 4. Algorithme A* et Heuristique
Contrairement à Dijkstra qui explore le graphe dans toutes les directions, l'algorithme A* optimise réellement la recherche en l'orientant vers la destination.

La différence avec Dijkstra est dans la manière dont on trie les noeuds dans notre file de priorité.
Pour chaque sommet v , on calcule un score `f(v) = g(v) + h(v)`.
Avec : 
- `g(v)` (Coût réel) : La distance exacte parcourue depuis le point de départ jusqu'à v. C'est la valeur utilisée par Dijkstra.
- `h(v)` (Heuristique) : Une estimation de la distance restante entre v et l'arrivée.

Pour que A* trouve toujours le chemin le plus court, l'heuristique h(v) doit être **admissible**. Cela signifie qu'elle ne doit **jamais surestimer** la distance réelle

On utilise pour cela la formule Haversine (calculée à partir des coordonnées dans nodes.txt). Or comme la ligne droite est le chemin le plus court entre deux points, la distance à vol d'oiseau est une heuristique admissible pour notre réseau routier.

**Adaptation du tas binaire pour A**
Le passage de Dijkstra à A* a nécessité une modification de la structure de notre tas binaire :
- Clé de tri : Le tas est désormais trié selon le score f(v) et non uniquement la distance. C'est en fait ce qui permet à l'algorithme de privilégier les routes qui semblent se rapprocher de la cible.
- Vérification g(v) : Pour notre approche Lazy Deletion, on stocke aussi, en plus de f(v), la distance réelle g(v) dans chaque élément du tas. Ainsi la comparaison pour l'extraction se fait sur g (if (courant.g > distances[u]) continue;) car c'est la seule valeur qui représente un coût réel atteint (= distance physiquement valide).

**Déroulement et Performance**
- Contrairement à Dijkstra, A* nécessite de charger en mémoire le fichier `nodes.txt` pour accéder instantanément aux latitudes/longitudes de chaque sommet.
- À chaque itération, A* extrait le noeud qui minimise la distance totale estimée.

Ainsi on a réellement un gain d'efficacité : dans nos tests sur le réseau Île-de-France, A* réduit énormément le nombre d'extractions ( noeuds visités) par rapport à Dijkstra. En ignorant les routes qui s'éloignent de la destination, le temps de calcul est divisé par un facteur significatif tout en garantissant le même résultat optimal.


## 5. Algorithme ALT
Limite géométrique de A* : sur un réseau routier réel, la distance à vol d'oiseau peut être très loin de la réalité (à cause de fleuves, de montagnes ou autre). 

L'algorithme ALT permet d'avoir une heuristique plus puissante, basée sur la topologie réelle du graphe routier, sans avoir besoin des coordonnées GPS.

### L'inégalité triangulaire
Dans un triangle formé par notre noeud courant U, notre destination V, et un point de repère fixe appelé, le Landmark L, la distance directe entre U et V sera toujours **supérieure ou égale** à la différence de leurs distances respectives vers le Landmark :

`dist(U, V) >= |dist(U, L) - dist(V, L)|`

Puisque cette différence ne **surestime jamais** la vraie distance routière, elle constitue une heuristique parfaite.

### Fonctionnement de l'algorithme
L'implémentation de ALT se divise en deux phases distinctes :

1. Phase de pré-calcul, avant de répondre à la requete :
 - On sélectionne K noeuds aléatoires sur la carte (les Landmarks).
 - Pour chacun de ces Landmarks, on lance un algorithme de **Dijkstra** sans heuristique et sans condition d'arrêt pour calculer la distance entre ce Landmark et absolument tous les autres noeuds.
 - Ces distances sont stockées en mémoire dans un tableau.

2. Phase de requête, quand un utilisateur demande un trajet entre un point de départ et une arrivée :
  - L'algorithme de parcours fonctionne comme A* (avec tas binaire trié par le score f).
  - **L'heuristique change :** Au lieu d'utiliser la trigonométrie (Haversine), le moteur interroge ses tableaux de pré-calculs. Pour chaque voisin, il calcule `|dist(voisin, L) - dist(arrivee, L)|` pour tous les Landmarks L.
  - L'algorithme garde la **valeur maximale** trouvée parmi tous les Landmarks.

---------- A FAIRE
***TESTER D'AUTRES FORMAT au lieu de CSR, COMME tableau de pointeurs vers des listes chaînées (une liste par noeud contenant ses voisins). et trouver que ça a 2 défauts majeurs :
1. **Surcharge mémoire (Overhead) :** Chaque élément d'une liste chaînée nécessite le stockage d'un pointeur supplémentaire (`next`).
2. **Défaut de localité (Cache Miss) :** Les éléments alloués via de multiples appels à `malloc` sont dispersés de manière aléatoire dans la RAM. Lors du parcours des voisins (l'opération la plus fréquente dans Dijkstra), le processeur subit de constants "cache misses", ce qui effondre les performances.***

***TESTER SI ON ECONOMISE RÉELLEMENT 33% 
***

dij :

*** tester ? Au lieu d'utiliser des pointeurs et des allocations dynamiques pour chaque noeud de l'arbre (ce qui causerait des défauts de localité spatiale et ralentirait l'exécution), 
****

*** tester decrease key ?
***

*** tester Mais chercher un élément au milieu d'un tas binaire prend un temps linéaire O(V), à moins de maintenir un lourd tableau de pointeurs inversés (pos[]) qui consomme de la mémoire et dégrade les performances du cache.
***


a_star :

***Ainsi on a réellement un gain d'efficacité : dans nos tests sur le réseau Île-de-France, A* réduit énormément le nombre d'extractions ( noeuds visités) par rapport à Dijkstra. En ignorant les routes qui s'éloignent de la destination, le temps de calcul est divisé par un facteur significatif (quel facteur) tout en garantissant le même résultat optimal
facteur 3-5 à tester
***

-----------
--------------------------------------------

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
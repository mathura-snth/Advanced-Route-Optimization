# Projet : Moteur de Route Planning, *répondre vite à des requêtes de plus court chemin*

Ce projet vise à construire un moteur de calcul d'itinéraires sur des réseaux routiers.
L'objectif est d'implémenter et de comparer plusieurs algorithmes de plus court chemin (Dijkstra, A*, ALT, Contraction Hierarchies) en optimisant la représentation en mémoire et les temps de requête.

---
## 1. Extraction des Données (Python)

Les données brutes proviennent d'extractions OpenStreetMap (fichiers `.osm.pbf`). Au lieu d'utiliser le format XML (.osm) traité directement en C, on a préféré utiliser une pipeline hybride **Python (Osmium/Pandas) + PBF** pour deux raisons :

1. D'après https://learnosm.org/fr/osm-data/file-formats/, le format binaire PBF est une version compressée, ce qui permet de manipuler des zones denses (comme l'Île-de-France) sans saturer la mémoire. Le passage par un format texte XML aurait eu en plus un coût de parsing bien plus important. (Sachant qu'il s'agisse du format `.osm` ou `.osm.pbf`, les deux fichiers contiennent les mêmes informations).
 
2. on a utilisé Python pour fournir au moteur C un graphe sous forme de fichiers. Cela augmente la vitesse de chargement et nous permet de nous concentrer sur l'optimisation algorithmique.

### Étapes de traitement

Pour transformer les données brutes d'OpenStreetMap en un graphe exploitable par notre moteur en C, on a basé notre script d'extraction sur la bibliothèque `pyosmium`.
- on a appliqué le design pattern recommandé par la documentation officielle de pyosmium : le SimpleHandler (lecture en streaming ligne par ligne et appel de nos fonctions que si l'on rencontre une entité géographique correspondante).

Ce qu'on a apporté comme modifications/optimisations :
- Structures des données des .txt générés :
  1) `self.nodes` sous forme de dictionnaire : pour stocker temporairement les intersections. (dico pour rechercher par ID en temps constant O(1), indispensable pour récupérer instantanément les coordonnées GPS lors de la lecture des routes)
  2) `self.edges` sous forme de liste : pour accumuler les segments de route. L'ajout en fin de liste facilite la conversion finale vers un DataFrame pandas

- On a filtré pour ne conserver que les types de routes ('motorway', 'trunk' etc) pertinents. Enfin,la sortie a été entièrement restructurée, on a remappé les identifiants OSM pour n'avoir que des valeurs contigues entre 0 et N.

- Comme on a que des points GPS (longitute, latitude), on n'a pas la longueur des routes. On ajoute donc la formule Haversine pour calculer la longueur physique de chaque petit bout de rue entre deux intersections voisines. On calcule donc la distance entre un point A et son voisin direct B, qui devient le poids de l'arête dans le fichier `edges.txt`.

**Formule Haversine :** 
C'est la distance du grand cercle entre deux points d'une sphère, à partir de leurs longitudes et latitudes (d'après Wikipedia https://fr.wikipedia.org/wiki/Formule_de_haversine).
d = 2R * arcin(sqrt(a))
 = 2R * arctan(sqrt(a) / sqrt(1 - a)) -> on a opté pour cette équivalence pour éviter des plantages : à cause des arrondis du processeur (*le calcul de a peut parfois donner un résultat supérieur à 1 (ex: 1.000000000002) or la fonction arcsin ne supporte pas les valeurs supérieures à 1*).
avec a = sin((lat2 - lat1)/2)^2 + cos(lat1) * cos(lat2) * sin((lon2 - lon1)/2)^2.

**Fichiers générés :**
* `nodes.txt` : contient les sommets (id mappé, latitude, longitude). Nécessaire pour les heuristiques géométriques (comme A*).
* `edges.txt` : contient les arêtes (source, destination, distance).

---
## 2. Le Graphe CSR : Représentation en Mémoire

L'énoncé du projet souligne un point important : sur des réseaux routiers de grande taille (plusieurs centaines de milliers d'arêtes), la structure mémoire compte énormément. 

### Notre choix : La structure CSR (Compressed Sparse Row)
Elle consiste à aplatir le graphe dans deux tableaux contigus en mémoire.

En général, chaque arête stocke son point de départ, sa cible et son poids (u, v, w). Le CSR nous permet de supprimer le point de départ de chaque arête en regroupant les voisins d'un même noeud de manière contiguë.

Par exemple, admettons qu'on ait :
- noeud 0 relié au noeud 1 (poids 5) et au noeud 2 (poids 8) 
- noeud 1 relié au noeud 2 (poids 3)
- noeud 2 relié nulle part
  - **Approche normale pour 0 :** On stocke (0, 1, 5) et (0, 2, 8) -> 6 valeurs.
  - **Approche CSR pour 0 :** On stocke uniquement les cibles et poids (1, 5) et (2, 8) -> 4 valeurs.

Sur un graphe de plusieurs millions d'arêtes, nous économisons de la mémoire vive en supprimant la redondance du noeud source.

**Accélération de l'accès aux voisins** : pour retrouver ces voisins sans stocker la source, on utilise un tableau d'index appelé `first_edge` (= offsets) pour savoir où commencent les voisins du noeud courant. Le fait que la mémoire soit contigue permet de charger les blocs de voisins directement dans le cache, garantissant une itération sur les voisins rapide et un accès direct en O(1).

Par exemple, `first_edge[0] = 0;` `first_edge[1] = 2;` `first_edge[2] = 3`.
Si on veut les voisins du noeud 0 :
 - Début : `first_edge[0] = 0`
 - Fin : `first_edge[1] = 2`
 - Donc on lit les cases 0 et 1 (on exclut la borne de fin 2). On obtient bien les deux arêtes du noeud 0.

Ainsi on a :
* `edges` : Un tableau unique qui regroupe **toutes** les arêtes du graphe (destination + poids).
* `first_edge` : Un tableau d'offsets (index). Pour accéder aux voisins d'un noeud U, l'algorithme lit `edges` de l'indice `first_edge[U]` à `first_edge[U+1]`.


### Construction du CSR
On doit résoudre 2 problèmes :
1. la taille des tableaux alloués dynamiquement doit être définie à l'avance
2. les données géographiques du fichier brut ne sont pas ordonnées (les routes d'un même noeud ne sont pas écrites les unes à la suite des autres)

Pour cela on implémente en 3 lectures du fichier :
1. **Évaluation :** 1ère lecture du fichier pour identifier l'id maximal (N noeuds) et compter le nombre total des arêtes, permettant une allocation mémoire (`malloc`) sans gaspillage.
2. **Calcul des degrés :** 2ème lecture du fichier pour compter le nombre d'arêtes sortantes pour chaque noeud (= nombre de voisins de chaque noeud), puis transformation de ces degrés en tableau d'index (offsets) via une somme (accumulation) qui permet de générer `first_edge`.
3. **Remplissage :** 3ème lecture pour parcourir le fichier désordonné, on utilise une copie temporaire (current_offset), chaque arête lue est insérée dans la case mémoire qui lui était réservée (bon nombre de résevation grâce à 1ère lecture). On a alors un remplissage contigu du tableau `edges`.

---
## 3 Algorithme de Dijkstra et file de priorité - stratégie paresseuse

L'algorithme de Dijkstra est une exploration itérative du sommet le plus proche du point de départ.
Pour que cette recherche soit efficace, on a besoin d'une structure de données capable de nous renvoyer le minimum donc on a mis en place une file de priorité sous la forme d'un tas binaire.

### Le tas binaire
Au lieu d'utiliser des pointeurs et des allocations dynamiques pour chaque noeud de l'arbre ce qui ralentirait l'exécution, on modélise notre tas binaire dans un tableau.
La relation d'ordre fixée est l'**inférieur ou égal (min-heap)**.

Pour un élément à l'indice `i` :
- Son parent se trouve à l'indice `(i - 1) / 2`
- Son enfant gauche se trouve à l'indice `2i + 1`
- Son enfant droit se trouve à l'indice `2i + 2`
Notre tas stocke des structures `element_tas_t` contenant la priorité et le coût, c'est le score qui sert de clé de tri pour l'arbre.

### Le problème de la mise à jour
En général, quand un chemin plus court est découvert vers un sommet déjà dans le tas, on fait une **diminution de clé** mais cette opération est très coûteuse :
  - coût de recherche : localiser un noeud spécifique au milieu d'un tableau de tas prend un temps linéaire O(V).
  - indexation : pour ramener ce coût à O(log V), il faudrait avoir un tableau d'index inversés qu'on met à jour à chaque échange de noeuds, ce qui complexifie le code et consomme de la mémoire vive.

### L'approche paresseuse

Pour résoudre ce problème nous nous sommes inspirés de la stratégie paresseuse vue en cours pour les tas de Fibonacci.

Dans un tas de Fibonacci, on ne cherche pas à avoir une structure parfaite à chaque modification, on se permet d'avoir une liste brouillon d'arbres non consolidés pour gagner du temps pendant des insertions. Dans le cas du Dijkstra, on a essayé de faire un peu de même :

- **l'opération la plus coûteuse** : la recherche et la mise à jour immédiate d'un noeud lors d'une relaxation.
- **report du coût** : plutôt que de mettre à jour un noeud existant, quand une meilleure distance est trouvée pour un noeud V, on insère une nouvelle paire (V, nouvelle_distance) dans le tas (sans supprimer l'ancienne).
- **nettoyage à l'extraction** : le prix de la stratégie paresseuse n'est payé qu'au moment de l'opération `extraire_min`. C'est là qu'on fait la **suppression paresseuse** : puisque les propriétés du tas garantissent que la paire contenant la distance la plus courte remonte toujours à la racine en premier, on traite la donnée valide en première. Donc quand les anciennes paires (doublons obsolètes et plus grands) finissent par sortir du tas plus tard, on compare leur valeur avec ce qui a été enregistré dans notre tableau global et si la distance extraite est supérieure à la meilleure distance connue, elle est ignorée : `if (courant.cout_reel > distances[u]) continue;`

### Algorithme de Dijkstra
1) Création d'un tableau des distances (où tous les `sommets` sont initialisés à l'infini) et d'un tableau `predecesseurs` initialisé à -1 qui serbira à retracer l'itinéraire.
2) La distance du noeud de départ est à 0 et cette première paire est insérée dans le tas binaire pour faire la recherche.
3) Tant que le tas n'est pas vide :
 - **Extraction du minimum** : sommet U ayant la plus petite distance cumulée
 - **Early Exit** : Si le sommet extrait U est la destination, on arrête puisque c'est le chemin le plus court définitif (propriété min-heap dont on parlait).
 - **Relaxation via CSR** : on prend les arêtes sortantes de U en utilisant la structure CSR (de first_edge[U] à first_edge[U+1]). Pour chaque voisin V, si la distance pour l'atteindre en passant par U est inférieure à sa distance actuellement connue, on met à jour le tableau distances, on insère la nouvelle paire dans le tas et on recommence.

**Complexité** : Avec notre implémentation via tas binaire et format CSR, l'algorithme s'exécute avec une complexité temporelle de O((V + E) \log V) dans le pire des cas, ce qui permet de traiter le réseau routiers en moins d'une minute.

---
## 4. Algorithme A* et Heuristique

Contrairement à Dijkstra qui explore le graphe dans toutes les directions, l'algorithme A* optimise réellement la recherche en l'orientant vers la destination.

La différence avec Dijkstra est dans la manière dont on trie les noeuds dans notre file de priorité.
Pour chaque sommet v , on calcule un score `f(v) = g(v) + h(v)`.
Avec : 
- `g(v)` (Coût réel) : La distance exacte parcourue depuis le point de départ jusqu'à v. C'est la valeur utilisée par Dijkstra.
- `h(v)` (Heuristique) : Une estimation de la distance restante entre v et l'arrivée.

Pour que A* trouve toujours le chemin le plus court, l'heuristique h(v) doit être **admissible**. Elle ne doit **jamais surestimer** la distance réelle

On utilise pour cela la **formule Haversine** (calculée à partir des coordonnées dans `nodes.txt`). Or comme la ligne droite est le chemin le plus court entre deux points, la distance à vol d'oiseau est une heuristique admissible pour notre réseau routier.

**Adaptation du tas binaire pour A***
Le passage de Dijkstra à A* a nécessité une modification de la structure de notre tas binaire :
- Clé de tri : le score f(v) et non uniquement la distance. C'est en fait ce qui permet de privilégier les routes qui semblent se rapprocher de la destination.
- Vérification du coût réel : de suppression paresseuse, on stocke aussi, en plus de f(v), la distance réelle g(v) dans chaque élément du tas (`cout_reel`). Ainsi la comparaison pour l'extraction se fait sur g (`if (courant.cout_reel > distances[u]) continue;`) car c'est la seule valeur qui représente un coût réel atteint (= distance physiquement valide).

**Déroulement et Performance**
- Contrairement à Dijkstra, A* nécessite de charger le fichier `nodes.txt` pour accéder directement aux latitudes/longitudes de chaque sommet.
- À chaque itération, A* extrait le noeud qui minimise la distance totale estimée.

Ainsi on a réellement un gain d'efficacité : dans nos tests sur le réseau Île-de-France, A* réduit énormément le nombre d'extractions (noeuds visités) par rapport à Dijkstra. En ignorant les routes qui s'éloignent de la destination, le temps de calcul est divisé tout en ayant le même résultat optimal.

---
## 5. Algorithme ALT

Limite de A* : sur un réseau routier réel, la distance à vol d'oiseau est souvent très loin de la réalité (à cause de fleuves ou autre). 

L'algorithme ALT permet d'avoir une heuristique plus puissante, basée sur la topologie réelle du graphe routier, sans avoir besoin des coordonnées GPS.

### L'inégalité triangulaire
Dans un triangle formé par notre noeud courant U, notre destination V, et un point de repère fixe (le landmark L), la distance directe entre U et V sera toujours **supérieure ou égale** à la différence de leurs distances respectives vers le Landmark :
    `|dist(U, V) + dist(V, L)| >= dist(V, L)`
<=> `dist(U, V) >= |dist(U, L) - dist(V, L)|`

Puisque cette différence ne **surestime jamais** la vraie distance routière, elle constitue elle aussi une heuristique parfaite.

### Fonctionnement de l'algorithme
L'implémentation de ALT se divise en deux phases distinctes :

1. Phase de pré-calcul, avant de répondre à la requete :
 - On sélectionne K noeuds aléatoires sur la carte (landmarks)
 - Pour chacun d'eux, on lance un algorithme de **Dijkstra** sans heuristique et sans condition d'arrêt pour calculer la distance entre ce landmark et absolument tous les autres noeuds
 - Ces distances sont stockées dans un tableau

2. Phase de réelle recherche du plus court chemin entre départ et arrivée :
  - L'algorithme de parcours fonctionne comme A* (avec tas binaire trié par le score f).
  - **L'heuristique change :** Au lieu d'utiliser (Haversine), on utilise les tableaux de pré-calculs : pour chaque voisin, on calcule `|dist(voisin, L) - dist(arrivee, L)|` pour tous les landmarks L.
  - L'algorithme garde la **valeur maximale** trouvée parmi tous les landmarks.

---
## 6. Contraction Hierarchies (CH)

Pour aller encore plus vite, on a implémenté l'algorithme des Contraction Hierarchies (CH). Cette méthode repose sur une phase de **pré-traitement préalable** (lourde en calcul) et une phase de **requête** (rapide). 

On peut faire un parallèle avec la **compression de chemins** vue en cours dans la structure **Union-Find **: dans les deux cas, on accepte de modifier préalablement une structure afin de rendre les opérations futures beaucoup plus rapides.
Mais contrairement à Union-Find qui restructure des arbres, les Contraction Hierarchies enrichissent le graphe en ajoutant des raccourcis virtuels.

### Phase 1 : Pré-traitement (Contraction des noeuds)
Le pré-traitement consiste à contracter les sommets un à un selon un ordre d’importance.

Chaque sommet reçoit un rang, correspondant à son ordre de contraction : plus son rang est élevé, plus il est considéré comme important dans la hiérarchie.

- **choix de l’ordre de contraction, Edge Difference** : l’efficacité de CH dépend fortement de l’ordre choisi. Nous utilisons l’heuristique abordée par John Lazarsfeld de l’Edge Difference, définie par : ED(v)= ∣raccourcis(v)∣ − ∣arêtes supprimées(v)|

Cette métrique favorise les sommets dont la contraction simplifie fortement le graphe tout en ajoutant peu de nouveaux raccourcis.

- **stratégie de mise à jour paresseuse, Lazy Update** : Comme la contraction d'un noeud modifie l'Edge Difference de ses voisins (donc les scores des autres sommets deviennent potentiellement obsolètes), nous utilisons une file de priorité pour maintenir l'ordre. Plutôt que de tout recalculer (coûteux), nous employons une stratégie paresseuse :
      - le sommet extrait du tas voit son score recalculé
      - si ce score reste meilleur ou équivalent au prochain meilleur candidat valide, il est contracté
      - sinon il est réinséré avec sa nouvelle priorité.
Ce qui permet de conserver un bon ordre de contraction sans recalcul global.

- **contraction d’un sommet** : Lorsqu’un sommet v est contracté, il disparaît temporairement du graphe. Le problème est qu’un plus court chemin pouvait passer par : u→v→w. Donc supprimer v risquerait de détruire ce chemin optimal. Pour préserver toutes les distances, il faut examiner chaque paire de voisins (u,w) du sommet contracté v.

- **recherche témoin (Witness Search)** : Pour savoir si ce raccourci est réellement nécessaire, on lance un Dijkstra local entre u et w. Cette recherche :
      - interdit le passage par le sommet contracté v
      - s’effectue que sur les sommets encore non contractés
      - s’arrête dès que la meilleure distance extraite dépasse le coût du raccourci candidat pour éviter d’effectuer une exploration complète inutile.

Deux cas sont possibles :
      - Un chemin alternatif existe avec un coût inférieur ou égal → aucun raccourci ajouté
      - Aucun chemin alternatif assez court n’existe → raccourci virtuel inséré


- **optimisation mémoire** :  Comme les recherches témoins sont nombreuses, pour éviter de réinitialiser entièrement le tableau des distances après chaque Dijkstra local, on remet à l’infini que les sommets visités pendant la recherche (d'où stockage dans visite). Ce qui réduit le coût du pré-traitement.

- **construction du graphe ascendant** : une fois tous les sommets contractés, le graphe est filtré donc on garde que les arêtes allant d’un sommet de rang inférieur vers un sommet de rang supérieur : `if (rank[u] < rank[v])`` On obtient ainsi le graphe **ascendant**. Cette orientation oblige les recherches futures à remonter dans la hiérarchie, donc réduit le nombre de sommets explorés.

### Phase 2 : Réelle recherche - Dijkstra Bidirectionnel (hiérarchique)
La recherche du plus court chemin s’effectue par un Dijkstra bidirectionnel sur le graphe ascendant : deux explorations sont lancées en même temps :
- Un tas **"Aller"** depuis le noeud de départ
- Un tas **"Retour"** depuis la destination
Les deux recherches utilisent le graphe ascendant, possible car les raccourcis sont bidirectionnels (pendant la contraction).

- **détection de rencontre** : quand un sommet a été atteint par les deux explorations, on obtient un chemin candidat : dist_aller (u) + dist_retour(u). Le plus petit de ces candidats est la meilleure solution courante.

- **condition d'arrêt** : on s'arrête dès que les distances minimales présentes au sommet des deux tas dépassent la meilleure solution déjà trouvée. À ce moment là aucun meilleur chemin ne peut encore être découvert.


# NOTES EN PLUS PENDANT LES TPs :

Matrice d’adjacence, liste chaînée -> coder le graphe en matrice de liste d’adjacence
Avoir structure efficace sur les graphes (on veut pas changer la carte, on veut que ce soit compact)
Difficulté : comment accéder aux voisins etc

Grilles perturbées

Prendre tous les sommets -> garder les intersection (points valence de 2)
Garder les données comme ça et faire graphes etc pcq peut etre optimisation possible via ça


Trier les points
En fonction du nombre de routes qui passent par ces points, si une seule route par ces points alors pas d’intersection => on l’enlève mais garder les données des points retirés car sinon fausse la distance 

En gros : on stocke la distance entre les points reliant A et B et on les cumule pour avoir distance A-B par ces points là mais on supprime ces points pour dire que c’est pas A-g-g-h-j-j-k-i-u-y-B mais A-B
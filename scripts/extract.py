import osmium
import pandas as pd
import math

# Calcul de la distance en mètres entre deux points à la surface de la Terre
def distance(lat1, lon1, lat2, lon2):
    R = 6371000 # rayon de la Terre (mètres)
    phi1 = math.radians(lat1)
    phi2 = math.radians(lat2)
    a = math.sin((math.radians(lat2 - lat1))/2)**2 + math.cos(phi1)*math.cos(phi2)*math.sin((math.radians(lon2 - lon1))/2)**2
    return R * (2 * math.atan2(math.sqrt(a), math.sqrt(1 - a)))

# classe osmium.SimpleHandler qui va lire le fichier PBF
class MapHandler(osmium.SimpleHandler):
    def __init__(self):
        osmium.SimpleHandler.__init__(self)
        self.nodes = {} # dico pour stocker temporairement les intersections
        self.edges = [] # liste pour stocker les routes (arêtes)
        self.valid_highways = {'motorway', 'trunk', 'primary', 'secondary', 'tertiary', 'unclassified', 'residential'}

    # fonction appelée automatiquement pour chaque noeud du fichier
    def node(self, n):
        # On mémorise la position de chaque intersection
        self.nodes[n.id] = (n.location.lat, n.location.lon)

    # fonction appelée automatiquement pour chaque chemin du fichier
    def way(self, w):
        # on vérifie que l'objet est une route + qu'elle fait partie de notre liste de type de route
        if 'highway' in w.tags and w.tags['highway'] in self.valid_highways:

            # on va procéder segment par segment
            for i in range(len(w.nodes) - 1):
                # récupération de l'identifiant des noeuds qui font le segment
                n1 = w.nodes[i].ref
                n2 = w.nodes[i+1].ref
                # vérification que ces deux points sont stockés dans notre dico
                if n1 in self.nodes and n2 in self.nodes:

                    # appel à Haversine
                    dist = distance(self.nodes[n1][0], self.nodes[n1][1], self.nodes[n2][0], self.nodes[n2][1])
                    # création de l'arête dans edges.txt format : (départ, arrivée, distance)
                    self.edges.append((n1, n2, dist))

print("Lecture du .pbf")
handler = MapHandler()
handler.apply_file("../data/ile-de-france-260403.osm.pbf")

print(f"nb routes trouvées = {len(handler.edges)}.")
edges_df = pd.DataFrame(handler.edges, columns=['u', 'v', 'length'])

# On crée des ID propres (0, 1, 2...) pour le programme C
unique_nodes = pd.unique(edges_df[['u', 'v']].values.ravel('K'))
mapping = {osm_id: new_id for new_id, osm_id in enumerate(unique_nodes)}

edges_df['u_mapped'] = edges_df['u'].map(mapping)
edges_df['v_mapped'] = edges_df['v'].map(mapping)

print("Sauvegarde des fichiers")
edges_df[['u_mapped', 'v_mapped', 'length']].to_csv("../data/edges.txt", sep=' ', index=False, header=False)

# Fichier des noeuds
nodes_data = [(mapping[n_id], handler.nodes[n_id][0], handler.nodes[n_id][1]) for n_id in unique_nodes]
nodes_df = pd.DataFrame(nodes_data, columns=['id_mapped', 'lat', 'lon']).sort_values('id_mapped')
nodes_df.to_csv("../data/nodes.txt", sep=' ', index=False, header=False)
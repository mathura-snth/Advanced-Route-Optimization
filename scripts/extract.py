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
        self.noeuds = {} # dico pour stocker temporairement les intersections
        self.aretes = [] # liste pour stocker les routes (arêtes)
        self.routes = {'motorway', 'trunk', 'primary', 'secondary', 'tertiary', 'unclassified', 'residential'}

    # fonction appelée automatiquement pour chaque noeud du fichier
    def node(self, n):
        # mémorise la position de chaque intersection
        self.noeuds[n.id] = (n.location.lat, n.location.lon)

    # fonction appelée automatiquement pour chaque chemin du fichier
    def way(self, w):
        # on vérifie que l'objet est une route + qu'elle fait partie de notre liste de type de route
        if 'highway' in w.tags and w.tags['highway'] in self.routes:

            # on va procéder segment par segment
            for i in range(len(w.nodes) - 1):
                # récupération de l'identifiant des noeuds qui font le segment
                depart = w.nodes[i].ref
                arrivee = w.nodes[i+1].ref
                # vérification que ces deux points sont stockés dans notre dico
                if depart in self.noeuds and arrivee in self.noeuds:

                    # appel à Haversine
                    dist = distance(self.noeuds[depart][0], self.noeuds[depart][1], self.noeuds[arrivee][0], self.noeuds[arrivee][1])
                    # création de l'arête dans aretes.csv format : (départ, arrivée, distance)
                    self.aretes.append((depart, arrivee, dist))

print("on charge et lit le fichier .osm.pbf")
handler = MapHandler()
handler.apply_file("donnees/ile-de-france-260403.osm.pbf")

print(f"nb routes trouvées = {len(handler.aretes)}.")
aretes_df = pd.DataFrame(handler.aretes, columns=['u', 'v', 'length'])

# On crée des ID propres (0, 1, 2...) pour le programme C
print("id propres et continus")
unique_noeuds = pd.unique(aretes_df[['u', 'v']].values.ravel('K'))
mapping = {osm_id: new_id for new_id, osm_id in enumerate(unique_noeuds)}

aretes_df['u_mapped'] = aretes_df['u'].map(mapping)
aretes_df['v_mapped'] = aretes_df['v'].map(mapping)

print("Sauvegarde des fichiers")
aretes_df[['u_mapped', 'v_mapped', 'length']].to_csv("donnees/aretes.csv", sep=',', index=False, header=False)

# Fichier des noeuds
noeuds_donnees = [(mapping[n_id], handler.noeuds[n_id][0], handler.noeuds[n_id][1]) for n_id in unique_noeuds]
noeuds_df = pd.DataFrame(noeuds_donnees, columns=['id_mapped', 'lat', 'lon']).sort_values('id_mapped')
noeuds_df.to_csv("donnees/noeuds.csv", sep=',', index=False, header=False)
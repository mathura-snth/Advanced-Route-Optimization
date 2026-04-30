import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

def analyze_plot(filename, label):
    # Lecture du fichier .plot (Colonnes : Index, Valeur, Coût Amorti)
    data = pd.read_csv(filename, sep=' ', header=None, names=['idx', 'val', 'amortized'])
    
    values = data['val']
    stats = {
        'Algo': label,
        'Moyenne (ms)': values.mean() * 1000,
        'Médiane (ms)': values.median() * 1000,
        'P95 (ms)': np.percentile(values, 95) * 1000,
        'Max (ms)': values.max() * 1000
    }
    return stats, values

algos = [
    ('results/time_dijkstra.plot', 'Dijkstra'),
    ('results/time_astar.plot', 'A*'),
    ('results/time_alt.plot', 'ALT'),
    ('results/time_ch.plot', 'CH')
]

all_stats = []
plt.figure(figsize=(10, 6))

for path, name in algos:
    s, v = analyze_plot(path, name)
    all_stats.append(s)
    plt.plot(v.index, v.rolling(window=50).mean() * 1000, label=f"{name} (Moyenne mobile)")

# Affichage du tableau récapitulatif
df_stats = pd.DataFrame(all_stats)
print(df_stats.to_string(index=False))

# Génération du graphique
plt.xlabel("Index de la requête")
plt.ylabel("Temps de réponse (ms)")
plt.title("Évolution des performances par algorithme")
plt.legend()
plt.yscale('log') # Échelle logarithmique car CH est trop rapide comparé à Dijkstra
plt.savefig("results/comparison_graph.png")
plt.show()
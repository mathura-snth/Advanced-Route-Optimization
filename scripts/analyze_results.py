import pandas as pd
import numpy as np
import os

def analyze_plot(filename, label, is_memory=False):
    if not os.path.exists(filename):
        print(f"Fichier introuvable: {filename}")
        return None
        
    donnees = pd.read_csv(filename, sep=' ', header=None, names=['idx', 'val', 'amortized'])
    values = donnees['val']
    
    if is_memory:
        # La mémoire est enregistrée en octets, on la convertit en Mégaoctets (Mo)
        values = values / (1024 * 1024)
        stats = {
            'Algo': label,
            'RAM Moyenne (Mo)': values.mean(),
            'RAM Max (Mo)': values.max()
        }
    else:
        # Le temps est en secondes, on le convertit en millisecondes (ms)
        mean_sec = values.mean() # pour débit
        stats = {
            'Algo': label,
            'Débit (req/s)': int(1 / mean_sec) if mean_sec > 0 else 0,
            'Moyenne (ms)': values.mean() * 1000,
            'Médiane (ms)': values.median() * 1000,
            'P95 (ms)': np.percentile(values, 95) * 1000,
            'Max (ms)': values.max() * 1000
        }
    return stats, values

# PARTIE 1 : ANALYSE DES TEMPS DE RÉPONSE
algos_time = [
    ('resultats/time_dijkstra.plot', 'Dijkstra'),
    ('resultats/time_astar.plot', 'A*'),
    ('resultats/time_alt.plot', 'ALT'),
    ('resultats/time_ch.plot', 'CH')
]

time_stats = []

for path, name in algos_time:
    s, v = analyze_plot(path, name, is_memory=False)
    if s is not None:
        time_stats.append(s)

if time_stats:
    df_time = pd.DataFrame(time_stats)
    print("\nstatistique des temps")
    print(df_time.to_string(index=False))
    with open('resultats/synthese_temps.txt', 'w') as f:
        f.write("STATISTIQUES DE TEMPS\n")
        f.write(df_time.to_string(index=False))
        f.write("\n")

# PARTIE 2 : ANALYSE DE LA MÉMOIRE (RAM)
algos_memory = [
    ('resultats/memory_dijkstra.plot', 'Dijkstra / A* / ALT'),
    ('resultats/memory_ch.plot', 'Contraction Hierarchies')
]

mem_stats = []

for path, name in algos_memory:
    s, v = analyze_plot(path, name, is_memory=True)
    if s is not None:
        mem_stats.append(s)

if mem_stats:
    df_mem = pd.DataFrame(mem_stats)
    print("\nles stat pour la mémoire :")
    print(df_mem.to_string(index=False))
    with open('resultats/synthese_memoire.txt', 'w') as f:
        f.write("STATISTIQUES DE MEMOIRE\n")
        f.write(df_mem.to_string(index=False))
        f.write("\n")

# PARTIE 3 : ANALYSE DES COÛTS DE PRÉTRAITEMENT
pre_file = 'resultats/pretraitements_costs.txt'

if os.path.exists(pre_file):
    df_pre = pd.read_csv(pre_file, sep=' ', header=None, names=['Algo', 'Temps (secondes)', 'Surcoût RAM (Mo)'])
    print("\nCoût du prétraitement")
    print(df_pre.to_string(index=False))
    with open('resultats/synthese_pretraitement.txt', 'w') as f:
        f.write("COUT DU PRETRAITEMENT\n")
        f.write(df_pre.to_string(index=False))
        f.write("\n")
else:
    print(f"\nFichier de prétraitement introuvable: {pre_file}. Relancez le programme C.")
    
print("\n")
import pandas as pd
import numpy as np
import os

def analyze_plot(filename, label, is_memory=False):
    if not os.path.exists(filename):
        print(f"Fichier introuvable: {filename}")
        return None
        
    data = pd.read_csv(filename, sep=' ', header=None, names=['idx', 'val', 'amortized'])
    values = data['val']
    
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
        stats = {
            'Algo': label,
            'Moyenne (ms)': values.mean() * 1000,
            'Médiane (ms)': values.median() * 1000,
            'P95 (ms)': np.percentile(values, 95) * 1000,
            'Max (ms)': values.max() * 1000
        }
    return stats, values

# PARTIE 1 : ANALYSE DES TEMPS DE RÉPONSE
algos_time = [
    ('results/time_dijkstra.plot', 'Dijkstra'),
    ('results/time_astar.plot', 'A*'),
    ('results/time_alt.plot', 'ALT'),
    ('results/time_ch.plot', 'CH')
]

time_stats = []

for path, name in algos_time:
    s, v = analyze_plot(path, name, is_memory=False)
    if s is not None:
        time_stats.append(s)

if time_stats:
    df_time = pd.DataFrame(time_stats)
    print("\nSTATISTIQUES DE TEMPS")
    print(df_time.to_string(index=False))

# PARTIE 2 : ANALYSE DE LA MÉMOIRE (RAM)
algos_memory = [
    ('results/memory_dijkstra.plot', 'Dijkstra / A* / ALT'),
    ('results/memory_ch.plot', 'Contraction Hierarchies')
]

mem_stats = []

for path, name in algos_memory:
    s, v = analyze_plot(path, name, is_memory=True)
    if s is not None:
        mem_stats.append(s)

if mem_stats:
    df_mem = pd.DataFrame(mem_stats)
    print("\nSTATISTIQUES DE MÉMOIRE")
    print(df_mem.to_string(index=False))
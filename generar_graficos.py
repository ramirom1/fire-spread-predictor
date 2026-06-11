import matplotlib.pyplot as plt
import numpy as np
import os

# Data
# Format: { Size: { Focos: [T1, T2, T4, T8] } }
data = {
    "10000x10000": {
        1: [104148, 26546.8, 21063.0, 17699.6],
        3: [100334, 37569.8, 23231.2, 19049.6],
        5: [99656,  36815.8, 22877.8, 18010.1]
    },
    "15000x15000": {
        1: [308017, 62023.2, 55405.2, 37826.9],
        3: [279798, 87297.7, 70329.0, 47708.9],
        5: [269076, 89026.9, 67654.3, 46483.8]
    },
    "20000x20000": {
        1: [741801, 109684.0, 90247.6, 71385.0],
        3: [676599, 145972.0, 103823.0, 79533.5],
        5: [664370, 145548.0, 101118.0, 78787.3]
    }
}

processes = [1, 2, 4, 8]
markers = {1: 'o', 3: 's', 5: '^'}
colors = {1: 'blue', 3: 'orange', 5: 'green'}

output_dir = "graficos"
os.makedirs(output_dir, exist_ok=True)

# Generate plots for each size
for size, foci_data in data.items():
    
    # 1. Rendimiento (Execution Time)
    plt.figure(figsize=(8, 6))
    for foci, times in foci_data.items():
        plt.plot(processes, [t/1000 for t in times], marker=markers[foci], color=colors[foci], linewidth=2, label=f'{foci} Foco(s)')
    
    plt.xlabel('Cantidad de Procesos (P)', fontsize=12, fontweight='bold')
    plt.ylabel('Tiempo de Ejecución (Segundos)', fontsize=12, fontweight='bold')
    plt.title(f'Rendimiento - Matriz {size}', fontsize=14, fontweight='bold')
    plt.xticks(processes)
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.legend()
    plt.tight_layout()
    plt.savefig(f"{output_dir}/rendimiento_{size}.png", dpi=300)
    plt.close()

    # 2. Speedup
    plt.figure(figsize=(8, 6))
    for foci, times in foci_data.items():
        t1 = times[0]
        speedups = [t1 / t for t in times]
        plt.plot(processes, speedups, marker=markers[foci], color=colors[foci], linewidth=2, label=f'{foci} Foco(s)')
    
    # Ideal Speedup
    plt.plot(processes, processes, 'k--', linewidth=2, label='Ideal')
    
    plt.xlabel('Cantidad de Procesos (P)', fontsize=12, fontweight='bold')
    plt.ylabel('Speedup (S)', fontsize=12, fontweight='bold')
    plt.title(f'Speedup - Matriz {size}', fontsize=14, fontweight='bold')
    plt.xticks(processes)
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.legend()
    plt.tight_layout()
    plt.savefig(f"{output_dir}/speedup_{size}.png", dpi=300)
    plt.close()

    # 3. Eficiencia
    plt.figure(figsize=(8, 6))
    for foci, times in foci_data.items():
        t1 = times[0]
        efficiencies = [(t1 / t) / p for t, p in zip(times, processes)]
        plt.plot(processes, efficiencies, marker=markers[foci], color=colors[foci], linewidth=2, label=f'{foci} Foco(s)')
    
    plt.axhline(y=1.0, color='k', linestyle='--', linewidth=2, label='Ideal')
    
    plt.xlabel('Cantidad de Procesos (P)', fontsize=12, fontweight='bold')
    plt.ylabel('Eficiencia (E)', fontsize=12, fontweight='bold')
    plt.title(f'Eficiencia - Matriz {size}', fontsize=14, fontweight='bold')
    plt.xticks(processes)
    plt.ylim(0, 2.5)
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.legend(loc='lower left')
    plt.tight_layout()
    plt.savefig(f"{output_dir}/eficiencia_{size}.png", dpi=300)
    plt.close()

print(f"Graficos (Rendimiento, Speedup, Eficiencia) generados en ./{output_dir}")

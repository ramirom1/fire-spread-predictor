import matplotlib.pyplot as plt
import numpy as np

# Datos extraídos de metrics_10k_p8_f1.out
compute_f1 = 0.319
halo_f1 = 0.061
reduce_f1 = 0.735
overhead_f1 = 1.173 - (compute_f1 + halo_f1 + reduce_f1)

# Datos extraídos de metrics_10k_p8_f5.out
compute_f5 = 0.414
halo_f5 = 0.060
reduce_f5 = 1.003
overhead_f5 = 1.551 - (compute_f5 + halo_f5 + reduce_f5)

labels = ['1 Foco', '5 Focos']
compute_data = [compute_f1, compute_f5]
halo_data = [halo_f1, halo_f5]
reduce_data = [reduce_f1, reduce_f5]
overhead_data = [overhead_f1, overhead_f5]

x = np.arange(len(labels))
width = 0.5

fig, ax = plt.subplots(figsize=(10, 7))

p1 = ax.bar(x, compute_data, width, label='Cómputo Útil', color='#2ecc71', edgecolor='black')
p2 = ax.bar(x, halo_data, width, bottom=compute_data, label='Halos (MPI_Sendrecv)', color='#3498db', edgecolor='black')
p3 = ax.bar(x, reduce_data, width, bottom=np.array(compute_data)+np.array(halo_data), label='Sincronización/Espera (MPI_Allreduce)', color='#e74c3c', edgecolor='black')
p4 = ax.bar(x, overhead_data, width, bottom=np.array(compute_data)+np.array(halo_data)+np.array(reduce_data), label='Overhead de Bucle', color='#95a5a6', edgecolor='black')

# Añadir valores absolutos en las barras
def add_labels(rects):
    for rect in rects:
        height = rect.get_height()
        if height > 0.1:  # solo mostrar si es lo suficientemente grande
            ax.annotate(f'{height:.2f} ms',
                        xy=(rect.get_x() + rect.get_width() / 2, rect.get_y() + height / 2),
                        xytext=(0, 0),  
                        textcoords="offset points",
                        ha='center', va='center', color='white', fontweight='bold', fontsize=11)

add_labels(p1)
add_labels(p2)
add_labels(p3)
add_labels(p4)

# Totales arriba de las barras
ax.text(0, 1.173 + 0.05, 'Total: 1.17 ms', ha='center', va='bottom', fontweight='bold', fontsize=12)
ax.text(1, 1.551 + 0.05, 'Total: 1.55 ms', ha='center', va='bottom', fontweight='bold', fontsize=12)

ax.set_ylabel('Tiempo promedio por iteración (ms)', fontsize=12, fontweight='bold')
ax.set_title('Desglose de Tiempo por Iteración (1 Foco vs 5 Focos)', fontsize=16, fontweight='bold', pad=20)
ax.set_xticks(x)
ax.set_xticklabels(labels, fontsize=14, fontweight='bold')

# Poner la leyenda abajo para que no se corte
ax.legend(loc='upper center', bbox_to_anchor=(0.5, -0.1), ncol=2, fontsize=11)

plt.ylim(0, 1.8)
plt.grid(axis='y', linestyle='--', alpha=0.7)

# Ajustar layout manualmente para dar espacio a la leyenda abajo
plt.subplots_adjust(bottom=0.2)

plt.savefig('grafico_barras_profiling.png', dpi=300, bbox_inches='tight')
print("Gráfico generado exitosamente en 'grafico_barras_profiling.png'")

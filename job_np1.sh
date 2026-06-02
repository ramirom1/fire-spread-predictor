#!/bin/bash
#SBATCH --job-name=fire-np1
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --output=exp_np1_%j.out
#SBATCH --error=exp_np1_%j.err

module load openmpi/4.1.4

SEED=43
ITERATIONS=100000
WIND=E
SIZES=(10000 15000 20000)
FIRES=(1 3 5)

echo "=== Experimentos Secuenciales (NP=1) ==="
echo "Nodo: $(hostname)"
echo "Fecha: $(date)"

for SIZE in "${SIZES[@]}"; do
    for F in "${FIRES[@]}"; do
        echo "----------------------------------------------"
        echo "Tamaño: ${SIZE}x${SIZE} | Procesos: 1 | Focos: ${F}"
        echo "Inicio: $(date)"
        
        ./sequential_algorithm/sequential_fire \
            --rows $SIZE --cols $SIZE \
            --iterations $ITERATIONS \
            --seed $SEED \
            --wind $WIND \
            --fires $F \
            --no-window
            
        echo "Fin: $(date)"
    done
done
echo "=== Todos finalizados ==="

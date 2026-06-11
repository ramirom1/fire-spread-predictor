#!/bin/bash
#SBATCH --job-name=fire-np8
#SBATCH --nodes=1
#SBATCH --ntasks=8
#SBATCH --cpus-per-task=1
#SBATCH --output=exp_np8_%j.out
#SBATCH --error=exp_np8_%j.err

module load openmpi/4.1.4

SEED=67
ITERATIONS=100000
SIZES=(10000 15000 20000)
FIRES=(1 3 5)

echo "=== Experimentos Paralelos (NP=8) ==="
echo "Nodo: $(hostname)"
echo "Fecha: $(date)"

for SIZE in "${SIZES[@]}"; do
    for F in "${FIRES[@]}"; do
        echo "----------------------------------------------"
        echo "Tamaño: ${SIZE}x${SIZE} | Procesos: 8 | Focos: ${F}"
        echo "Inicio: $(date)"
        
        mpirun -np 8 ./parallel_algorithm/mpi_fire \
            --rows $SIZE --cols $SIZE \
            --iterations $ITERATIONS \
            --seed $SEED \
            --fires $F \
            --no-window
            
        echo "Fin: $(date)"
    done
done
echo "=== Todos finalizados ==="

#!/bin/bash
#SBATCH --job-name=fire-np4
#SBATCH --nodes=1
#SBATCH --ntasks=4
#SBATCH --cpus-per-task=1
#SBATCH --output=exp_np4_%j.out
#SBATCH --error=exp_np4_%j.err

module load openmpi/4.1.4

SEED=67
ITERATIONS=100000
SIZES=(10000 15000 20000)
FIRES=(1 3 5)

echo "=== Experimentos Paralelos (NP=4) ==="
echo "Nodo: $(hostname)"
echo "Fecha: $(date)"

for SIZE in "${SIZES[@]}"; do
    for F in "${FIRES[@]}"; do
        echo "----------------------------------------------"
        echo "Tamaño: ${SIZE}x${SIZE} | Procesos: 4 | Focos: ${F}"
        echo "Inicio: $(date)"
        
        mpirun -np 4 ./parallel_algorithm/mpi_fire \
            --rows $SIZE --cols $SIZE \
            --iterations $ITERATIONS \
            --seed $SEED \
            --fires $F \
            --no-window
            
        echo "Fin: $(date)"
    done
done
echo "=== Todos finalizados ==="

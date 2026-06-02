#!/bin/bash
#SBATCH --job-name=fire-np2
#SBATCH --nodes=1
#SBATCH --ntasks=2
#SBATCH --cpus-per-task=1
#SBATCH --output=exp_np2_%j.out
#SBATCH --error=exp_np2_%j.err

module load openmpi/4.1.4

SEED=43
ITERATIONS=100000
WIND=E
SIZES=(10000 15000 20000)
FIRES=(1 3 5)

echo "=== Experimentos Paralelos (NP=2) ==="
echo "Nodo: $(hostname)"
echo "Fecha: $(date)"

for SIZE in "${SIZES[@]}"; do
    for F in "${FIRES[@]}"; do
        echo "----------------------------------------------"
        echo "Tamaño: ${SIZE}x${SIZE} | Procesos: 2 | Focos: ${F}"
        echo "Inicio: $(date)"
        
        mpirun -np 2 ./parallel_algorithm/mpi_fire \
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

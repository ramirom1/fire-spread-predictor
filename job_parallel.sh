#!/bin/bash
#SBATCH --job-name=fire-parallel
#SBATCH --nodes=1
#SBATCH --ntasks=32
#SBATCH --cpus-per-task=1
#SBATCH --output=parallel_%j.out
#SBATCH --error=parallel_%j.err

# ============================================================
# Fire Spread Predictor – Ejecución paralela MPI en cluster
# ============================================================

module load gcc/12.2.0
module load openmpi/4.1.4

ROWS=15000
COLS=15000
ITERATIONS=100000
SEED=43
WIND=E
NP=$SLURM_NTASKS

echo "=== Fire Spread Predictor – Paralelo MPI ==="
echo "Nodo: $(hostname)"
echo "Fecha: $(date)"
echo "Procesos MPI: ${NP}"
echo "Parámetros: ${ROWS}x${COLS}, iter=${ITERATIONS}, seed=${SEED}, wind=${WIND}"
echo "=============================================="

cd parallel_algorithm
mpirun -np $NP ./mpi_fire --rows $ROWS --cols $COLS --iterations $ITERATIONS --seed $SEED --wind $WIND --no-window

echo ""
echo "=== Fin: $(date) ==="

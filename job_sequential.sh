#!/bin/bash
#SBATCH --job-name=fire-sequential
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --output=sequential_%j.out
#SBATCH --error=sequential_%j.err

# ============================================================
# Fire Spread Predictor – Ejecución secuencial en cluster
# ============================================================

module load gcc/12.2.0

ROWS=15000
COLS=15000
ITERATIONS=100000
SEED=43
WIND=E
FIRES=3

echo "=== Fire Spread Predictor – Secuencial ==="
echo "Nodo: $(hostname)"
echo "Fecha: $(date)"
echo "Parámetros: ${ROWS}x${COLS}, iter=${ITERATIONS}, seed=${SEED}, wind=${WIND}, fires=${FIRES}"
echo "==========================================="

cd sequential_algorithm
./sequential_fire --rows $ROWS --cols $COLS --iterations $ITERATIONS --seed $SEED --wind $WIND --fires $FIRES --no-window

echo ""
echo "=== Fin: $(date) ==="
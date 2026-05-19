#include "sequential_fire.h"

static double baseIgnitionProbability(CellType type) {
    switch (type) {
        case WATER:   return 0.0;
        case FOREST:  return 0.42;
        case CITY:    return 0.24;
        case BURNING: return 0.0;
        case ASH:     return 0.0;
    }
    return 0.0;
}

static bool canIgnite(CellType type) {
    return type == FOREST || type == CITY;
}

static int countBurningNeighbors(const Matrix &mat, int row, int col) {
    int rows = (int)mat.size();
    int cols = (int)mat[0].size();
    int burningNeighbors = 0;

    for (auto [nr, nc] : neighbors4(row, col, rows, cols)) {
        if (mat[nr][nc] == BURNING)
            ++burningNeighbors;
    }

    return burningNeighbors;
}

static double ignitionProbability(CellType type, int burningNeighbors) {
    double base = baseIgnitionProbability(type);
    if (base <= 0.0 || burningNeighbors <= 0)
        return 0.0;

    double survivalProbability = 1.0;
    for (int i = 0; i < burningNeighbors; ++i)
        survivalProbability *= (1.0 - base);

    return 1.0 - survivalProbability;
}

std::optional<Position> igniteRandomCell(Matrix &mat, FireState &fireState, std::mt19937 &rng) {
    std::vector<Position> burnableCells;

    for (int r = 0; r < (int)mat.size(); ++r) {
        for (int c = 0; c < (int)mat[0].size(); ++c) {
            if (canIgnite(mat[r][c]))
                burnableCells.push_back({r, c});
        }
    }

    if (burnableCells.empty())
        return std::nullopt;

    std::uniform_int_distribution<int> randomIndex(0, (int)burnableCells.size() - 1);
    Position start = burnableCells[randomIndex(rng)];

    mat[start.row][start.col] = BURNING;
    fireState.listaFuego1.push_back(start);

    return start;
}

static void markFireCandidates(
    const Matrix &mat,
    const std::vector<Position> &burningCells,
    std::vector<std::vector<bool>> &candidate,
    std::vector<Position> &candidates
) {
    int rows = (int)mat.size();
    int cols = (int)mat[0].size();

    for (const Position &cell : burningCells) {
        for (auto [nr, nc] : neighbors4(cell.row, cell.col, rows, cols)) {
            if (canIgnite(mat[nr][nc]) && !candidate[nr][nc]) {
                candidate[nr][nc] = true;
                candidates.push_back({nr, nc});
            }
        }
    }
}

int advanceFire(Matrix &mat, FireState &fireState, std::mt19937 &rng) {
    int rows = (int)mat.size();
    int cols = (int)mat[0].size();
    std::uniform_real_distribution<double> probability(0.0, 1.0);

    std::vector<std::vector<bool>> candidate(rows, std::vector<bool>(cols, false));
    std::vector<Position> candidates;
    candidates.reserve(
        4 * (
            fireState.listaFuego1.size() +
            fireState.listaFuego2.size() +
            fireState.listaFuego3.size()
        )
    );

    markFireCandidates(mat, fireState.listaFuego1, candidate, candidates);
    markFireCandidates(mat, fireState.listaFuego2, candidate, candidates);
    markFireCandidates(mat, fireState.listaFuego3, candidate, candidates);

    std::vector<Position> newFires;
    for (const Position &cell : candidates) {
        int burningNeighbors = countBurningNeighbors(mat, cell.row, cell.col);
        double ignitionChance = ignitionProbability(mat[cell.row][cell.col], burningNeighbors);

        if (probability(rng) <= ignitionChance)
            newFires.push_back(cell);
    }

    for (const Position &cell : fireState.listaFuego3)
        mat[cell.row][cell.col] = ASH;

    fireState.listaFuego3 = std::move(fireState.listaFuego2);
    fireState.listaFuego2 = std::move(fireState.listaFuego1);
    fireState.listaFuego1 = std::move(newFires);

    for (const Position &cell : fireState.listaFuego1)
        mat[cell.row][cell.col] = BURNING;

    return (int)fireState.listaFuego1.size();
}

int countActiveFires(const FireState &fireState) {
    return (int)(
        fireState.listaFuego1.size() +
        fireState.listaFuego2.size() +
        fireState.listaFuego3.size()
    );
}

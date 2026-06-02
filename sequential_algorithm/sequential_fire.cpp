#include "sequential_fire.h"

#include <cstdint>
#include <cstring>
#include <random>

// ============================================================
// Parsing de dirección de viento desde string
// ============================================================
WindDirection parseWindDirection(const char *str) {
    if (!str) return WindDirection::NONE;
    if (std::strcmp(str, "north") == 0 || std::strcmp(str, "N") == 0) return WindDirection::NORTH;
    if (std::strcmp(str, "south") == 0 || std::strcmp(str, "S") == 0) return WindDirection::SOUTH;
    if (std::strcmp(str, "east")  == 0 || std::strcmp(str, "E") == 0) return WindDirection::EAST;
    if (std::strcmp(str, "west")  == 0 || std::strcmp(str, "W") == 0) return WindDirection::WEST;
    return WindDirection::NONE;
}

// ============================================================
// Probabilidades base
// ============================================================
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

// ============================================================
// Modificador de viento
//
// Dado un vecino ardiente en (fr, fc) y un candidato en (cr, cc),
// calcula el factor multiplicativo según la dirección del viento.
//
// El viento EMPUJA el fuego en su dirección:
//   - A favor del viento:   ×1.5 (boost)
//   - Contra el viento:     ×0.3 (penalización fuerte)
//   - Perpendicular:        ×0.8 (penalización leve)
// ============================================================
static double windModifier(int fr, int fc, int cr, int cc, WindDirection wind) {
    if (wind == WindDirection::NONE) return 1.0;

    // Dirección de propagación: del fuego (fr,fc) al candidato (cr,cc)
    int dr = cr - fr;  // positivo = hacia abajo (sur)
    int dc = cc - fc;  // positivo = hacia la derecha (este)

    // Determinar si la propagación va a favor, en contra o perpendicular al viento
    switch (wind) {
        case WindDirection::NORTH: // Viento empuja hacia el norte (dr negativo)
            if (dr < 0) return 1.3;  // A favor
            if (dr > 0) return 0.3;  // Contra
            return 0.7;              // Perpendicular (este/oeste)

        case WindDirection::SOUTH: // Viento empuja hacia el sur (dr positivo)
            if (dr > 0) return 1.3;
            if (dr < 0) return 0.3;
            return 0.7;

        case WindDirection::EAST:  // Viento empuja hacia el este (dc positivo)
            if (dc > 0) return 1.3;
            if (dc < 0) return 0.3;
            return 0.7;
        case WindDirection::WEST:  // Viento empuja hacia el oeste (dc negativo)
            if (dc < 0) return 1.3;
            if (dc > 0) return 0.3;
            return 0.7;

        default:
            return 1.0;
    }
}

// ============================================================
// Probabilidad de ignición con viento
//
// Cada vecino ardiente contribuye independientemente.
// La probabilidad de NO encenderse por un vecino es:
//   (1 - base * windMod)
// La probabilidad acumulada de encenderse es:
//   1 - Π(1 - base * windMod_i)
// ============================================================
static double ignitionProbabilityWithWind(
    const Matrix &mat, int candidateRow, int candidateCol, WindDirection wind
) {
    CellType type = mat[candidateRow][candidateCol];
    double base = baseIgnitionProbability(type);
    if (base <= 0.0) return 0.0;

    int rows = (int)mat.size();
    int cols = (int)mat[0].size();
    double survivalProbability = 1.0;
    bool hasBurningNeighbor = false;

    for (auto [nr, nc] : neighbors4(candidateRow, candidateCol, rows, cols)) {
        if (mat[nr][nc] == BURNING) {
            hasBurningNeighbor = true;
            double mod = windModifier(nr, nc, candidateRow, candidateCol, wind);
            double adjustedBase = std::min(base * mod, 1.0);  // Clamp a [0, 1]
            survivalProbability *= (1.0 - adjustedBase);
        }
    }

    if (!hasBurningNeighbor) return 0.0;
    return 1.0 - survivalProbability;
}

// ============================================================
// Sorteo determinístico de ignición (idéntico a la versión paralela)
// ============================================================
static uint64_t splitMix64(uint64_t value) {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

double ignitionDraw(unsigned int seed, int iteration, int globalRow, int globalCol) {
    uint64_t value = seed;
    value ^= splitMix64((uint64_t)(uint32_t)iteration);
    value ^= splitMix64(((uint64_t)(uint32_t)globalRow << 32) | (uint32_t)globalCol);
    value = splitMix64(value);

    return (value >> 11) * (1.0 / 9007199254740992.0);
}

// ============================================================
// Funciones de fuego
// ============================================================
std::optional<Position> chooseInitialFire(Matrix &mat, unsigned int seed) {
    std::vector<Position> burnableCells;

    for (int r = 0; r < (int)mat.size(); ++r) {
        for (int c = 0; c < (int)mat[0].size(); ++c) {
            if (canIgnite(mat[r][c]))
                burnableCells.push_back({r, c});
        }
    }

    if (burnableCells.empty())
        return std::nullopt;

    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> randomIndex(0, (int)burnableCells.size() - 1);
    Position start = burnableCells[randomIndex(rng)];

    mat[start.row][start.col] = BURNING;
    return start;
}

std::vector<Position> chooseMultipleFires(Matrix &mat, unsigned int seed, int count) {
    std::vector<Position> burnableCells;

    for (int r = 0; r < (int)mat.size(); ++r) {
        for (int c = 0; c < (int)mat[0].size(); ++c) {
            if (canIgnite(mat[r][c]))
                burnableCells.push_back({r, c});
        }
    }

    std::vector<Position> fires;
    if (burnableCells.empty() || count <= 0)
        return fires;

    count = std::min(count, (int)burnableCells.size());

    // Fisher-Yates partial shuffle para elegir 'count' celdas distintas
    std::mt19937 rng(seed);
    for (int i = 0; i < count; ++i) {
        std::uniform_int_distribution<int> dist(i, (int)burnableCells.size() - 1);
        int j = dist(rng);
        std::swap(burnableCells[i], burnableCells[j]);

        Position &chosen = burnableCells[i];
        mat[chosen.row][chosen.col] = BURNING;
        fires.push_back(chosen);
    }

    return fires;
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

int advanceFire(Matrix &mat, FireState &fireState, unsigned int seed, int iteration, WindDirection wind) {
    int rows = (int)mat.size();
    int cols = (int)mat[0].size();

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
        double ignitionChance = ignitionProbabilityWithWind(mat, cell.row, cell.col, wind);

        if (ignitionDraw(seed, iteration, cell.row, cell.col) <= ignitionChance)
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

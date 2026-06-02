#include "mpi_fire.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <random>
#include <unordered_set>

namespace {

constexpr int kBlockTag = 100;
constexpr int kGatherTag = 200;
constexpr int kTopTag = 301;
constexpr int kBottomTag = 302;
constexpr int kLeftTag = 303;
constexpr int kRightTag = 304;

struct Halos {
    std::vector<CellType> top;
    std::vector<CellType> bottom;
    std::vector<CellType> left;
    std::vector<CellType> right;
};

BlockSpec makeBlockSpec(int globalRows, int globalCols, MPI_Comm cartComm, int cartRank) {
    int dims[2] = {};
    int periods[2] = {};
    int coords[2] = {};

    MPI_Cart_get(cartComm, 2, dims, periods, coords);
    MPI_Cart_coords(cartComm, cartRank, 2, coords);

    int baseRows = globalRows / dims[0];
    int extraRows = globalRows % dims[0];
    int baseCols = globalCols / dims[1];
    int extraCols = globalCols % dims[1];

    BlockSpec spec{};
    spec.rows = baseRows + (coords[0] < extraRows ? 1 : 0);
    spec.cols = baseCols + (coords[1] < extraCols ? 1 : 0);
    spec.rowOffset = coords[0] * baseRows + std::min(coords[0], extraRows);
    spec.colOffset = coords[1] * baseCols + std::min(coords[1], extraCols);
    return spec;
}

std::vector<int> packCells(const LocalBlock &block) {
    std::vector<int> packed;
    packed.reserve(block.cells.size());
    for (CellType cell : block.cells)
        packed.push_back((int)cell);
    return packed;
}

std::vector<int> packEnvironmentBlock(const Matrix &environment, const BlockSpec &spec) {
    std::vector<int> packed;
    packed.reserve((size_t)spec.rows * spec.cols);

    for (int r = 0; r < spec.rows; ++r) {
        for (int c = 0; c < spec.cols; ++c) {
            packed.push_back((int)environment[spec.rowOffset + r][spec.colOffset + c]);
        }
    }

    return packed;
}

void unpackCells(LocalBlock &block, const std::vector<int> &packed) {
    block.cells.resize(packed.size());
    for (size_t i = 0; i < packed.size(); ++i)
        block.cells[i] = (CellType)packed[i];
}

void unpackEnvironmentBlock(Matrix &environment, const BlockSpec &spec, const std::vector<int> &packed) {
    size_t index = 0;
    for (int r = 0; r < spec.rows; ++r) {
        for (int c = 0; c < spec.cols; ++c) {
            environment[spec.rowOffset + r][spec.colOffset + c] = (CellType)packed[index++];
        }
    }
}

bool contains(const LocalBlock &block, int globalRow, int globalCol) {
    return globalRow >= block.spec.rowOffset &&
           globalRow < block.spec.rowOffset + block.spec.rows &&
           globalCol >= block.spec.colOffset &&
           globalCol < block.spec.colOffset + block.spec.cols;
}

bool canIgnite(CellType cell) {
    return cell == FOREST || cell == CITY;
}

double baseIgnitionProbability(CellType cell) {
    switch (cell) {
        case WATER:   return 0.0;
        case FOREST:  return 0.42;
        case CITY:    return 0.24;
        case BURNING: return 0.0;
        case ASH:     return 0.0;
    }
    return 0.0;
}

double windModifier(int fireRow, int fireCol, int candidateRow, int candidateCol, WindDirection wind) {
    if (wind == WindDirection::NONE)
        return 1.0;

    int dr = candidateRow - fireRow;
    int dc = candidateCol - fireCol;

    switch (wind) {
        case WindDirection::NORTH:
            if (dr < 0) return 1.3;
            if (dr > 0) return 0.3;
            return 0.7;
        case WindDirection::SOUTH:
            if (dr > 0) return 1.3;
            if (dr < 0) return 0.3;
            return 0.7;
        case WindDirection::EAST:
            if (dc > 0) return 1.3;
            if (dc < 0) return 0.3;
            return 0.7;
        case WindDirection::WEST:
            if (dc < 0) return 1.3;
            if (dc > 0) return 0.3;
            return 0.7;
        case WindDirection::NONE:
            return 1.0;
    }
    return 1.0;
}

uint64_t splitMix64(uint64_t value) {
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

std::vector<int> packTop(const LocalBlock &block) {
    std::vector<int> edge(block.spec.cols);
    for (int c = 0; c < block.spec.cols; ++c)
        edge[c] = (int)block.at(0, c);
    return edge;
}

std::vector<int> packBottom(const LocalBlock &block) {
    std::vector<int> edge(block.spec.cols);
    for (int c = 0; c < block.spec.cols; ++c)
        edge[c] = (int)block.at(block.spec.rows - 1, c);
    return edge;
}

std::vector<int> packLeft(const LocalBlock &block) {
    std::vector<int> edge(block.spec.rows);
    for (int r = 0; r < block.spec.rows; ++r)
        edge[r] = (int)block.at(r, 0);
    return edge;
}

std::vector<int> packRight(const LocalBlock &block) {
    std::vector<int> edge(block.spec.rows);
    for (int r = 0; r < block.spec.rows; ++r)
        edge[r] = (int)block.at(r, block.spec.cols - 1);
    return edge;
}

void convertHalo(const std::vector<int> &packed, std::vector<CellType> &halo) {
    halo.resize(packed.size());
    for (size_t i = 0; i < packed.size(); ++i)
        halo[i] = (CellType)packed[i];
}

Halos exchangeHalos(const LocalBlock &block, MPI_Comm cartComm) {
    int up = MPI_PROC_NULL;
    int down = MPI_PROC_NULL;
    int left = MPI_PROC_NULL;
    int right = MPI_PROC_NULL;
    MPI_Cart_shift(cartComm, 0, 1, &up, &down);
    MPI_Cart_shift(cartComm, 1, 1, &left, &right);

    std::vector<int> topSend = packTop(block);
    std::vector<int> bottomSend = packBottom(block);
    std::vector<int> leftSend = packLeft(block);
    std::vector<int> rightSend = packRight(block);
    std::vector<int> topRecv(block.spec.cols, (int)WATER);
    std::vector<int> bottomRecv(block.spec.cols, (int)WATER);
    std::vector<int> leftRecv(block.spec.rows, (int)WATER);
    std::vector<int> rightRecv(block.spec.rows, (int)WATER);

    MPI_Sendrecv(
        topSend.data(), (int)topSend.size(), MPI_INT, up, kTopTag,
        topRecv.data(), (int)topRecv.size(), MPI_INT, up, kBottomTag,
        cartComm, MPI_STATUS_IGNORE
    );
    MPI_Sendrecv(
        bottomSend.data(), (int)bottomSend.size(), MPI_INT, down, kBottomTag,
        bottomRecv.data(), (int)bottomRecv.size(), MPI_INT, down, kTopTag,
        cartComm, MPI_STATUS_IGNORE
    );
    MPI_Sendrecv(
        leftSend.data(), (int)leftSend.size(), MPI_INT, left, kLeftTag,
        leftRecv.data(), (int)leftRecv.size(), MPI_INT, left, kRightTag,
        cartComm, MPI_STATUS_IGNORE
    );
    MPI_Sendrecv(
        rightSend.data(), (int)rightSend.size(), MPI_INT, right, kRightTag,
        rightRecv.data(), (int)rightRecv.size(), MPI_INT, right, kLeftTag,
        cartComm, MPI_STATUS_IGNORE
    );

    Halos halos;
    convertHalo(topRecv, halos.top);
    convertHalo(bottomRecv, halos.bottom);
    convertHalo(leftRecv, halos.left);
    convertHalo(rightRecv, halos.right);
    return halos;
}

CellType neighborState(const LocalBlock &block, const Halos &halos, int row, int col) {
    if (row >= 0 && row < block.spec.rows && col >= 0 && col < block.spec.cols)
        return block.at(row, col);
    if (row == -1 && col >= 0 && col < block.spec.cols)
        return halos.top[col];
    if (row == block.spec.rows && col >= 0 && col < block.spec.cols)
        return halos.bottom[col];
    if (col == -1 && row >= 0 && row < block.spec.rows)
        return halos.left[row];
    if (col == block.spec.cols && row >= 0 && row < block.spec.rows)
        return halos.right[row];
    return WATER;
}

void addCandidate(
    const LocalBlock &block,
    int row,
    int col,
    std::unordered_set<int64_t> &visited,
    std::vector<Position> &candidates
) {
    if (row < 0 || row >= block.spec.rows || col < 0 || col >= block.spec.cols)
        return;
    if (!canIgnite(block.at(row, col)))
        return;

    int64_t key = (int64_t)row * block.spec.cols + col;
    if (visited.insert(key).second)
        candidates.push_back({row, col});
}

void markLocalCandidates(
    const LocalBlock &block,
    const std::vector<Position> &burningCells,
    std::unordered_set<int64_t> &visited,
    std::vector<Position> &candidates
) {
    for (const Position &cell : burningCells) {
        addCandidate(block, cell.row - 1, cell.col, visited, candidates);
        addCandidate(block, cell.row + 1, cell.col, visited, candidates);
        addCandidate(block, cell.row, cell.col - 1, visited, candidates);
        addCandidate(block, cell.row, cell.col + 1, visited, candidates);
    }
}

void markHaloCandidates(
    const LocalBlock &block,
    const Halos &halos,
    std::unordered_set<int64_t> &visited,
    std::vector<Position> &candidates
) {
    for (int c = 0; c < block.spec.cols; ++c) {
        if (halos.top[c] == BURNING)
            addCandidate(block, 0, c, visited, candidates);
        if (halos.bottom[c] == BURNING)
            addCandidate(block, block.spec.rows - 1, c, visited, candidates);
    }

    for (int r = 0; r < block.spec.rows; ++r) {
        if (halos.left[r] == BURNING)
            addCandidate(block, r, 0, visited, candidates);
        if (halos.right[r] == BURNING)
            addCandidate(block, r, block.spec.cols - 1, visited, candidates);
    }
}

double ignitionProbability(
    const LocalBlock &block,
    const Halos &halos,
    Position candidate,
    WindDirection wind
) {
    double base = baseIgnitionProbability(block.at(candidate.row, candidate.col));
    if (base <= 0.0)
        return 0.0;

    const int dr[] = {-1, 1, 0, 0};
    const int dc[] = {0, 0, -1, 1};
    int candidateGlobalRow = block.spec.rowOffset + candidate.row;
    int candidateGlobalCol = block.spec.colOffset + candidate.col;
    double survivalProbability = 1.0;
    bool hasBurningNeighbor = false;

    for (int direction = 0; direction < 4; ++direction) {
        int neighborRow = candidate.row + dr[direction];
        int neighborCol = candidate.col + dc[direction];
        if (neighborState(block, halos, neighborRow, neighborCol) != BURNING)
            continue;

        hasBurningNeighbor = true;
        int fireGlobalRow = candidateGlobalRow + dr[direction];
        int fireGlobalCol = candidateGlobalCol + dc[direction];
        double adjusted = std::min(
            base * windModifier(fireGlobalRow, fireGlobalCol, candidateGlobalRow, candidateGlobalCol, wind),
            1.0
        );
        survivalProbability *= (1.0 - adjusted);
    }

    return hasBurningNeighbor ? 1.0 - survivalProbability : 0.0;
}

} // namespace

CellType &LocalBlock::at(int row, int col) {
    return cells[(size_t)row * spec.cols + col];
}

CellType LocalBlock::at(int row, int col) const {
    return cells[(size_t)row * spec.cols + col];
}

WindDirection parseWindDirection(const char *value) {
    if (!value)
        return WindDirection::NONE;
    if (std::strcmp(value, "north") == 0 || std::strcmp(value, "N") == 0)
        return WindDirection::NORTH;
    if (std::strcmp(value, "south") == 0 || std::strcmp(value, "S") == 0)
        return WindDirection::SOUTH;
    if (std::strcmp(value, "east") == 0 || std::strcmp(value, "E") == 0)
        return WindDirection::EAST;
    if (std::strcmp(value, "west") == 0 || std::strcmp(value, "W") == 0)
        return WindDirection::WEST;
    return WindDirection::NONE;
}

const char *windDirectionName(WindDirection wind) {
    switch (wind) {
        case WindDirection::NORTH: return "Norte";
        case WindDirection::SOUTH: return "Sur";
        case WindDirection::EAST:  return "Este";
        case WindDirection::WEST:  return "Oeste";
        case WindDirection::NONE:  return "Sin viento";
    }
    return "Sin viento";
}

std::optional<Position> chooseInitialFire(Matrix &environment, unsigned int seed) {
    std::vector<Position> burnableCells;
    for (int r = 0; r < (int)environment.size(); ++r) {
        for (int c = 0; c < (int)environment[0].size(); ++c) {
            if (canIgnite(environment[r][c]))
                burnableCells.push_back({r, c});
        }
    }

    if (burnableCells.empty())
        return std::nullopt;

    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> randomIndex(0, (int)burnableCells.size() - 1);
    Position initialFire = burnableCells[randomIndex(rng)];
    environment[initialFire.row][initialFire.col] = BURNING;
    return initialFire;
}

std::vector<Position> chooseMultipleFires(Matrix &environment, unsigned int seed, int count) {
    std::vector<Position> burnableCells;
    for (int r = 0; r < (int)environment.size(); ++r) {
        for (int c = 0; c < (int)environment[0].size(); ++c) {
            if (canIgnite(environment[r][c]))
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
        environment[chosen.row][chosen.col] = BURNING;
        fires.push_back(chosen);
    }

    return fires;
}

LocalBlock scatterEnvironment(
    const Matrix *environment,
    int globalRows,
    int globalCols,
    MPI_Comm cartComm
) {
    int rank = 0;
    int size = 0;
    MPI_Comm_rank(cartComm, &rank);
    MPI_Comm_size(cartComm, &size);

    LocalBlock localBlock;
    localBlock.spec = makeBlockSpec(globalRows, globalCols, cartComm, rank);

    if (rank == 0) {
        for (int target = 0; target < size; ++target) {
            BlockSpec targetSpec = makeBlockSpec(globalRows, globalCols, cartComm, target);
            std::vector<int> packed = packEnvironmentBlock(*environment, targetSpec);

            if (target == 0) {
                unpackCells(localBlock, packed);
            } else {
                MPI_Send(packed.data(), (int)packed.size(), MPI_INT, target, kBlockTag, cartComm);
            }
        }
    } else {
        std::vector<int> packed((size_t)localBlock.spec.rows * localBlock.spec.cols);
        MPI_Recv(packed.data(), (int)packed.size(), MPI_INT, 0, kBlockTag, cartComm, MPI_STATUS_IGNORE);
        unpackCells(localBlock, packed);
    }

    return localBlock;
}

Matrix gatherEnvironment(
    const LocalBlock &localBlock,
    int globalRows,
    int globalCols,
    MPI_Comm cartComm
) {
    int rank = 0;
    int size = 0;
    MPI_Comm_rank(cartComm, &rank);
    MPI_Comm_size(cartComm, &size);

    std::vector<int> localPacked = packCells(localBlock);
    Matrix environment;

    if (rank == 0) {
        environment.assign(globalRows, std::vector<CellType>(globalCols, WATER));
        unpackEnvironmentBlock(environment, localBlock.spec, localPacked);

        for (int source = 1; source < size; ++source) {
            BlockSpec sourceSpec = makeBlockSpec(globalRows, globalCols, cartComm, source);
            std::vector<int> packed((size_t)sourceSpec.rows * sourceSpec.cols);
            MPI_Recv(packed.data(), (int)packed.size(), MPI_INT, source, kGatherTag, cartComm, MPI_STATUS_IGNORE);
            unpackEnvironmentBlock(environment, sourceSpec, packed);
        }
    } else {
        MPI_Send(localPacked.data(), (int)localPacked.size(), MPI_INT, 0, kGatherTag, cartComm);
    }

    return environment;
}

int initializeLocalFire(LocalBlock &localBlock, FireState &fireState, Position initialFire) {
    if (!contains(localBlock, initialFire.row, initialFire.col))
        return 0;

    Position localFire{
        initialFire.row - localBlock.spec.rowOffset,
        initialFire.col - localBlock.spec.colOffset
    };
    localBlock.at(localFire.row, localFire.col) = BURNING;
    fireState.listaFuego1.push_back(localFire);
    return 1;
}

int initializeLocalFires(LocalBlock &localBlock, FireState &fireState, const std::vector<Position> &fires) {
    int count = 0;
    for (const Position &fire : fires)
        count += initializeLocalFire(localBlock, fireState, fire);
    return count;
}

int advanceParallelFire(
    LocalBlock &localBlock,
    FireState &fireState,
    unsigned int seed,
    int iteration,
    WindDirection wind,
    MPI_Comm cartComm
) {
    Halos halos = exchangeHalos(localBlock, cartComm);

    std::unordered_set<int64_t> visited;
    std::vector<Position> candidates;
    candidates.reserve(
        4 * (
            fireState.listaFuego1.size() +
            fireState.listaFuego2.size() +
            fireState.listaFuego3.size()
        ) +
        2 * (localBlock.spec.rows + localBlock.spec.cols)
    );
    visited.reserve(candidates.capacity());

    markLocalCandidates(localBlock, fireState.listaFuego1, visited, candidates);
    markLocalCandidates(localBlock, fireState.listaFuego2, visited, candidates);
    markLocalCandidates(localBlock, fireState.listaFuego3, visited, candidates);
    markHaloCandidates(localBlock, halos, visited, candidates);

    std::vector<Position> newFires;
    for (const Position &cell : candidates) {
        double chance = ignitionProbability(localBlock, halos, cell, wind);
        int globalRow = localBlock.spec.rowOffset + cell.row;
        int globalCol = localBlock.spec.colOffset + cell.col;
        if (ignitionDraw(seed, iteration, globalRow, globalCol) <= chance)
            newFires.push_back(cell);
    }

    for (const Position &cell : fireState.listaFuego3)
        localBlock.at(cell.row, cell.col) = ASH;

    fireState.listaFuego3 = std::move(fireState.listaFuego2);
    fireState.listaFuego2 = std::move(fireState.listaFuego1);
    fireState.listaFuego1 = std::move(newFires);

    for (const Position &cell : fireState.listaFuego1)
        localBlock.at(cell.row, cell.col) = BURNING;

    return (int)fireState.listaFuego1.size();
}

int countActiveFires(const FireState &fireState) {
    return (int)(
        fireState.listaFuego1.size() +
        fireState.listaFuego2.size() +
        fireState.listaFuego3.size()
    );
}

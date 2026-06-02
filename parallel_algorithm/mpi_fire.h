#pragma once

#include "../matrix_creator/environment.h"

#include <mpi.h>

#include <optional>
#include <string>
#include <vector>

enum class WindDirection {
    NONE,
    NORTH,
    SOUTH,
    EAST,
    WEST
};

struct Position {
    int row;
    int col;
};

struct BlockSpec {
    int rowOffset;
    int colOffset;
    int rows;
    int cols;
};

struct LocalBlock {
    BlockSpec spec;
    std::vector<CellType> cells;

    CellType &at(int row, int col);
    CellType at(int row, int col) const;
};

struct FireState {
    std::vector<Position> listaFuego1;
    std::vector<Position> listaFuego2;
    std::vector<Position> listaFuego3;
};

WindDirection parseWindDirection(const char *value);
const char *windDirectionName(WindDirection wind);
std::optional<Position> chooseInitialFire(Matrix &environment, unsigned int seed);
std::vector<Position> chooseMultipleFires(Matrix &environment, unsigned int seed, int count);

LocalBlock scatterEnvironment(
    const Matrix *environment,
    int globalRows,
    int globalCols,
    MPI_Comm cartComm
);

Matrix gatherEnvironment(
    const LocalBlock &localBlock,
    int globalRows,
    int globalCols,
    MPI_Comm cartComm
);

int initializeLocalFire(LocalBlock &localBlock, FireState &fireState, Position initialFire);
int initializeLocalFires(LocalBlock &localBlock, FireState &fireState, const std::vector<Position> &fires);
int advanceParallelFire(
    LocalBlock &localBlock,
    FireState &fireState,
    unsigned int seed,
    int iteration,
    WindDirection wind,
    MPI_Comm cartComm
);
int countActiveFires(const FireState &fireState);

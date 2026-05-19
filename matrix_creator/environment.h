#pragma once

#include <utility>
#include <vector>

enum CellType {
    WATER    = 0,
    FOREST   = 1,
    CITY     = 2,
    BURNING  = 3,
    ASH      = 4
};

using Matrix = std::vector<std::vector<CellType>>;

std::vector<std::pair<int, int>> neighbors4(int row, int col, int rows, int cols);
Matrix createEnvironment(int rows, int cols, unsigned int seed = 0);
void showMatrixSDL(const Matrix &mat, int cellSize = 0);

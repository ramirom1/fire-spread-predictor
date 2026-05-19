#include "environment.h"

#include <ctime>
#include <iostream>

static void printEnvironmentStats(const Matrix &env) {
    int counts[5] = {};
    for (const auto &row : env)
        for (CellType cell : row)
            counts[cell]++;

    int rows = (int)env.size();
    int cols = (int)env[0].size();
    int total = rows * cols;

    std::cout << "Agua:   " << counts[WATER]  << " (" << 100.0 * counts[WATER]  / total << "%)\n";
    std::cout << "Bosque: " << counts[FOREST] << " (" << 100.0 * counts[FOREST] / total << "%)\n";
    std::cout << "Ciudad: " << counts[CITY]   << " (" << 100.0 * counts[CITY]   / total << "%)\n";
}

int main() {
    const int ROWS = 500;
    const int COLS = 500;
    unsigned int seed = (unsigned int)std::time(nullptr);

    std::cout << "Generando entorno " << ROWS << "x" << COLS << "..." << std::endl;
    Matrix env = createEnvironment(ROWS, COLS, seed);

    printEnvironmentStats(env);

    std::cout << "Abriendo ventana SDL2 con el mapa generado... (Cerrar con ESC o botón X)" << std::endl;
    showMatrixSDL(env);

    return 0;
}

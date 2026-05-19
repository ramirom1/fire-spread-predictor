#include "sequential_fire.h"

#include <chrono>
#include <ctime>
#include <iostream>
#include <random>

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
    const int MAX_ITERATIONS = 100;
    unsigned int seed = (unsigned int)std::time(nullptr);
    std::mt19937 rng(seed);

    std::cout << "Generando entorno " << ROWS << "x" << COLS << "..." << std::endl;
    Matrix env = createEnvironment(ROWS, COLS, seed);

    printEnvironmentStats(env);

    FireState fireState;
    std::optional<Position> initialFire = igniteRandomCell(env, fireState, rng);
    if (!initialFire) {
        std::cerr << "No hay casillas combustibles para iniciar el fuego." << std::endl;
        return 1;
    }

    std::cout << "Fuego inicial: (" << initialFire->row << ", " << initialFire->col << ")\n";
    std::cout << "Simulando " << MAX_ITERATIONS << " iteraciones..." << std::endl;

    auto inicio = std::chrono::high_resolution_clock::now();
    for (int iteration = 0; iteration < MAX_ITERATIONS; ++iteration) {
        int newFires = advanceFire(env, fireState, rng);
        std::cout << "Iteracion " << iteration
                  << " | nuevos fuegos: " << newFires
                  << " | fuegos activos: " << countActiveFires(fireState)
                  << "\n";

        if (countActiveFires(fireState) == 0) {
            std::cout << "El fuego se extinguio en la iteracion " << iteration << ".\n";
            break;
        }
    }

    auto fin = std::chrono::high_resolution_clock::now();
    auto duracion = std::chrono::duration_cast<std::chrono::milliseconds>(fin - inicio);

    std::cout << "Tiempo: " << duracion.count() << " ms" << std::endl;

    std::cout << "Abriendo ventana SDL2 con el estado final... (Cerrar con ESC o botón X)" << std::endl;
    showMatrixSDL(env);

    return 0;
}

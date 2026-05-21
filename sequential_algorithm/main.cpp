#include "sequential_fire.h"
#include "animated_view.h"

#include <chrono>
#include <ctime>
#include <iostream>
#include <random>
#include <string>

static void printUsage(const char *programName) {
    std::cout << "Uso: " << programName << " [opciones]\n"
              << "\n"
              << "Opciones:\n"
              << "  --wind <N|S|E|W>   Dirección del viento (defecto: sin viento)\n"
              << "  --rows <n>         Filas de la grilla (defecto: 500)\n"
              << "  --cols <n>         Columnas de la grilla (defecto: 500)\n"
              << "  --batch            Modo batch: ejecuta todo y muestra resultado final\n"
              << "  --no-gui           Sin visualización: solo ejecuta y mide tiempo\n"
              << "  --help, -h         Muestra esta ayuda\n"
              << "\n"
              << "Modo animado (defecto):\n"
              << "  SPACE        – Play / Pause\n"
              << "  S            – Single step\n"
              << "  R            – Reset (nuevo mapa aleatorio)\n"
              << "  UP / DOWN    – Aumentar / Reducir velocidad\n"
              << "  Scroll       – Zoom (centrado en cursor)\n"
              << "  Click+Drag   – Pan\n"
              << "  +/-          – Zoom (centrado en pantalla)\n"
              << "  0            – Reset zoom (ver todo)\n"
              << "  ESC          – Salir\n";
}

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

static const char* windDirectionName(WindDirection wind) {
    switch (wind) {
        case WindDirection::NORTH: return "Norte";
        case WindDirection::SOUTH: return "Sur";
        case WindDirection::EAST:  return "Este";
        case WindDirection::WEST:  return "Oeste";
        default:                   return "Sin viento";
    }
}

// ════════════════════════════════════════════════════════════════
// Modo batch (comportamiento original)
// ════════════════════════════════════════════════════════════════
static int runBatch(int rows, int cols, unsigned int seed, WindDirection wind, bool showGui) {
    const int MAX_ITERATIONS = 50000;
    std::mt19937 rng(seed);

    std::cout << "Generando entorno " << rows << "x" << cols << "..." << std::endl;
    Matrix env = createEnvironment(rows, cols, seed);

    printEnvironmentStats(env);
    std::cout << "Viento: " << windDirectionName(wind) << std::endl;

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
        int newFires = advanceFire(env, fireState, rng, wind);
        //std::cout << "Iteracion " << iteration
        //          << " | nuevos fuegos: " << newFires
        //          << " | fuegos activos: " << countActiveFires(fireState)
        //          << "\n";

        if (countActiveFires(fireState) == 0) {
            std::cout << "El fuego se extinguio en la iteracion " << iteration << ".\n";
            break;
        }
    }

    auto fin = std::chrono::high_resolution_clock::now();
    auto duracion = std::chrono::duration_cast<std::chrono::milliseconds>(fin - inicio);
    std::cout << "Tiempo: " << duracion.count() << " ms" << std::endl;

    if (showGui) {
        std::cout << "Abriendo ventana SDL2 con el estado final... (Cerrar con ESC o botón X)" << std::endl;
        showMatrixSDL(env);
    }

    return 0;
}

// ════════════════════════════════════════════════════════════════
// Main
// ════════════════════════════════════════════════════════════════
int main(int argc, char *argv[]) {
    int rows = 500;
    int cols = 500;
    unsigned int seed = (unsigned int)std::time(nullptr);
    WindDirection wind = WindDirection::NONE;
    bool batchMode = false;
    bool noGui = false;

    // Parsear argumentos
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--wind" || arg == "-w") && i + 1 < argc) {
            wind = parseWindDirection(argv[++i]);
        } else if (arg == "--rows" && i + 1 < argc) {
            rows = std::stoi(argv[++i]);
        } else if (arg == "--cols" && i + 1 < argc) {
            cols = std::stoi(argv[++i]);
        } else if (arg == "--batch") {
            batchMode = true;
        } else if (arg == "--no-gui") {
            noGui = true;
            batchMode = true;
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
    }

    if (batchMode) {
        return runBatch(rows, cols, seed, wind, !noGui);
    }

    // Modo animado (defecto)
    std::cout << "Grilla: " << rows << "x" << cols << "\n";
    std::cout << "Viento: " << windDirectionName(wind) << "\n";

    AnimatedView view(rows, cols, seed, wind);
    view.run();

    return 0;
}

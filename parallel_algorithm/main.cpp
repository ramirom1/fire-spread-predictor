#include "mpi_fire.h"

#include <ctime>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

struct Options {
    int rows = 10000;
    int cols = 10000;
    int iterations = 50000;
    unsigned int seed = 0;
    bool showWindow = true;
    bool help = false;
    WindDirection wind = WindDirection::NONE;
};

int parsePositiveInt(const char *value, int fallback) {
    if (!value)
        return fallback;
    int parsed = std::atoi(value);
    return parsed > 0 ? parsed : fallback;
}

Options parseOptions(int argc, char *argv[]) {
    Options options;
    options.seed = (unsigned int)std::time(nullptr);

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--wind" || arg == "-w") && i + 1 < argc) {
            options.wind = parseWindDirection(argv[++i]);
        } else if (arg == "--rows" && i + 1 < argc) {
            options.rows = parsePositiveInt(argv[++i], options.rows);
        } else if (arg == "--cols" && i + 1 < argc) {
            options.cols = parsePositiveInt(argv[++i], options.cols);
        } else if (arg == "--iterations" && i + 1 < argc) {
            options.iterations = parsePositiveInt(argv[++i], options.iterations);
        } else if (arg == "--seed" && i + 1 < argc) {
            options.seed = (unsigned int)std::strtoul(argv[++i], nullptr, 10);
        } else if (arg == "--no-window") {
            options.showWindow = false;
        } else if (arg == "--help" || arg == "-h") {
            options.help = true;
        }
    }

    return options;
}

void printUsage(const char *programName) {
    std::cout << "Uso: mpirun -np <procesos> " << programName << " [opciones]\n"
              << "  --rows <n>        Filas del mapa. Valor por defecto: 500\n"
              << "  --cols <n>        Columnas del mapa. Valor por defecto: 500\n"
              << "  --iterations <n>  Iteraciones maximas. Valor por defecto: 200\n"
              << "  --seed <n>        Semilla del mapa y decisiones de fuego\n"
              << "  --wind <N|S|E|W>  Direccion del viento; sin opcion corre sin viento\n"
              << "  --no-window       No abre SDL al final\n";
}

void printEnvironmentStats(const Matrix &environment) {
    int counts[5] = {};
    for (const auto &row : environment)
        for (CellType cell : row)
            counts[cell]++;

    int total = (int)environment.size() * (int)environment[0].size();
    std::cout << "Agua:   " << counts[WATER]  << " (" << 100.0 * counts[WATER]  / total << "%)\n";
    std::cout << "Bosque: " << counts[FOREST] << " (" << 100.0 * counts[FOREST] / total << "%)\n";
    std::cout << "Ciudad: " << counts[CITY]   << " (" << 100.0 * counts[CITY]   / total << "%)\n";
}

} // namespace

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int worldRank = 0;
    int worldSize = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &worldRank);
    MPI_Comm_size(MPI_COMM_WORLD, &worldSize);

    Options options = parseOptions(argc, argv);
    if (options.help) {
        if (worldRank == 0)
            printUsage(argv[0]);
        MPI_Finalize();
        return 0;
    }

    int sharedOptions[5] = {
        options.rows,
        options.cols,
        options.iterations,
        options.showWindow ? 1 : 0,
        (int)options.wind
    };
    MPI_Bcast(sharedOptions, 5, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&options.seed, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
    options.rows = sharedOptions[0];
    options.cols = sharedOptions[1];
    options.iterations = sharedOptions[2];
    options.showWindow = sharedOptions[3] != 0;
    options.wind = (WindDirection)sharedOptions[4];

    int dims[2] = {};
    MPI_Dims_create(worldSize, 2, dims);
    int periods[2] = {0, 0};
    MPI_Comm cartComm = MPI_COMM_NULL;
    MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, 0, &cartComm);

    int rank = 0;
    MPI_Comm_rank(cartComm, &rank);

    int invalidGrid = dims[0] > options.rows || dims[1] > options.cols;
    if (invalidGrid) {
        if (rank == 0) {
            std::cerr << "La grilla MPI " << dims[0] << "x" << dims[1]
                      << " no cabe en un mapa " << options.rows << "x" << options.cols << ".\n";
        }
        MPI_Comm_free(&cartComm);
        MPI_Finalize();
        return 1;
    }

    Matrix environment;
    Position initialFire{-1, -1};
    int hasInitialFire = 0;

    if (rank == 0) {
        std::cout << "Generando entorno " << options.rows << "x" << options.cols
                  << " para " << worldSize << " procesos..." << std::endl;
        environment = createEnvironment(options.rows, options.cols, options.seed);
        printEnvironmentStats(environment);

        std::optional<Position> start = chooseInitialFire(environment, options.seed);
        if (start) {
            initialFire = *start;
            hasInitialFire = 1;
            std::cout << "Fuego inicial: (" << initialFire.row << ", " << initialFire.col << ")\n";
            std::cout << "Viento: " << windDirectionName(options.wind) << "\n";
            std::cout << "Bloques MPI: " << dims[0] << "x" << dims[1] << "\n";
        } else {
            std::cerr << "No hay casillas combustibles para iniciar el fuego." << std::endl;
        }
    }

    MPI_Bcast(&hasInitialFire, 1, MPI_INT, 0, cartComm);
    int initialCoordinates[2] = {initialFire.row, initialFire.col};
    MPI_Bcast(initialCoordinates, 2, MPI_INT, 0, cartComm);
    initialFire = {initialCoordinates[0], initialCoordinates[1]};

    if (!hasInitialFire) {
        MPI_Comm_free(&cartComm);
        MPI_Finalize();
        return 1;
    }

    LocalBlock localBlock = scatterEnvironment(
        rank == 0 ? &environment : nullptr,
        options.rows,
        options.cols,
        cartComm
    );
    FireState fireState;
    initializeLocalFire(localBlock, fireState, initialFire);

    if (rank == 0)
        std::cout << "Simulando " << options.iterations << " iteraciones..." << std::endl;

    MPI_Barrier(cartComm);
    double startTime = MPI_Wtime();
    for (int iteration = 0; iteration < options.iterations; ++iteration) {
        int localNewFires = advanceParallelFire(
            localBlock,
            fireState,
            options.seed,
            iteration,
            options.wind,
            cartComm
        );
        int localActiveFires = countActiveFires(fireState);
        int globalNewFires = 0;
        int globalActiveFires = 0;

        MPI_Allreduce(&localNewFires, &globalNewFires, 1, MPI_INT, MPI_SUM, cartComm);
        MPI_Allreduce(&localActiveFires, &globalActiveFires, 1, MPI_INT, MPI_SUM, cartComm);

        if (rank == 0) {
            std::cout << "Iteracion " << iteration
                      << " | nuevos fuegos: " << globalNewFires
                      << " | fuegos activos: " << globalActiveFires
                      << "\n";
        }

        if (globalActiveFires == 0) {
            if (rank == 0)
                std::cout << "El fuego se extinguio en la iteracion " << iteration << ".\n";
            break;
        }
    }

    double localElapsed = MPI_Wtime() - startTime;
    double elapsed = 0.0;
    MPI_Reduce(&localElapsed, &elapsed, 1, MPI_DOUBLE, MPI_MAX, 0, cartComm);

    Matrix finalEnvironment = gatherEnvironment(localBlock, options.rows, options.cols, cartComm);

    if (rank == 0) {
        std::cout << "Tiempo paralelo: " << elapsed * 1000.0 << " ms" << std::endl;
        if (options.showWindow) {
            std::cout << "Abriendo ventana SDL2 con el estado final... (Cerrar con ESC o boton X)" << std::endl;
            //showMatrixSDL(finalEnvironment);
        }
    }

    MPI_Comm_free(&cartComm);
    MPI_Finalize();
    return 0;
}

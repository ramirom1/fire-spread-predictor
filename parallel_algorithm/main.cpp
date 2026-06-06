#include "mpi_fire.h"

#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

struct Options {
    int rows = 10000;
    int cols = 10000;
    int iterations = 50000;
    int numFires = 1;
    unsigned int seed = 0;
    bool showWindow = true;
    bool showMetrics = false;
    bool help = false;
    WindDirection wind = WindDirection::NONE;
};

struct RankMetrics {
    long long iterations = 0;
    long long totalCandidates = 0;
    long long totalNewFires = 0;
    long long totalActiveFires = 0;
    long long maxCandidates = 0;
    long long maxActiveFires = 0;
    double totalIterationSeconds = 0.0;
    double totalHaloSeconds = 0.0;
    double totalCandidateSeconds = 0.0;
    double totalIgnitionSeconds = 0.0;
    double totalUpdateSeconds = 0.0;
    double totalReductionSeconds = 0.0;
    double maxIterationSeconds = 0.0;
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
        } else if (arg == "--fires" && i + 1 < argc) {
            options.numFires = parsePositiveInt(argv[++i], options.numFires);
        } else if (arg == "--seed" && i + 1 < argc) {
            options.seed = (unsigned int)std::strtoul(argv[++i], nullptr, 10);
        } else if (arg == "--no-window") {
            options.showWindow = false;
        } else if (arg == "--metrics") {
            options.showMetrics = true;
        } else if (arg == "--help" || arg == "-h") {
            options.help = true;
        }
    }

    return options;
}

void printUsage(const char *programName) {
    std::cout << "Uso: mpirun -np <procesos> " << programName << " [opciones]\n"
              << "  --rows <n>        Filas del mapa. Valor por defecto: 10000\n"
              << "  --cols <n>        Columnas del mapa. Valor por defecto: 10000\n"
              << "  --iterations <n>  Iteraciones maximas. Valor por defecto: 50000\n"
              << "  --fires <n>       Cantidad de focos iniciales. Valor por defecto: 1\n"
              << "  --seed <n>        Semilla del mapa y decisiones de fuego\n"
              << "  --wind <N|S|E|W>  Direccion del viento; sin opcion corre sin viento\n"
              << "  --no-window       No abre SDL al final\n"
              << "  --metrics         Imprime metricas por rank al finalizar\n";
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

double averageLongLong(long long total, long long count) {
    return count > 0 ? (double)total / (double)count : 0.0;
}

double averageSeconds(double total, long long count) {
    return count > 0 ? total / (double)count : 0.0;
}

void printMetricsReport(const RankMetrics &localMetrics, MPI_Comm cartComm) {
    int rank = 0;
    int size = 0;
    MPI_Comm_rank(cartComm, &rank);
    MPI_Comm_size(cartComm, &size);

    long long localCounts[6] = {
        localMetrics.iterations,
        localMetrics.totalCandidates,
        localMetrics.totalNewFires,
        localMetrics.totalActiveFires,
        localMetrics.maxCandidates,
        localMetrics.maxActiveFires
    };
    double localTimes[7] = {
        localMetrics.totalIterationSeconds,
        localMetrics.totalHaloSeconds,
        localMetrics.totalCandidateSeconds,
        localMetrics.totalIgnitionSeconds,
        localMetrics.totalUpdateSeconds,
        localMetrics.totalReductionSeconds,
        localMetrics.maxIterationSeconds
    };

    std::vector<long long> allCounts;
    std::vector<double> allTimes;
    if (rank == 0) {
        allCounts.resize((size_t)size * 6);
        allTimes.resize((size_t)size * 7);
    }

    MPI_Gather(
        localCounts, 6, MPI_LONG_LONG,
        rank == 0 ? allCounts.data() : nullptr, 6, MPI_LONG_LONG,
        0, cartComm
    );
    MPI_Gather(
        localTimes, 7, MPI_DOUBLE,
        rank == 0 ? allTimes.data() : nullptr, 7, MPI_DOUBLE,
        0, cartComm
    );

    if (rank != 0)
        return;

    long long totalCandidates = 0;
    long long maxTotalCandidates = 0;
    double totalIterationSeconds = 0.0;
    double maxTotalIterationSeconds = 0.0;
    double totalComputeSeconds = 0.0;
    double maxTotalComputeSeconds = 0.0;

    for (int r = 0; r < size; ++r) {
        long long rankCandidates = allCounts[(size_t)r * 6 + 1];
        double rankIterationSeconds = allTimes[(size_t)r * 7 + 0];
        double rankComputeSeconds =
            allTimes[(size_t)r * 7 + 2] +
            allTimes[(size_t)r * 7 + 3] +
            allTimes[(size_t)r * 7 + 4];
        totalCandidates += rankCandidates;
        maxTotalCandidates = std::max(maxTotalCandidates, rankCandidates);
        totalIterationSeconds += rankIterationSeconds;
        maxTotalIterationSeconds = std::max(maxTotalIterationSeconds, rankIterationSeconds);
        totalComputeSeconds += rankComputeSeconds;
        maxTotalComputeSeconds = std::max(maxTotalComputeSeconds, rankComputeSeconds);
    }

    double avgTotalCandidates = size > 0 ? (double)totalCandidates / (double)size : 0.0;
    double avgTotalIterationSeconds = size > 0 ? totalIterationSeconds / (double)size : 0.0;
    double avgTotalComputeSeconds = size > 0 ? totalComputeSeconds / (double)size : 0.0;
    double candidateImbalance = avgTotalCandidates > 0.0 ? maxTotalCandidates / avgTotalCandidates : 0.0;
    double timeImbalance = avgTotalIterationSeconds > 0.0 ? maxTotalIterationSeconds / avgTotalIterationSeconds : 0.0;
    double computeImbalance = avgTotalComputeSeconds > 0.0 ? maxTotalComputeSeconds / avgTotalComputeSeconds : 0.0;

    std::cout << "\nMetricas por rank:\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout
        << "rank"
        << " | iter"
        << " | avg_iter_ms"
        << " | avg_compute_ms"
        << " | avg_halo_ms"
        << " | avg_cand_ms"
        << " | avg_ign_ms"
        << " | avg_update_ms"
        << " | avg_reduce_ms"
        << " | avg_candidates"
        << " | max_candidates"
        << " | avg_active"
        << " | max_active"
        << " | total_new_fires"
        << "\n";

    for (int r = 0; r < size; ++r) {
        long long iterations = allCounts[(size_t)r * 6 + 0];
        long long candidates = allCounts[(size_t)r * 6 + 1];
        long long newFires = allCounts[(size_t)r * 6 + 2];
        long long activeFires = allCounts[(size_t)r * 6 + 3];
        long long maxCandidates = allCounts[(size_t)r * 6 + 4];
        long long maxActiveFires = allCounts[(size_t)r * 6 + 5];

        double iterationSeconds = allTimes[(size_t)r * 7 + 0];
        double haloSeconds = allTimes[(size_t)r * 7 + 1];
        double candidateSeconds = allTimes[(size_t)r * 7 + 2];
        double ignitionSeconds = allTimes[(size_t)r * 7 + 3];
        double updateSeconds = allTimes[(size_t)r * 7 + 4];
        double reductionSeconds = allTimes[(size_t)r * 7 + 5];
        double computeSeconds = candidateSeconds + ignitionSeconds + updateSeconds;

        std::cout
            << r
            << " | " << iterations
            << " | " << averageSeconds(iterationSeconds, iterations) * 1000.0
            << " | " << averageSeconds(computeSeconds, iterations) * 1000.0
            << " | " << averageSeconds(haloSeconds, iterations) * 1000.0
            << " | " << averageSeconds(candidateSeconds, iterations) * 1000.0
            << " | " << averageSeconds(ignitionSeconds, iterations) * 1000.0
            << " | " << averageSeconds(updateSeconds, iterations) * 1000.0
            << " | " << averageSeconds(reductionSeconds, iterations) * 1000.0
            << " | " << averageLongLong(candidates, iterations)
            << " | " << maxCandidates
            << " | " << averageLongLong(activeFires, iterations)
            << " | " << maxActiveFires
            << " | " << newFires
            << "\n";
    }

    std::cout << "\nResumen de balance de carga:\n";
    std::cout << "Desbalance por candidatos acumulados (max/promedio): " << candidateImbalance << "\n";
    std::cout << "Desbalance por computo acumulado (max/promedio): " << computeImbalance << "\n";
    std::cout << "Desbalance por tiempo acumulado (max/promedio): " << timeImbalance << "\n";
    std::cout << "Interpretacion: para balance de carga, priorizar candidatos y computo. El tiempo total puede ocultar diferencias por sincronizacion MPI.\n";
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

    int sharedOptions[7] = {
        options.rows,
        options.cols,
        options.iterations,
        options.numFires,
        options.showWindow ? 1 : 0,
        (int)options.wind,
        options.showMetrics ? 1 : 0
    };
    MPI_Bcast(sharedOptions, 7, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&options.seed, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
    options.rows = sharedOptions[0];
    options.cols = sharedOptions[1];
    options.iterations = sharedOptions[2];
    options.numFires = sharedOptions[3];
    options.showWindow = sharedOptions[4] != 0;
    options.wind = (WindDirection)sharedOptions[5];
    options.showMetrics = sharedOptions[6] != 0;

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
    std::vector<Position> initialFires;
    int numFiresFound = 0;

    if (rank == 0) {
        std::cout << "Generando entorno " << options.rows << "x" << options.cols
                  << " para " << worldSize << " procesos..." << std::endl;
        environment = createEnvironment(options.rows, options.cols, options.seed);
        printEnvironmentStats(environment);

        initialFires = chooseMultipleFires(environment, options.seed, options.numFires);
        numFiresFound = (int)initialFires.size();
        if (numFiresFound > 0) {
            std::cout << "Focos iniciales (" << numFiresFound << "):\n";
            for (int i = 0; i < numFiresFound; ++i)
                std::cout << "  Foco " << (i + 1) << ": (" << initialFires[i].row << ", " << initialFires[i].col << ")\n";
            std::cout << "Viento: " << windDirectionName(options.wind) << "\n";
            std::cout << "Bloques MPI: " << dims[0] << "x" << dims[1] << "\n";
        } else {
            std::cerr << "No hay casillas combustibles para iniciar el fuego." << std::endl;
        }
    }

    MPI_Bcast(&numFiresFound, 1, MPI_INT, 0, cartComm);

    if (numFiresFound == 0) {
        MPI_Comm_free(&cartComm);
        MPI_Finalize();
        return 1;
    }

    // Broadcast de todas las coordenadas de fuego
    std::vector<int> fireCoords(2 * numFiresFound);
    if (rank == 0) {
        for (int i = 0; i < numFiresFound; ++i) {
            fireCoords[2 * i]     = initialFires[i].row;
            fireCoords[2 * i + 1] = initialFires[i].col;
        }
    }
    MPI_Bcast(fireCoords.data(), 2 * numFiresFound, MPI_INT, 0, cartComm);

    if (rank != 0) {
        initialFires.resize(numFiresFound);
        for (int i = 0; i < numFiresFound; ++i) {
            initialFires[i] = {fireCoords[2 * i], fireCoords[2 * i + 1]};
        }
    }

    LocalBlock localBlock = scatterEnvironment(
        rank == 0 ? &environment : nullptr,
        options.rows,
        options.cols,
        cartComm
    );
    FireState fireState;
    initializeLocalFires(localBlock, fireState, initialFires);

    if (rank == 0)
        std::cout << "Simulando " << options.iterations << " iteraciones..." << std::endl;

    RankMetrics rankMetrics;

    MPI_Barrier(cartComm);
    double startTime = MPI_Wtime();
    for (int iteration = 0; iteration < options.iterations; ++iteration) {
        ParallelStepMetrics stepMetrics;
        double iterationStart = MPI_Wtime();
        int localNewFires = advanceParallelFire(
            localBlock,
            fireState,
            options.seed,
            iteration,
            options.wind,
            cartComm,
            &stepMetrics
        );
        int localActiveFires = countActiveFires(fireState);
        int globalNewFires = 0;
        int globalActiveFires = 0;

        double reductionStart = MPI_Wtime();
        MPI_Allreduce(&localNewFires, &globalNewFires, 1, MPI_INT, MPI_SUM, cartComm);
        MPI_Allreduce(&localActiveFires, &globalActiveFires, 1, MPI_INT, MPI_SUM, cartComm);
        double reductionEnd = MPI_Wtime();
        double iterationEnd = MPI_Wtime();

        double iterationSeconds = iterationEnd - iterationStart;
        double reductionSeconds = reductionEnd - reductionStart;
        rankMetrics.iterations++;
        rankMetrics.totalCandidates += stepMetrics.candidatesEvaluated;
        rankMetrics.totalNewFires += stepMetrics.newFires;
        rankMetrics.totalActiveFires += localActiveFires;
        rankMetrics.maxCandidates = std::max(rankMetrics.maxCandidates, stepMetrics.candidatesEvaluated);
        rankMetrics.maxActiveFires = std::max(rankMetrics.maxActiveFires, (long long)localActiveFires);
        rankMetrics.totalIterationSeconds += iterationSeconds;
        rankMetrics.totalHaloSeconds += stepMetrics.haloSeconds;
        rankMetrics.totalCandidateSeconds += stepMetrics.candidateSeconds;
        rankMetrics.totalIgnitionSeconds += stepMetrics.ignitionSeconds;
        rankMetrics.totalUpdateSeconds += stepMetrics.updateSeconds;
        rankMetrics.totalReductionSeconds += reductionSeconds;
        rankMetrics.maxIterationSeconds = std::max(rankMetrics.maxIterationSeconds, iterationSeconds);

        //if (rank == 0) {
        //    std::cout << "Iteracion " << iteration
        //              << " | nuevos fuegos: " << globalNewFires
        //              << " | fuegos activos: " << globalActiveFires
        //              << "\n";
        //}

        if (globalActiveFires == 0) {
            if (rank == 0)
                std::cout << "El fuego se extinguio en la iteracion " << iteration << ".\n";
            break;
        }
    }

    double localElapsed = MPI_Wtime() - startTime;
    double elapsed = 0.0;
    MPI_Reduce(&localElapsed, &elapsed, 1, MPI_DOUBLE, MPI_MAX, 0, cartComm);

    if (options.showMetrics)
        printMetricsReport(rankMetrics, cartComm);

    Matrix finalEnvironment;
    if (options.showWindow)
        finalEnvironment = gatherEnvironment(localBlock, options.rows, options.cols, cartComm);

    if (rank == 0) {
        std::cout << "Tiempo paralelo: " << elapsed * 1000.0 << " ms" << std::endl;
        if (options.showWindow) {
            std::cout << "Abriendo ventana SDL2 con el estado final... (Cerrar con ESC o boton X)" << std::endl;
            (void)finalEnvironment;
            //showMatrixSDL(finalEnvironment);
        }
    }

    MPI_Comm_free(&cartComm);
    MPI_Finalize();
    return 0;
}

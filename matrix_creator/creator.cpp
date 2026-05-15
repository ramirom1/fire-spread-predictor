#include <iostream>
#include <vector>
#include <queue>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <random>
#include <SDL2/SDL.h>

// ============================================================
// Tipos de casilla del entorno
// ============================================================
enum CellType {
    WATER    = 0,  // Agua
    FOREST   = 1,  // Bosque
    CITY     = 2,  // Ciudad
    BURNING  = 3,  // Quemándose
    ASH      = 4   // Ceniza
};

// ============================================================
// Representación de la matriz del entorno
// ============================================================
using Matrix = std::vector<std::vector<CellType>>;

// ============================================================
// Funciones auxiliares
// ============================================================

// Devuelve los 4-vecinos válidos de (r, c) en una grilla rows x cols
static std::vector<std::pair<int,int>> neighbors4(int r, int c, int rows, int cols) {
    std::vector<std::pair<int,int>> res;
    const int dr[] = {-1, 1, 0, 0};
    const int dc[] = {0, 0, -1, 1};
    for (int d = 0; d < 4; ++d) {
        int nr = r + dr[d];
        int nc = c + dc[d];
        if (nr >= 0 && nr < rows && nc >= 0 && nc < cols)
            res.push_back({nr, nc});
    }
    return res;
}



// ============================================================
// createEnvironment: genera la matriz del entorno
//
// Parámetros:
//   rows, cols   – dimensiones de la matriz
//   seed         – semilla para el RNG (0 = usar time)
//
// Proporciones objetivo (aproximadas):
//   Agua   ~15-20%
//   Bosque ~60-70%
//   Ciudad ~10-15%
//
// La matriz generada solo contiene WATER, FOREST y CITY.
// Las casillas BURNING y ASH se asignan durante la simulación.
// ============================================================
Matrix createEnvironment(int rows, int cols, unsigned int seed = 0) {
    // --- Inicialización ---
    if (seed == 0) seed = (unsigned int)std::time(nullptr);
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> randRow(0, rows - 1);
    std::uniform_int_distribution<int> randCol(0, cols - 1);
    std::uniform_real_distribution<double> prob(0.0, 1.0);

    int totalCells = rows * cols;

    // Empezamos con todo bosque
    Matrix mat(rows, std::vector<CellType>(cols, FOREST));

    // ----------------------------------------------------------
    // 1. Generar lagos (WATER) usando ruido fractal
    //    Genera terreno con formas naturales e irregulares.
    //    Objetivo: ~8-12% del total
    // ----------------------------------------------------------
    {
        // Generar mapa de ruido fractal (múltiples octavas)
        std::vector<std::vector<double>> noiseMap(rows, std::vector<double>(cols, 0.0));

        int numOctaves = 5;
        double persistence = 0.5;  // cuánto aporta cada octava
        double amplitude = 1.0;
        double totalAmplitude = 0.0;

        for (int oct = 0; oct < numOctaves; ++oct) {
            // Resolución de la grilla de ruido para esta octava
            // Octava 0: muy gruesa (pocos puntos), octava N: más fina
            int gridSize = 4 * (1 << oct); // 4, 8, 16, 32, 64...

            // Generar valores aleatorios en una grilla gruesa
            std::vector<std::vector<double>> grid(gridSize + 1,
                std::vector<double>(gridSize + 1));
            for (int gr = 0; gr <= gridSize; ++gr)
                for (int gc = 0; gc <= gridSize; ++gc)
                    grid[gr][gc] = prob(rng);

            // Interpolar bilinealmente al tamaño completo
            for (int r = 0; r < rows; ++r) {
                for (int c = 0; c < cols; ++c) {
                    double gr = (double)r / rows * gridSize;
                    double gc = (double)c / cols * gridSize;
                    int gr0 = (int)gr, gc0 = (int)gc;
                    int gr1 = std::min(gr0 + 1, gridSize);
                    int gc1 = std::min(gc0 + 1, gridSize);
                    double fr = gr - gr0, fc = gc - gc0;

                    // Interpolación bilineal
                    double v00 = grid[gr0][gc0], v10 = grid[gr1][gc0];
                    double v01 = grid[gr0][gc1], v11 = grid[gr1][gc1];
                    double v0 = v00 * (1 - fc) + v01 * fc;
                    double v1 = v10 * (1 - fc) + v11 * fc;
                    double val = v0 * (1 - fr) + v1 * fr;

                    noiseMap[r][c] += val * amplitude;
                }
            }
            totalAmplitude += amplitude;
            amplitude *= persistence;
        }

        // Normalizar a [0, 1]
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
                noiseMap[r][c] /= totalAmplitude;

        // Determinar umbral para obtener ~8-12% de agua
        // Recopilar todos los valores y encontrar el percentil correcto
        double waterFraction = 0.08 + prob(rng) * 0.04;
        std::vector<double> allValues;
        allValues.reserve(totalCells);
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
                allValues.push_back(noiseMap[r][c]);

        std::sort(allValues.begin(), allValues.end());
        double threshold = allValues[(int)(waterFraction * totalCells)];

        // Asignar agua donde el ruido está por debajo del umbral
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
                if (noiseMap[r][c] < threshold)
                    mat[r][c] = WATER;
    }

    // ----------------------------------------------------------
    // 1b. Limpieza: eliminar islotes de bosque de 1 celda en el agua
    // ----------------------------------------------------------
    bool changed = true;
    while (changed) {
        changed = false;
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                if (mat[r][c] != FOREST) continue;
                int waterCount = 0;
                auto nbrs = neighbors4(r, c, rows, cols);
                for (auto [nr, nc] : nbrs)
                    if (mat[nr][nc] == WATER) ++waterCount;
                if (waterCount >= 3 || (nbrs.size() <= 3 && waterCount == (int)nbrs.size())) {
                    mat[r][c] = WATER;
                    changed = true;
                }
            }
        }
    }

    // ----------------------------------------------------------
    // 2. Generar ciudades (CITY) usando ruido fractal
    //    Objetivo: ~10-15% del total
    //    Solo se colocan sobre FOREST (nunca sobre agua)
    // ----------------------------------------------------------
    {
        std::vector<std::vector<double>> cityNoise(rows, std::vector<double>(cols, 0.0));

        int numOctaves = 4;
        double persistence = 0.45;
        double amplitude = 1.0;
        double totalAmplitude = 0.0;

        for (int oct = 0; oct < numOctaves; ++oct) {
            int gridSize = 3 * (1 << oct); // 3, 6, 12, 24...

            std::vector<std::vector<double>> grid(gridSize + 1,
                std::vector<double>(gridSize + 1));
            for (int gr = 0; gr <= gridSize; ++gr)
                for (int gc = 0; gc <= gridSize; ++gc)
                    grid[gr][gc] = prob(rng);

            for (int r = 0; r < rows; ++r) {
                for (int c = 0; c < cols; ++c) {
                    double gr = (double)r / rows * gridSize;
                    double gc = (double)c / cols * gridSize;
                    int gr0 = (int)gr, gc0 = (int)gc;
                    int gr1 = std::min(gr0 + 1, gridSize);
                    int gc1 = std::min(gc0 + 1, gridSize);
                    double fr = gr - gr0, fc = gc - gc0;

                    double v00 = grid[gr0][gc0], v10 = grid[gr1][gc0];
                    double v01 = grid[gr0][gc1], v11 = grid[gr1][gc1];
                    double v0 = v00 * (1 - fc) + v01 * fc;
                    double v1 = v10 * (1 - fc) + v11 * fc;
                    double val = v0 * (1 - fr) + v1 * fr;

                    cityNoise[r][c] += val * amplitude;
                }
            }
            totalAmplitude += amplitude;
            amplitude *= persistence;
        }

        // Normalizar a [0, 1]
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
                cityNoise[r][c] /= totalAmplitude;

        // Recopilar solo los valores de celdas FOREST para calcular el umbral
        double cityFraction = 0.10 + prob(rng) * 0.05;
        std::vector<double> forestValues;
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
                if (mat[r][c] == FOREST)
                    forestValues.push_back(cityNoise[r][c]);

        if (!forestValues.empty()) {
            std::sort(forestValues.begin(), forestValues.end());
            // El umbral se calcula sobre el total de celdas, no solo bosque
            int cityCells = (int)(cityFraction * totalCells);
            cityCells = std::min(cityCells, (int)forestValues.size());
            double threshold = forestValues[cityCells - 1];

            for (int r = 0; r < rows; ++r)
                for (int c = 0; c < cols; ++c)
                    if (mat[r][c] == FOREST && cityNoise[r][c] <= threshold)
                        mat[r][c] = CITY;
        }
    }

    return mat;
}

// ============================================================
// Colores RGB para cada tipo de casilla
// ============================================================
struct Color { Uint8 r, g, b; };

static Color cellColor(CellType type) {
    switch (type) {
        case WATER:   return {41,  128, 185};  // Azul agua
        case FOREST:  return {39,  174, 96 };  // Verde bosque
        case CITY:    return {243, 156, 18 };  // Naranja/amarillo ciudad
        case BURNING: return {231, 76,  60 };  // Rojo fuego
        case ASH:     return {127, 140, 141};  // Gris ceniza
    }
    return {0, 0, 0};
}

// ============================================================
// showMatrixSDL: renderiza la matriz en una ventana SDL2
//
// Parámetros:
//   mat       – la matriz a visualizar
//   cellSize  – tamaño en píxeles de cada celda (0 = auto)
// ============================================================
void showMatrixSDL(const Matrix &mat, int cellSize = 0) {
    int rows = (int)mat.size();
    int cols = (int)mat[0].size();

    // Necesitamos SDL inicializado para detectar resolución
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "Error SDL_Init: " << SDL_GetError() << std::endl;
        return;
    }

    // Auto-calcular tamaño de celda según resolución real de pantalla
    if (cellSize <= 0) {
        SDL_DisplayMode dm;
        SDL_GetDesktopDisplayMode(0, &dm);
        // Usar 85% de la pantalla como máximo
        int maxW = (int)(dm.w * 0.85);
        int maxH = (int)(dm.h * 0.85);
        cellSize = std::max(1, std::min(maxW / cols, maxH / rows));
    }

    int winW = cols * cellSize;
    int winH = rows * cellSize;

    SDL_Window *window = SDL_CreateWindow(
        "Fire Spread Predictor - Entorno",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        winW, winH, SDL_WINDOW_SHOWN
    );
    if (!window) {
        std::cerr << "Error SDL_CreateWindow: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer) {
        std::cerr << "Error SDL_CreateRenderer: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    // Renderizar la matriz
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            Color col = cellColor(mat[r][c]);
            SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, 255);
            SDL_Rect rect = {c * cellSize, r * cellSize, cellSize, cellSize};
            SDL_RenderFillRect(renderer, &rect);
        }
    }
    SDL_RenderPresent(renderer);

    // Bucle de eventos: mantener ventana abierta hasta que se cierre
    bool running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                running = false;
        }
        SDL_Delay(16); // ~60 FPS
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

// ============================================================
// Main de prueba
// ============================================================
int main() {
    const int ROWS = 500;
    const int COLS = 500;

    std::cout << "Generando entorno " << ROWS << "x" << COLS << "..." << std::endl;
    Matrix env = createEnvironment(ROWS, COLS);

    // Estadísticas
    int counts[5] = {};
    for (const auto &row : env)
        for (CellType c : row)
            counts[c]++;

    int total = ROWS * COLS;
    std::cout << "Agua:   " << counts[WATER]   << " (" << 100.0*counts[WATER]/total   << "%)\n";
    std::cout << "Bosque: " << counts[FOREST]  << " (" << 100.0*counts[FOREST]/total  << "%)\n";
    std::cout << "Ciudad: " << counts[CITY]    << " (" << 100.0*counts[CITY]/total    << "%)\n";

    std::cout << "Abriendo ventana SDL2... (Cerrar con ESC o botón X)" << std::endl;
    showMatrixSDL(env);

    return 0;
}


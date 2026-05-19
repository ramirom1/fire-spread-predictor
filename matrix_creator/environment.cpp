#include "environment.h"

#include <algorithm>
#include <ctime>
#include <iostream>
#include <random>
#include <SDL2/SDL.h>

std::vector<std::pair<int, int>> neighbors4(int row, int col, int rows, int cols) {
    std::vector<std::pair<int, int>> res;
    const int dr[] = {-1, 1, 0, 0};
    const int dc[] = {0, 0, -1, 1};

    for (int d = 0; d < 4; ++d) {
        int nr = row + dr[d];
        int nc = col + dc[d];
        if (nr >= 0 && nr < rows && nc >= 0 && nc < cols)
            res.push_back({nr, nc});
    }

    return res;
}

Matrix createEnvironment(int rows, int cols, unsigned int seed) {
    if (seed == 0)
        seed = (unsigned int)std::time(nullptr);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> prob(0.0, 1.0);

    int totalCells = rows * cols;
    Matrix mat(rows, std::vector<CellType>(cols, FOREST));

    {
        std::vector<std::vector<double>> noiseMap(rows, std::vector<double>(cols, 0.0));

        int numOctaves = 5;
        double persistence = 0.5;
        double amplitude = 1.0;
        double totalAmplitude = 0.0;

        for (int oct = 0; oct < numOctaves; ++oct) {
            int gridSize = 4 * (1 << oct);

            std::vector<std::vector<double>> grid(gridSize + 1,
                std::vector<double>(gridSize + 1));
            for (int gr = 0; gr <= gridSize; ++gr)
                for (int gc = 0; gc <= gridSize; ++gc)
                    grid[gr][gc] = prob(rng);

            for (int r = 0; r < rows; ++r) {
                for (int c = 0; c < cols; ++c) {
                    double gr = (double)r / rows * gridSize;
                    double gc = (double)c / cols * gridSize;
                    int gr0 = (int)gr;
                    int gc0 = (int)gc;
                    int gr1 = std::min(gr0 + 1, gridSize);
                    int gc1 = std::min(gc0 + 1, gridSize);
                    double fr = gr - gr0;
                    double fc = gc - gc0;

                    double v00 = grid[gr0][gc0];
                    double v10 = grid[gr1][gc0];
                    double v01 = grid[gr0][gc1];
                    double v11 = grid[gr1][gc1];
                    double v0 = v00 * (1 - fc) + v01 * fc;
                    double v1 = v10 * (1 - fc) + v11 * fc;
                    double val = v0 * (1 - fr) + v1 * fr;

                    noiseMap[r][c] += val * amplitude;
                }
            }

            totalAmplitude += amplitude;
            amplitude *= persistence;
        }

        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
                noiseMap[r][c] /= totalAmplitude;

        double waterFraction = 0.08 + prob(rng) * 0.04;
        std::vector<double> allValues;
        allValues.reserve(totalCells);
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
                allValues.push_back(noiseMap[r][c]);

        std::sort(allValues.begin(), allValues.end());
        double threshold = allValues[(int)(waterFraction * totalCells)];

        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
                if (noiseMap[r][c] < threshold)
                    mat[r][c] = WATER;
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                if (mat[r][c] != FOREST)
                    continue;

                int waterCount = 0;
                auto nbrs = neighbors4(r, c, rows, cols);
                for (auto [nr, nc] : nbrs)
                    if (mat[nr][nc] == WATER)
                        ++waterCount;

                if (waterCount >= 3 || (nbrs.size() <= 3 && waterCount == (int)nbrs.size())) {
                    mat[r][c] = WATER;
                    changed = true;
                }
            }
        }
    }

    {
        std::vector<std::vector<double>> cityNoise(rows, std::vector<double>(cols, 0.0));

        int numOctaves = 4;
        double persistence = 0.45;
        double amplitude = 1.0;
        double totalAmplitude = 0.0;

        for (int oct = 0; oct < numOctaves; ++oct) {
            int gridSize = 3 * (1 << oct);

            std::vector<std::vector<double>> grid(gridSize + 1,
                std::vector<double>(gridSize + 1));
            for (int gr = 0; gr <= gridSize; ++gr)
                for (int gc = 0; gc <= gridSize; ++gc)
                    grid[gr][gc] = prob(rng);

            for (int r = 0; r < rows; ++r) {
                for (int c = 0; c < cols; ++c) {
                    double gr = (double)r / rows * gridSize;
                    double gc = (double)c / cols * gridSize;
                    int gr0 = (int)gr;
                    int gc0 = (int)gc;
                    int gr1 = std::min(gr0 + 1, gridSize);
                    int gc1 = std::min(gc0 + 1, gridSize);
                    double fr = gr - gr0;
                    double fc = gc - gc0;

                    double v00 = grid[gr0][gc0];
                    double v10 = grid[gr1][gc0];
                    double v01 = grid[gr0][gc1];
                    double v11 = grid[gr1][gc1];
                    double v0 = v00 * (1 - fc) + v01 * fc;
                    double v1 = v10 * (1 - fc) + v11 * fc;
                    double val = v0 * (1 - fr) + v1 * fr;

                    cityNoise[r][c] += val * amplitude;
                }
            }

            totalAmplitude += amplitude;
            amplitude *= persistence;
        }

        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
                cityNoise[r][c] /= totalAmplitude;

        double cityFraction = 0.10 + prob(rng) * 0.05;
        std::vector<double> forestValues;
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
                if (mat[r][c] == FOREST)
                    forestValues.push_back(cityNoise[r][c]);

        if (!forestValues.empty()) {
            std::sort(forestValues.begin(), forestValues.end());
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

struct Color {
    Uint8 r;
    Uint8 g;
    Uint8 b;
};

static Color cellColor(CellType type) {
    switch (type) {
        case WATER:   return {41,  128, 185};
        case FOREST:  return {39,  174, 96 };
        case CITY:    return {243, 156, 18 };
        case BURNING: return {231, 76,  60 };
        case ASH:     return {127, 140, 141};
    }
    return {0, 0, 0};
}

void showMatrixSDL(const Matrix &mat, int cellSize) {
    int rows = (int)mat.size();
    int cols = (int)mat[0].size();

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "Error SDL_Init: " << SDL_GetError() << std::endl;
        return;
    }

    if (cellSize <= 0) {
        SDL_DisplayMode dm;
        SDL_GetDesktopDisplayMode(0, &dm);
        int maxW = (int)(dm.w * 0.85);
        int maxH = (int)(dm.h * 0.85);
        cellSize = std::max(1, std::min(maxW / cols, maxH / rows));
    }

    int winW = cols * cellSize;
    int winH = rows * cellSize;

    SDL_Window *window = SDL_CreateWindow(
        "Fire Spread Predictor",
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

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            Color col = cellColor(mat[r][c]);
            SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, 255);
            SDL_Rect rect = {c * cellSize, r * cellSize, cellSize, cellSize};
            SDL_RenderFillRect(renderer, &rect);
        }
    }
    SDL_RenderPresent(renderer);

    bool running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                running = false;
        }
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

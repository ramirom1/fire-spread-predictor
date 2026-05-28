#pragma once

#include "../matrix_creator/environment.h"

#include <optional>
#include <random>
#include <vector>

struct Position {
    int row;
    int col;
};

struct FireState {
    std::vector<Position> listaFuego1;
    std::vector<Position> listaFuego2;
    std::vector<Position> listaFuego3;
};

// Dirección del viento
enum class WindDirection {
    NONE,   // Sin viento
    NORTH,  // Viento sopla hacia el norte (fuego se propaga más fácil hacia el norte)
    SOUTH,
    EAST,
    WEST
};

WindDirection parseWindDirection(const char *str);

// Sorteo determinístico de ignición (misma función que la versión paralela).
// Devuelve un valor en [0, 1) basado en (seed, iteration, globalRow, globalCol).
double ignitionDraw(unsigned int seed, int iteration, int globalRow, int globalCol);

// Elige la celda inicial de fuego de forma determinística a partir de la seed.
// Misma lógica que chooseInitialFire de la versión paralela.
std::optional<Position> chooseInitialFire(Matrix &mat, unsigned int seed);

std::optional<Position> igniteRandomCell(Matrix &mat, FireState &fireState, std::mt19937 &rng);
int advanceFire(Matrix &mat, FireState &fireState, unsigned int seed, int iteration, WindDirection wind = WindDirection::NONE);
int countActiveFires(const FireState &fireState);

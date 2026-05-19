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

std::optional<Position> igniteRandomCell(Matrix &mat, FireState &fireState, std::mt19937 &rng);
int advanceFire(Matrix &mat, FireState &fireState, std::mt19937 &rng, WindDirection wind = WindDirection::NONE);
int countActiveFires(const FireState &fireState);

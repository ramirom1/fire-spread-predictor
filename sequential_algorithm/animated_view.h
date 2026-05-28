#pragma once

#include "sequential_fire.h"
#include "../matrix_creator/environment.h"

#include <SDL2/SDL.h>
#include <random>
#include <string>

/// @class AnimatedView
/// @brief Visualización animada de la simulación de fuego usando SDL2.
///        Usa SDL_Texture (1 píxel por celda) + zoom/pan con el ratón y teclado.
class AnimatedView {
public:
    /// Construye la vista animada.
    /// @param rows         Filas de la grilla
    /// @param cols         Columnas de la grilla
    /// @param seed         Semilla para el RNG
    /// @param wind         Dirección del viento
    /// @param winWidth     Ancho inicial de la ventana en píxeles
    /// @param winHeight    Alto inicial de la ventana en píxeles
    AnimatedView(int rows, int cols, unsigned int seed,
                 WindDirection wind = WindDirection::NONE,
                 int winWidth = 1024, int winHeight = 768);

    ~AnimatedView();

    /// Ejecuta el bucle principal (bloquea hasta que se cierre la ventana).
    void run();

private:
    // ── Simulación ────────────────────────────────────────────
    int            m_rows;
    int            m_cols;
    unsigned int   m_seed;
    Matrix         m_env;
    FireState      m_fireState;
    std::mt19937   m_rng;
    WindDirection   m_wind;
    int            m_generation;
    bool           m_finished;      ///< true cuando el fuego se extinguió

    // ── Visualización ─────────────────────────────────────────
    SDL_Window*    m_window;
    SDL_Renderer*  m_renderer;
    SDL_Texture*   m_texture;       ///< 1 píxel por celda
    int            m_winWidth;
    int            m_winHeight;

    // ── Viewport (zoom / pan) ─────────────────────────────────
    double         m_viewX;         ///< Esquina superior izquierda del viewport (en celdas)
    double         m_viewY;
    double         m_viewW;         ///< Ancho del viewport (en celdas)
    double         m_viewH;         ///< Alto del viewport (en celdas)
    double         m_zoomLevel;     ///< Factor de zoom (1.0 = ver toda la grilla)

    // ── Control de tiempo ─────────────────────────────────────
    bool           m_running;       ///< false para terminar el loop
    bool           m_paused;
    float          m_stepTime;      ///< Segundos entre pasos de simulación
    Uint32         m_lastStepTick;

    // ── Drag (pan con ratón) ──────────────────────────────────
    bool           m_dragging;
    int            m_dragStartX;
    int            m_dragStartY;
    double         m_dragStartViewX;
    double         m_dragStartViewY;

    // ── Métodos internos ──────────────────────────────────────
    void initSimulation(unsigned int seed);
    void handleEvents();
    void stepSimulation();
    void updateTexture();
    void render();
    void renderHUD();
    void clampViewport();

    /// Convierte coordenadas de pantalla a coordenadas de celda.
    void screenToCell(int sx, int sy, double &cellX, double &cellY) const;

    /// Zoom centrado en un punto de la pantalla.
    void zoomAt(int screenX, int screenY, double factor);

    /// Convierte un CellType a color RGBX8888.
    static Uint32 cellToPixel(CellType type);
};

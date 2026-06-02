#include "animated_view.h"

#include <algorithm>
#include <cstdio>
#include <iostream>

// ════════════════════════════════════════════════════════════════
// Colores (mismo esquema que environment.cpp)
// ════════════════════════════════════════════════════════════════
Uint32 AnimatedView::cellToPixel(CellType type) {
    // Formato SDL_PIXELFORMAT_ARGB8888: 0xAARRGGBB
    switch (type) {
        case WATER:   return 0xFF2980B9;  // (41, 128, 185)
        case FOREST:  return 0xFF27AE60;  // (39, 174, 96)
        case CITY:    return 0xFFF39C12;  // (243, 156, 18)
        case BURNING: return 0xFFE74C3C;  // (231, 76, 60)
        case ASH:     return 0xFF7F8C8D;  // (127, 140, 141)
    }
    return 0xFF000000;
}

// ════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ════════════════════════════════════════════════════════════════
AnimatedView::AnimatedView(int rows, int cols, unsigned int seed,
                           WindDirection wind, int numFires, int winWidth, int winHeight)
    : m_rows(rows), m_cols(cols)
    , m_seed(seed), m_rng(seed), m_wind(wind)
    , m_generation(0), m_numFires(numFires), m_finished(false)
    , m_window(nullptr), m_renderer(nullptr), m_texture(nullptr)
    , m_winWidth(winWidth), m_winHeight(winHeight)
    , m_viewX(0), m_viewY(0)
    , m_viewW(cols), m_viewH(rows)
    , m_zoomLevel(1.0)
    , m_running(true)
    , m_paused(true)
    , m_stepTime(0.15f)
    , m_lastStepTick(0)
    , m_dragging(false)
    , m_dragStartX(0), m_dragStartY(0)
    , m_dragStartViewX(0), m_dragStartViewY(0)
{
    // Crear entorno
    initSimulation(seed);

    // Inicializar SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "Error SDL_Init: " << SDL_GetError() << std::endl;
        return;
    }

    m_window = SDL_CreateWindow(
        "Fire Spread Predictor – Animated",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        m_winWidth, m_winHeight,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!m_window) {
        std::cerr << "Error SDL_CreateWindow: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return;
    }

    m_renderer = SDL_CreateRenderer(
        m_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!m_renderer) {
        std::cerr << "Error SDL_CreateRenderer: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(m_window);
        SDL_Quit();
        return;
    }

    // Textura: 1 píxel por celda
    m_texture = SDL_CreateTexture(
        m_renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        m_cols, m_rows
    );
    if (!m_texture) {
        std::cerr << "Error SDL_CreateTexture: " << SDL_GetError() << std::endl;
    }

    // Usar nearest-neighbor para que el zoom no haga blur
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    updateTexture();
}

AnimatedView::~AnimatedView() {
    if (m_texture)  SDL_DestroyTexture(m_texture);
    if (m_renderer) SDL_DestroyRenderer(m_renderer);
    if (m_window)   SDL_DestroyWindow(m_window);
    SDL_Quit();
}

// ════════════════════════════════════════════════════════════════
// Inicialización de la simulación
// ════════════════════════════════════════════════════════════════
void AnimatedView::initSimulation(unsigned int seed) {
    m_seed = seed;
    m_env = createEnvironment(m_rows, m_cols, seed);
    m_fireState = FireState{};
    m_generation = 0;
    m_finished = false;

    std::vector<Position> fires = chooseMultipleFires(m_env, seed, m_numFires);
    if (!fires.empty()) {
        for (const Position &fire : fires)
            m_fireState.listaFuego1.push_back(fire);
        std::cout << "Focos iniciales (" << fires.size() << "):\n";
        for (int i = 0; i < (int)fires.size(); ++i)
            std::cout << "  Foco " << (i + 1) << ": (" << fires[i].row << ", " << fires[i].col << ")\n";
    } else {
        std::cerr << "No hay celdas combustibles.\n";
        m_finished = true;
    }
}

// ════════════════════════════════════════════════════════════════
// Conversión coordenadas pantalla ↔ celda
// ════════════════════════════════════════════════════════════════
void AnimatedView::screenToCell(int sx, int sy, double &cellX, double &cellY) const {
    cellX = m_viewX + (double)sx / m_winWidth  * m_viewW;
    cellY = m_viewY + (double)sy / m_winHeight * m_viewH;
}

void AnimatedView::clampViewport() {
    // No dejar que el viewport se salga de la grilla
    m_viewW = std::min(m_viewW, (double)m_cols);
    m_viewH = std::min(m_viewH, (double)m_rows);
    m_viewX = std::max(0.0, std::min(m_viewX, (double)m_cols - m_viewW));
    m_viewY = std::max(0.0, std::min(m_viewY, (double)m_rows - m_viewH));
}

void AnimatedView::zoomAt(int screenX, int screenY, double factor) {
    // Coordenada de celda bajo el cursor ANTES del zoom
    double cellX, cellY;
    screenToCell(screenX, screenY, cellX, cellY);

    m_zoomLevel *= factor;
    m_zoomLevel = std::max(0.01, std::min(m_zoomLevel, 1.0));

    // Nuevo tamaño del viewport
    m_viewW = m_cols * m_zoomLevel;
    m_viewH = m_rows * m_zoomLevel;

    // Reposicionar para que el punto bajo el cursor no se mueva
    m_viewX = cellX - (double)screenX / m_winWidth  * m_viewW;
    m_viewY = cellY - (double)screenY / m_winHeight * m_viewH;

    clampViewport();
}

// ════════════════════════════════════════════════════════════════
// Eventos
// ════════════════════════════════════════════════════════════════
void AnimatedView::handleEvents() {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_QUIT:
                m_running = false;
                return;

            case SDL_WINDOWEVENT:
                if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    m_winWidth  = ev.window.data1;
                    m_winHeight = ev.window.data2;
                }
                break;

            case SDL_KEYDOWN:
                switch (ev.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        m_running = false;
                        return;

                    case SDLK_SPACE:
                        if (!m_finished) {
                            m_paused = !m_paused;
                            std::cout << (m_paused ? "[PAUSED]" : "[RUNNING]") << '\n';
                        }
                        break;

                    case SDLK_s:
                        if (!m_finished) {
                            stepSimulation();
                            std::cout << "Step → gen " << m_generation << '\n';
                        }
                        break;

                    case SDLK_r: {
                        unsigned int newSeed = m_rng();
                        initSimulation(newSeed);
                        updateTexture();
                        m_paused = true;
                        m_viewX = 0; m_viewY = 0;
                        m_viewW = m_cols; m_viewH = m_rows;
                        m_zoomLevel = 1.0;
                        std::cout << "[RESET]\n";
                        break;
                    }

                    // Velocidad de simulación
                    case SDLK_UP:
                        m_stepTime = std::max(0.02f, m_stepTime - 0.03f);
                        std::cout << "Velocidad: " << (1.0f / m_stepTime) << " pasos/s\n";
                        break;
                    case SDLK_DOWN:
                        m_stepTime = std::min(2.0f, m_stepTime + 0.03f);
                        std::cout << "Velocidad: " << (1.0f / m_stepTime) << " pasos/s\n";
                        break;

                    // Zoom con teclado (+ / -)
                    case SDLK_PLUS:
                    case SDLK_EQUALS:
                    case SDLK_KP_PLUS:
                        zoomAt(m_winWidth / 2, m_winHeight / 2, 0.8);
                        break;
                    case SDLK_MINUS:
                    case SDLK_KP_MINUS:
                        zoomAt(m_winWidth / 2, m_winHeight / 2, 1.25);
                        break;

                    // Reset zoom
                    case SDLK_0:
                        m_viewX = 0; m_viewY = 0;
                        m_viewW = m_cols; m_viewH = m_rows;
                        m_zoomLevel = 1.0;
                        break;

                    default: break;
                }
                break;

            // ── Zoom con rueda del ratón ──────────────────────
            case SDL_MOUSEWHEEL: {
                int mx, my;
                SDL_GetMouseState(&mx, &my);
                double factor = (ev.wheel.y > 0) ? 0.85 : 1.18;
                zoomAt(mx, my, factor);
                break;
            }

            // ── Pan con click izquierdo + arrastre ────────────
            case SDL_MOUSEBUTTONDOWN:
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    m_dragging = true;
                    m_dragStartX = ev.button.x;
                    m_dragStartY = ev.button.y;
                    m_dragStartViewX = m_viewX;
                    m_dragStartViewY = m_viewY;
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (ev.button.button == SDL_BUTTON_LEFT)
                    m_dragging = false;
                break;

            case SDL_MOUSEMOTION:
                if (m_dragging) {
                    double dx = (double)(m_dragStartX - ev.motion.x) / m_winWidth  * m_viewW;
                    double dy = (double)(m_dragStartY - ev.motion.y) / m_winHeight * m_viewH;
                    m_viewX = m_dragStartViewX + dx;
                    m_viewY = m_dragStartViewY + dy;
                    clampViewport();
                }
                break;
        }
    }
}

// ════════════════════════════════════════════════════════════════
// Paso de simulación
// ════════════════════════════════════════════════════════════════
void AnimatedView::stepSimulation() {
    int newFires = advanceFire(m_env, m_fireState, m_seed, m_generation, m_wind);
    ++m_generation;

    if (countActiveFires(m_fireState) == 0) {
        m_finished = true;
        m_paused = true;
        std::cout << "El fuego se extinguió en la generación " << m_generation << ".\n";
    }

    updateTexture();
}

// ════════════════════════════════════════════════════════════════
// Actualizar la textura (1 píxel por celda)
// ════════════════════════════════════════════════════════════════
void AnimatedView::updateTexture() {
    if (!m_texture) return;

    Uint32 *pixels = nullptr;
    int pitch = 0;

    if (SDL_LockTexture(m_texture, nullptr, (void **)&pixels, &pitch) != 0) {
        std::cerr << "Error SDL_LockTexture: " << SDL_GetError() << std::endl;
        return;
    }

    int pitchInPixels = pitch / 4;
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_cols; ++c) {
            pixels[r * pitchInPixels + c] = cellToPixel(m_env[r][c]);
        }
    }

    SDL_UnlockTexture(m_texture);
}

// ════════════════════════════════════════════════════════════════
// Render
// ════════════════════════════════════════════════════════════════
void AnimatedView::render() {
    if (!m_renderer || !m_texture) return;

    SDL_SetRenderDrawColor(m_renderer, 20, 20, 20, 255);
    SDL_RenderClear(m_renderer);

    // Rectángulo fuente: la porción visible de la textura (en píxeles de textura)
    SDL_Rect src;
    src.x = (int)m_viewX;
    src.y = (int)m_viewY;
    src.w = (int)m_viewW;
    src.h = (int)m_viewH;

    // Clamp para no salirse de la textura
    src.x = std::max(0, std::min(src.x, m_cols - 1));
    src.y = std::max(0, std::min(src.y, m_rows - 1));
    src.w = std::min(src.w, m_cols - src.x);
    src.h = std::min(src.h, m_rows - src.y);

    // Destino: toda la ventana (SDL escala automáticamente)
    SDL_Rect dst = {0, 0, m_winWidth, m_winHeight};

    SDL_RenderCopy(m_renderer, m_texture, &src, &dst);

    renderHUD();

    SDL_RenderPresent(m_renderer);
}

// ════════════════════════════════════════════════════════════════
// HUD - Barra de información superpuesta
// ════════════════════════════════════════════════════════════════
void AnimatedView::renderHUD() {
    // Sin SDL_ttf, dibujamos una barra de estado simple con rectángulos de color
    // que indican el estado de la simulación.

    int barHeight = 6;
    int barY = m_winHeight - barHeight;

    // Barra de fondo
    SDL_SetRenderDrawColor(m_renderer, 30, 30, 30, 200);
    SDL_Rect barBg = {0, barY, m_winWidth, barHeight};
    SDL_RenderFillRect(m_renderer, &barBg);

    if (m_finished) {
        // Barra gris = terminado
        SDL_SetRenderDrawColor(m_renderer, 127, 140, 141, 255);
    } else if (m_paused) {
        // Barra amarilla = pausa
        SDL_SetRenderDrawColor(m_renderer, 243, 156, 18, 255);
    } else {
        // Barra verde = corriendo
        SDL_SetRenderDrawColor(m_renderer, 39, 174, 96, 255);
    }

    // Ancho proporcional al progreso del zoom
    int indicatorW = (int)(m_winWidth * (1.0 - m_zoomLevel + 0.05));
    indicatorW = std::max(20, std::min(indicatorW, m_winWidth));
    SDL_Rect indicator = {0, barY, indicatorW, barHeight};
    SDL_RenderFillRect(m_renderer, &indicator);
}

// ════════════════════════════════════════════════════════════════
// Bucle principal
// ════════════════════════════════════════════════════════════════
void AnimatedView::run() {
    if (!m_window || !m_renderer) return;

    std::cout << "=== Fire Spread Predictor – Animated ===\n"
              << "Controls:\n"
              << "  SPACE        – Play / Pause\n"
              << "  S            – Single step\n"
              << "  R            – Reset (new random map)\n"
              << "  UP / DOWN    – Increase / Decrease speed\n"
              << "  Scroll       – Zoom in / out (centered on cursor)\n"
              << "  Click+Drag   – Pan\n"
              << "  +/-          – Zoom in / out (centered on screen)\n"
              << "  0            – Reset zoom (fit all)\n"
              << "  ESC          – Quit\n\n";

    m_lastStepTick = SDL_GetTicks();

    while (m_running) {
        handleEvents();
        if (!m_running) break;

        // Avanzar simulación si no está pausada
        Uint32 now = SDL_GetTicks();
        if (!m_paused && !m_finished) {
            Uint32 interval = (Uint32)(m_stepTime * 1000.0f);
            if (now - m_lastStepTick >= interval) {
                stepSimulation();
                m_lastStepTick = now;
            }
        }

        render();
    }
}

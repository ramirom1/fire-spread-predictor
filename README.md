# Fire Spread Predictor

Simulador de propagación de fuego sobre un entorno generado proceduralmente, inspirado en el Juego de la Vida de Conway. El objetivo es modelar cómo el fuego se expande a través de distintos tipos de terreno en una matriz de gran tamaño, con ejecución secuencial y paralela (MPI).

## Tipos de casilla

| Tipo | Descripción | Color (SDL2) |
|------|-------------|--------------|
| 🟦 **Agua** | Lagos y cuerpos de agua. No se queman. | Azul `(41, 128, 185)` |
| 🟩 **Bosque** | Vegetación. Combustible principal del fuego. | Verde `(39, 174, 96)` |
| 🟧 **Ciudad** | Zonas urbanas. | Naranja `(243, 156, 18)` |
| 🟥 **Quemándose** | Casilla actualmente en llamas. | Rojo `(231, 76, 60)` |
| ⬜ **Ceniza** | Restos de una casilla que ya ardió. | Gris `(127, 140, 141)` |

## Estructura del proyecto

```
fire-spread-predictor/
├── README.md
├── install_dependencies.sh       # Instalación de dependencias (local)
├── job_np1.sh                    # Job SLURM: secuencial (1 proceso)
├── job_np2.sh                    # Job SLURM: paralelo (2 procesos)
├── job_np4.sh                    # Job SLURM: paralelo (4 procesos)
├── job_np8.sh                    # Job SLURM: paralelo (8 procesos)
├── matrix_creator/
│   ├── environment.h             # Tipos, Matrix y funciones públicas del entorno
│   ├── environment.cpp           # Generación del mapa + visualización SDL2
│   ├── creator.cpp               # Ejecutable para crear y mostrar solo el mapa
│   └── Makefile
├── sequential_algorithm/
│   ├── sequential_fire.h         # Estado y funciones del algoritmo secuencial
│   ├── sequential_fire.cpp       # Propagación secuencial del fuego
│   ├── animated_view.h           # Visualización animada SDL2
│   ├── animated_view.cpp         # Implementación de la vista animada
│   ├── main.cpp                  # Ejecutable de simulación secuencial
│   └── Makefile
└── parallel_algorithm/
    ├── mpi_fire.h                # Estado y funciones del algoritmo paralelo
    ├── mpi_fire.cpp              # Propagación paralela MPI del fuego
    ├── main.cpp                  # Ejecutable de simulación paralela
    ├── Makefile
    └── README.md                 # Documentación detallada del algoritmo MPI
```

## Generación del entorno

La matriz se genera proceduralmente usando **ruido fractal** (value noise con múltiples octavas e interpolación bilineal), lo que produce terrenos con formas naturales e irregulares:

- **Lagos**: 5 octavas de ruido, costas irregulares con bahías, penínsulas e islas. Cobertura ~8-12%.
- **Ciudades**: 4 octavas con grilla base distinta, regiones grandes e irregulares. Cobertura ~10-15%. Solo se ubican sobre bosque (nunca sobre agua).
- **Bosque**: Ocupa el espacio restante (~70-80%).

Incluye un paso de limpieza que elimina islotes de bosque aislados dentro de lagos.

### Función principal

```cpp
Matrix createEnvironment(int rows, int cols, unsigned int seed = 0);
```

- **Parámetros**: dimensiones de la matriz y semilla opcional (0 = usar `time`)
- **Retorna**: `std::vector<std::vector<CellType>>` con el entorno generado
- Cada ejecución con distinta semilla produce un mapa único
- Ambos algoritmos (secuencial y paralelo) usan esta misma función.

## Algoritmo de propagación del fuego

Después de generar el entorno, el simulador elige una o más casillas combustibles (`Bosque` o `Ciudad`) y las marca como `Quemándose`. La cantidad de focos iniciales se controla con el parámetro `--fires <n>` (por defecto: 1). Los focos se eligen de forma determinística a partir de la semilla usando *Fisher-Yates partial shuffle*, garantizando posiciones distintas y reproducibilidad.

El estado del fuego se guarda en tres listas:

- `listaFuego1`: casillas prendidas en la iteración actual.
- `listaFuego2`: casillas que llevan una iteración quemándose.
- `listaFuego3`: casillas que llevan dos iteraciones quemándose.

En cada iteración:

1. Cada casilla en fuego revisa sus vecinos ortogonales: arriba, abajo, izquierda y derecha.
2. Solo pueden prenderse las casillas `Bosque` y `Ciudad`.
3. `Agua`, `Ceniza` y casillas ya `Quemándose` no pueden prenderse.
4. La probabilidad de ignición depende del tipo de casilla y de cuántos vecinos prendidos tenga.
5. Las casillas nuevas pasan a `listaFuego1`.
6. Las casillas de `listaFuego1` avanzan a `listaFuego2`, las de `listaFuego2` a `listaFuego3` y las de `listaFuego3` pasan a `Ceniza`.

Para evitar revisar más de una vez la misma casilla candidata, se usa una estructura de deduplicación (`candidate` en la versión secuencial, `unordered_set` en la paralela) junto con una lista compacta de `candidates`.

### Decisión determinística de ignición

Ambas versiones usan la misma función hash determinística `ignitionDraw(seed, iteration, globalRow, globalCol)` basada en **SplitMix64** para decidir si una casilla se prende. Esto garantiza que **con la misma semilla, dimensiones y viento, ambas versiones producen exactamente el mismo resultado**, independientemente de la cantidad de procesos MPI.

### Probabilidades

Probabilidad base por vecino en fuego:

| Tipo | Probabilidad base |
|------|-------------------|
| Agua | `0%` |
| Bosque | `42%` |
| Ciudad | `24%` |
| Quemándose | `0%` |
| Ceniza | `0%` |

Cuando una casilla tiene más de un vecino en fuego, se usa probabilidad acumulada:

```cpp
probabilidad = 1 - pow(1 - probabilidadBase, vecinosEnFuego)
```

Probabilidades acumuladas para `Bosque`:

| Vecinos en fuego | Probabilidad |
|------------------|--------------|
| 1 | `42%` |
| 2 | `66%` |
| 3 | `80%` |
| 4 | `89%` |

Probabilidades acumuladas para `Ciudad`:

| Vecinos en fuego | Probabilidad |
|------------------|--------------|
| 1 | `24%` |
| 2 | `42%` |
| 3 | `56%` |
| 4 | `67%` |

### Viento

El viento modifica la probabilidad de propagación de forma asimétrica:

| Dirección relativa | Modificador |
|--------------------|-------------|
| A favor del viento | `×1.3` |
| Perpendicular | `×0.7` |
| Contra el viento | `×0.3` |

## Algoritmo paralelo MPI

La versión paralela distribuye la matriz entre procesos MPI usando una topología cartesiana 2D. Ver [parallel_algorithm/README.md](parallel_algorithm/README.md) para la documentación completa del algoritmo.

Resumen:
- El `rank 0` genera el mapa y lo distribuye en bloques 2D fijos.
- Cada iteración intercambia halos con vecinos usando `MPI_Sendrecv`.
- La decisión de ignición usa `ignitionDraw` (no depende del orden de recorrido).
- `MPI_Allreduce` detecta el fin de la simulación.

## Visualización

La versión secuencial tiene dos modos de visualización:

- **Modo animado** (defecto): visualización interactiva en tiempo real con SDL2.
- **Modo batch** (`--batch`): ejecuta la simulación completa y muestra el resultado final.

Controles del modo animado:

| Tecla | Acción |
|-------|--------|
| `SPACE` | Play / Pause |
| `S` | Single step |
| `R` | Reset (nuevo mapa aleatorio) |
| `UP / DOWN` | Aumentar / Reducir velocidad |
| `Scroll` | Zoom (centrado en cursor) |
| `Click+Drag` | Pan |
| `+/-` | Zoom (centrado en pantalla) |
| `0` | Reset zoom (ver todo) |
| `ESC` | Salir |

## Uso

### 1. Instalar dependencias (local)

```bash
./install_dependencies.sh
```

O manualmente:

```bash
sudo apt install -y build-essential libsdl2-dev libopenmpi-dev openmpi-bin
```

### 2. Crear y visualizar solo el mapa

```bash
cd matrix_creator
make run
```

### 3. Ejecutar la simulación secuencial

```bash
cd sequential_algorithm
make run                    # modo animado (defecto)
make run WIND=N             # con viento
make run SEED=42            # con semilla fija
make run WIND=E SEED=42     # combinado
```

O directamente:

```bash
./sequential_fire --rows 500 --cols 500 --iterations 200 --seed 42 --wind E --fires 3
./sequential_fire --no-window --rows 1000 --cols 1000 --seed 42   # sin GUI
```

### 4. Ejecutar la simulación paralela

```bash
cd parallel_algorithm
make
mpirun -np 4 ./mpi_fire --rows 500 --cols 500 --iterations 200 --seed 42 --wind E
mpirun -np 4 ./mpi_fire --no-window    # sin ventana SDL
```

### 5. Comparar resultados (misma seed)

Con la misma semilla y parámetros, ambas versiones producen el mismo resultado:

```bash
# Secuencial
./sequential_fire --rows 500 --cols 500 --iterations 200 --seed 42 --wind E --no-window

# Paralelo (cualquier cantidad de procesos)
mpirun -np 4 ./mpi_fire --rows 500 --cols 500 --iterations 200 --seed 42 --wind E --no-window
```

### Parámetros comunes

| Parámetro | Descripción | Defecto |
|-----------|-------------|---------|
| `--rows <n>` | Filas de la grilla | `10000` |
| `--cols <n>` | Columnas de la grilla | `10000` |
| `--iterations <n>` | Iteraciones máximas | `50000` |
| `--seed <n>` | Semilla para reproducibilidad | `time` |
| `--wind <N\|S\|E\|W>` | Dirección del viento | sin viento |
| `--fires <n>` | Cantidad de focos iniciales | `1` |
| `--no-window` | Sin ventana SDL (headless) | — |
| `--batch` | Modo batch (solo secuencial) | — |
| `--help`, `-h` | Muestra ayuda | — |

### Comandos del Makefile

#### Secuencial (`sequential_algorithm/`)

| Comando | Acción |
|---------|--------|
| `make` | Compilar con SDL2 |
| `make cluster` | Compilar sin SDL2 (para cluster) |
| `make run` | Compilar y ejecutar modo animado |
| `make run WIND=N SEED=42` | Con opciones |
| `make clean` | Eliminar binario |

#### Paralelo (`parallel_algorithm/`)

| Comando | Acción |
|---------|--------|
| `make` | Compilar con SDL2 |
| `make cluster` | Compilar sin SDL2 (para cluster) |
| `make run` | Compilar y ejecutar con 10 procesos |
| `make run-headless` | Ejecutar sin ventana SDL |
| `make clean` | Eliminar binario |

## Ejecución en cluster (SLURM)

El proyecto incluye scripts SLURM para ejecutar en cluster sin necesidad de SDL2. Se separaron en 4 jobs distintos según la cantidad de procesos (1 secuencial, 2, 4 y 8 paralelos MPI). Cada script ejecuta las combinaciones de 3 tamaños de matriz (10000, 15000, 20000) y 3 cantidades de focos (1, 3, 5).

### Jobs disponibles

```bash
sbatch job_np1.sh   # Ejecuta las 9 combinaciones secuenciales
sbatch job_np2.sh   # Ejecuta las 9 combinaciones en paralelo con 2 procesos
sbatch job_np4.sh   # Ejecuta las 9 combinaciones en paralelo con 4 procesos
sbatch job_np8.sh   # Ejecuta las 9 combinaciones en paralelo con 8 procesos
```

Los parámetros fijos (seed=43, iteraciones=100000, viento=E) están configurados como variables dentro de cada script.

### Compilación en cluster

Los jobs usan `make cluster` automáticamente al inicio, que compila con `-DNO_SDL`, eliminando la dependencia de SDL2. Se usa el compilador del sistema para evitar problemas de compatibilidad con librerías base.

Módulo requerido en el cluster (se carga automáticamente en los scripts):
- `openmpi/4.1.4` (u otra versión compatible)

## Dependencias

| Paquete | Uso | Requerido en cluster |
|---------|-----|---------------------|
| `build-essential` (g++) | Compilación C++17 | Sí (`gcc/12.2.0`) |
| `libsdl2-dev` | Visualización gráfica | No (se compila con `-DNO_SDL`) |
| `libopenmpi-dev` / `openmpi-bin` | Ejecución paralela MPI | Sí (`openmpi/4.1.4`) |

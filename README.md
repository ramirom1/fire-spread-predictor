# Fire Spread Predictor

Simulador de propagación de fuego sobre un entorno generado proceduralmente, inspirado en el Juego de la Vida de Conway. El objetivo es modelar cómo el fuego se expande a través de distintos tipos de terreno en una matriz de gran tamaño, con ejecución paralela.

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
├── install_dependencies.sh      # Script de instalación de dependencias
├── matrix_creator/
│   ├── environment.h            # Tipos, Matrix y funciones públicas del entorno
│   ├── environment.cpp          # Generación del mapa + visualización SDL2
│   ├── creator.cpp              # Ejecutable para crear y mostrar solo el mapa
│   └── Makefile                 # Compilación del generador de mapa
└── sequential_algorithm/
    ├── sequential_fire.h        # Estado y funciones del algoritmo secuencial
    ├── sequential_fire.cpp      # Propagación secuencial del fuego
    ├── main.cpp                 # Ejecutable de simulación secuencial
    └── Makefile                 # Compilación del algoritmo secuencial
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
- El algoritmo secuencial usa esta misma matriz como entrada, llamando a `createEnvironment`.

## Algoritmo de propagación del fuego

Después de generar el entorno, el simulador elige aleatoriamente una o más casillas combustibles (`Bosque` o `Ciudad`) y las marca como `Quemándose`. La cantidad de focos iniciales se controla con el parámetro `--fires <n>` (por defecto: 1). Los focos se eligen de forma determinística a partir de la semilla usando *Fisher-Yates partial shuffle*, garantizando posiciones distintas y reproducibilidad.

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

Para evitar revisar más de una vez la misma casilla candidata, se usa una matriz auxiliar de booleanos llamada `candidate` junto con una lista `candidates`. Cuando una casilla en fuego encuentra un vecino combustible, primero se verifica si `candidate[fila][columna]` está en `false`. Si todavía no fue marcado, se cambia a `true` y se agrega la posición a `candidates`. Si otro fuego llega al mismo vecino en la misma iteración, la marca ya está en `true`, entonces no se vuelve a agregar.

Después de marcar candidatos, el algoritmo recorre solo la lista `candidates`. Cada casilla candidata se evalúa una sola vez. La probabilidad no aumenta por repetir la prueba, sino por la cantidad real de vecinos prendidos que tiene esa casilla.

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

## 🖥️ Visualización

La matriz se renderiza en una ventana **SDL2** donde cada celda es un cuadrado coloreado. El tamaño de celda se calcula automáticamente según la resolución de pantalla (85% del tamaño disponible).

- Cerrar ventana: **ESC** o botón **X**

## Uso

### 1. Instalar dependencias

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
make run
```

#### Con viento

El simulador acepta un parámetro `--wind` para indicar la dirección del viento, lo que modifica la propagación del fuego de forma asimétrica:

```bash
# Usando make con la variable WIND
make run WIND=N    # Viento hacia el norte
make run WIND=S    # Viento hacia el sur
make run WIND=E    # Viento hacia el este
make run WIND=W    # Viento hacia el oeste

# O ejecutando el binario directamente
./sequential_fire --wind N
./sequential_fire -w S
```

Sin el parámetro, la simulación corre sin viento (propagación isotrópica).

#### Con múltiples focos de incendio

El simulador acepta un parámetro `--fires` para indicar la cantidad de focos iniciales:

```bash
# Usando make en modo batch
./sequential_fire --batch --fires 5

# Combinado con viento
./sequential_fire --batch --fires 3 --wind N --seed 42
```

Sin el parámetro, la simulación inicia con un único foco (comportamiento por defecto).

#### Comandos del Makefile

| Comando | Acción |
|---------|--------|
| `make` | Solo compilar |
| `make run` | Compilar y ejecutar sin viento |
| `make run WIND=N` | Compilar y ejecutar con viento al norte |
| `make clean` | Eliminar binario |

## 🔧 Dependencias

| Paquete | Uso |
|---------|-----|
| `build-essential` (g++) | Compilación C++17 |
| `libsdl2-dev` | Visualización gráfica de la matriz |
| `libopenmpi-dev` | Ejecución paralela (próximamente) |

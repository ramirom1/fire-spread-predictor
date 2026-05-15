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
└── matrix_creator/
    ├── creator.cpp              # Generador del entorno + visualización SDL2
    └── Makefile                 # Compilación y ejecución
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

### 2. Compilar y ejecutar

```bash
cd matrix_creator
make run
```

Otros comandos del Makefile:

| Comando | Acción |
|---------|--------|
| `make` | Solo compilar |
| `make run` | Compilar y ejecutar |
| `make clean` | Eliminar binario |

## 🔧 Dependencias

| Paquete | Uso |
|---------|-----|
| `build-essential` (g++) | Compilación C++17 |
| `libsdl2-dev` | Visualización gráfica de la matriz |
| `libopenmpi-dev` | Ejecución paralela (próximamente) |

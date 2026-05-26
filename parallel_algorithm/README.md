# Algoritmo paralelo MPI

Esta carpeta contiene la propagacion MPI del fuego. El proceso `rank 0` genera el mapa con `createEnvironment`, enciende la casilla inicial, reparte bloques 2D y al final reune el estado para visualizarlo con SDL.

Durante la simulacion todos los ranks computan:

- Cada rank es dueno de un bloque local de la matriz.
- Los bloques se organizan con una topologia cartesiana MPI.
- Cada iteracion intercambia halos de un borde con los vecinos superior, inferior, izquierdo y derecho.
- Cada rank decide solo las celdas que pertenecen a su bloque.
- Los candidatos locales se deduplican con `unordered_set` y se recorren en una lista compacta.
- `MPI_Allreduce` suma fuegos nuevos y fuegos activos globales para imprimir progreso y detectar el fin.

## Paso a paso del algoritmo

1. El programa inicia MPI con `MPI_Init` y cada proceso obtiene su `rank`.

2. Se leen los parametros de ejecucion: filas, columnas, iteraciones, semilla, viento y si se abre o no la ventana SDL. El `rank 0` difunde esa configuracion al resto de procesos con `MPI_Bcast`.

3. MPI calcula una grilla 2D de procesos con `MPI_Dims_create` y crea una topologia cartesiana con `MPI_Cart_create`. Por ejemplo, con 4 procesos normalmente se obtiene una grilla `2x2`.

4. El `rank 0` genera la matriz completa llamando a `createEnvironment(rows, cols, seed)`. Esa matriz inicial contiene `WATER`, `FOREST` y `CITY`.

5. El `rank 0` elige una celda combustible inicial, la marca como `BURNING` y comparte sus coordenadas globales con todos los procesos.

6. El `rank 0` divide la matriz en bloques 2D fijos. Cada proceso recibe un `LocalBlock` con su porcion de la matriz, su offset global y sus dimensiones locales.

7. Cada proceso inicializa su `FireState` local. Solo el proceso dueno de la celda inicial agrega esa posicion a `listaFuego1`; los demas empiezan con listas vacias.

8. Antes de medir tiempo, todos los procesos pasan por un unico `MPI_Barrier`. Dentro del `for` principal no hay un `MPI_Barrier` explicito por iteracion.

9. En cada iteracion, todos los bloques intercambian halos con sus vecinos. Se envian y reciben los bordes superior, inferior, izquierdo y derecho usando `MPI_Sendrecv`. Esto ocurre incluso si un bloque no tiene fuego, porque podria recibir fuego desde un vecino.

10. Cada proceso arma una lista local de candidatos. No recorre todo su bloque; solo revisa vecinos de sus fuegos locales y celdas de borde afectadas por halos con `BURNING`.

11. Para no evaluar dos veces la misma celda candidata, cada proceso usa un `unordered_set` de posiciones visitadas y una lista compacta `candidates`.

12. Cada candidato se evalua una sola vez. La probabilidad depende del tipo de celda, de los vecinos en fuego locales o recibidos por halo, y del viento si se paso `--wind`.

13. La decision aleatoria usa una funcion deterministica basada en `seed`, `iteration`, fila global y columna global. Asi la decision de una celda no depende del orden local de recorrido.

14. Cada proceso actualiza sus listas locales: `listaFuego3` pasa a `ASH`, `listaFuego2` pasa a `listaFuego3`, `listaFuego1` pasa a `listaFuego2` y los nuevos fuegos pasan a `listaFuego1`.

15. Cada proceso calcula sus fuegos nuevos y fuegos activos locales. Luego `MPI_Allreduce` suma esos valores para obtener los totales globales.

16. Si la cantidad global de fuegos activos es `0`, la simulacion termina. Si no, se avanza a la siguiente iteracion.

17. Al finalizar, cada proceso envia su bloque final al `rank 0`. El `rank 0` reconstruye la matriz completa y, si no se uso `--no-window`, la muestra con SDL.

La version actual usa bloques fijos: si se ejecuta con `-np 10`, la matriz se divide en 10 bloques al inicio y esos bloques no se reasignan durante la simulacion.

## Compilar

```bash
make
```

## Ejecutar

```bash
mpirun -np 4 ./mpi_fire
```

Para probar sin abrir la ventana final:

```bash
mpirun -np 4 ./mpi_fire --no-window
```

Opciones:

```text
--rows <n>
--cols <n>
--iterations <n>
--seed <n>
--wind <N|S|E|W>
--no-window
```

Ejemplo reproducible:

```bash
mpirun -np 4 ./mpi_fire --rows 500 --cols 500 --iterations 200 --seed 123 --wind E
```

# Simulador de Planificación de Procesos

TP Integrador — Sistemas Operativos (UNAHUR, 2C 2026) — **Opción 1: Planificación de Procesos**

Simula la ejecución de un lote de tareas con cuatro algoritmos de planificación (**FCFS**, **SJF**,
**Prioridades** y **Round Robin**) y muestra el diagrama de Gantt, las métricas por proceso y una
tabla comparativa.

## Estructura

```
src/pcb.h            PCB (datos de cada proceso) y estados
src/cola.c, cola.h   Cola de procesos preparados (READY)
src/planificador.c   Simulador: algoritmos, Gantt y métricas
cargas/              Lotes de prueba: mixta.txt, convoy.txt, parejas.txt
scripts/             correr_todo.sh
resultados/          Salidas de las ejecuciones
docs/capturas/       Capturas de pantalla de la ejecución
```

## Compilar

Con gcc y make (Linux, WSL o Windows con MinGW):

```bash
make            # genera ./planificador
make clean
```

Sin make: `gcc -O2 -Wall -Wextra -std=c11 -o planificador src/planificador.c src/cola.c`

## Uso

```bash
./planificador                              # lote mixto, los cuatro algoritmos
./planificador -f cargas/convoy.txt         # otro lote
./planificador -a rr -q 2 -v                # solo Round Robin, quantum 2, mostrando cambios de estado
./planificador -b -c 1                      # Round Robin con varios quantum y costo de cambio de contexto
./scripts/correr_todo.sh                    # corre todo y guarda en resultados/
```

| Opción | Significado |
|---|---|
| `-f archivo` | lote de tareas (default `cargas/mixta.txt`) |
| `-a fcfs\|sjf\|prio\|rr\|todos` | algoritmo (default `todos`) |
| `-q N` | quantum de Round Robin (default 3) |
| `-c N` | costo de cada cambio de contexto, en unidades de tiempo (default 0) |
| `-v` | muestra cada cambio de estado de los procesos |
| `-b` | barrido: Round Robin con quantum 1, 2, 3, 4, 6, 8, 12 y 16 |

## Formato de un lote

Una tarea por línea; las líneas que empiezan con `#` son comentarios.

```
# nombre     llegada  rafaga  prioridad (1 = la mas alta)
Reporte         0       8        3
Email           1       4        1
```

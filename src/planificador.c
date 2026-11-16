/*
 * planificador.c - Simulador de planificacion de procesos.
 *
 * Lee un lote de tareas desde un archivo, lo ejecuta con cada algoritmo de
 * planificacion y muestra el diagrama de Gantt, las metricas por proceso y
 * una tabla comparativa.
 *
 * Uso: ./planificador [-f archivo] [-a algoritmo] [-q quantum] [-c costo] [-v] [-b]
 *   -f  archivo con el lote de tareas (default: cargas/mixta.txt)
 *   -a  fcfs | sjf | prio | rr | todos (default: todos)
 *   -q  quantum de Round Robin (default: 3)
 *   -c  costo de cada cambio de contexto, en unidades de tiempo (default: 0)
 *   -v  muestra cada cambio de estado de los procesos
 *   -b  barrido: prueba Round Robin con varios quantum y compara
 *
 * El tiempo es simulado: avanza de a una unidad ("tick") y no depende del reloj.
 */
#define _POSIX_C_SOURCE 200809L   /* getopt() es de POSIX, no del estandar de C */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pcb.h"
#include "cola.h"

#define MAX_SEGMENTOS 2048
#define ANCHO_GANTT   88      /* columnas por renglon del diagrama */

typedef enum { ALG_FCFS, ALG_SJF, ALG_PRIO, ALG_RR, CANT_ALG } algoritmo_t;

static const char *NOMBRE_ALG[CANT_ALG] = {
    "FCFS (por orden de llegada)",
    "SJF (primero el mas corto, no expropiativo)",
    "Prioridades (no expropiativo)",
    "Round Robin",
};
static const char *SIGLA_ALG[CANT_ALG] = { "FCFS", "SJF", "Prioridades", "Round Robin" };

/* Un tramo del diagrama de Gantt: quien uso la CPU entre ini y fin */
#define OCIOSA  (-1)          /* nadie tenia la CPU                 */
#define CAMBIO  (-2)          /* tiempo gastado en cambio de contexto */
typedef struct { int quien, ini, fin; } segmento_t;

typedef struct {
    double espera_prom, retorno_prom, respuesta_prom;
    int    cambios_contexto;
    int    tiempo_total;
    int    despacho[MAX_SEGMENTOS];   /* PIDs en el orden en que recibieron la CPU */
    int    n_despacho;
    segmento_t gantt[MAX_SEGMENTOS];
    int    n_gantt;
} resultado_t;

/* ------------------------------------------------------------------ carga */

static int leer_lote(const char *ruta, pcb_t *procs)
{
    FILE *f = fopen(ruta, "r");
    if (!f) { perror(ruta); exit(EXIT_FAILURE); }

    char linea[256];
    int n = 0, nro = 0;
    while (fgets(linea, sizeof(linea), f)) {
        nro++;
        if (linea[0] == '#' || linea[0] == '\n' || linea[0] == '\r') continue;
        if (n == MAX_PROCESOS) { fprintf(stderr, "%s: mas de %d procesos\n", ruta, MAX_PROCESOS); exit(EXIT_FAILURE); }
        pcb_t *p = &procs[n];
        if (sscanf(linea, "%15s %d %d %d", p->nombre, &p->llegada, &p->rafaga, &p->prioridad) != 4
            || p->llegada < 0 || p->rafaga <= 0) {
            fprintf(stderr, "%s:%d: se esperaba 'nombre llegada rafaga prioridad'\n", ruta, nro);
            exit(EXIT_FAILURE);
        }
        p->pid = n + 1;
        n++;
    }
    fclose(f);
    if (n == 0) { fprintf(stderr, "%s: no tiene procesos\n", ruta); exit(EXIT_FAILURE); }
    return n;
}

/* ------------------------------------------------------------- simulacion */

static void agregar_tramo(resultado_t *r, int quien, int t)
{
    /* Si sigue el mismo que en el tick anterior, se alarga el tramo */
    if (r->n_gantt > 0 && r->gantt[r->n_gantt - 1].quien == quien && r->gantt[r->n_gantt - 1].fin == t) {
        r->gantt[r->n_gantt - 1].fin = t + 1;
        return;
    }
    if (r->n_gantt == MAX_SEGMENTOS) { fprintf(stderr, "Simulacion demasiado larga\n"); exit(EXIT_FAILURE); }
    r->gantt[r->n_gantt++] = (segmento_t){ quien, t, t + 1 };
}

/* Pasa a READY a los procesos que llegan hasta el instante t.
 * 'orden' tiene los indices de los procesos ordenados por llegada. */
static void admitir(pcb_t *procs, const int *orden, int n, int *sig, int t, cola_t *listos, int verboso)
{
    while (*sig < n && procs[orden[*sig]].llegada <= t) {
        pcb_t *p = &procs[orden[*sig]];
        p->estado = READY;
        encolar(listos, p);
        if (verboso) printf("  t=%-3d P%d (%s) llega: NUEVO -> READY\n", t, p->pid, p->nombre);
        (*sig)++;
    }
}

/* Planificador de corto plazo: elige de la cola de listos quien sigue */
static pcb_t *elegir(algoritmo_t alg, cola_t *listos)
{
    int mejor = 0;
    for (int i = 1; i < listos->cantidad; i++) {
        const pcb_t *a = listos->items[i], *b = listos->items[mejor];
        if (alg == ALG_SJF  && a->rafaga    < b->rafaga)    mejor = i;
        if (alg == ALG_PRIO && a->prioridad < b->prioridad) mejor = i;
    }
    /* FCFS y Round Robin: mejor queda en 0, o sea, el primero de la cola */
    return cola_extraer(listos, mejor);
}

static void simular(algoritmo_t alg, const pcb_t *lote, int n, int quantum, int costo,
                    int verboso, pcb_t *procs, resultado_t *r)
{
    memset(r, 0, sizeof(*r));
    memcpy(procs, lote, n * sizeof(pcb_t));

    int orden[MAX_PROCESOS];
    for (int i = 0; i < n; i++) {
        procs[i].estado = NUEVO;
        procs[i].restante = procs[i].rafaga;
        procs[i].inicio = -1;
        /* insercion ordenada por llegada (si empatan, queda el orden del archivo) */
        int j = i;
        while (j > 0 && procs[orden[j - 1]].llegada > procs[i].llegada) { orden[j] = orden[j - 1]; j--; }
        orden[j] = i;
    }

    cola_t listos;
    cola_iniciar(&listos);
    pcb_t *actual = NULL, *ultimo = NULL;
    int t = 0, sig = 0, terminados = 0, usado = 0;

    while (terminados < n) {
        admitir(procs, orden, n, &sig, t, &listos, verboso);

        /* Despacho: si la CPU esta libre y hay alguien listo, se le entrega */
        if (!actual && !cola_vacia(&listos)) {
            actual = elegir(alg, &listos);
            if (ultimo && ultimo != actual) {
                r->cambios_contexto++;
                for (int k = 0; k < costo; k++) {       /* tiempo que no aprovecha nadie */
                    agregar_tramo(r, CAMBIO, t++);
                    admitir(procs, orden, n, &sig, t, &listos, verboso);
                }
            }
            if (ultimo != actual) r->despacho[r->n_despacho++] = actual->pid;
            actual->estado = RUNNING;
            if (actual->inicio == -1) actual->inicio = t;
            usado = 0;
            if (verboso) printf("  t=%-3d P%d (%s) recibe la CPU: READY -> RUNNING\n", t, actual->pid, actual->nombre);
        }

        if (!actual) {                                   /* no llego nadie todavia */
            agregar_tramo(r, OCIOSA, t++);
            continue;
        }

        /* El proceso en RUNNING ejecuta una unidad de tiempo */
        agregar_tramo(r, actual->pid, t++);
        actual->restante--;
        usado++;

        /* Los que llegan en este instante entran a la cola antes que el desalojado */
        admitir(procs, orden, n, &sig, t, &listos, verboso);

        if (actual->restante == 0) {
            actual->estado = TERMINATED;
            actual->fin = t;
            terminados++;
            if (verboso) printf("  t=%-3d P%d (%s) termina: RUNNING -> TERMINATED\n", t, actual->pid, actual->nombre);
            ultimo = actual;
            actual = NULL;
        } else if (alg == ALG_RR && usado == quantum) {
            /* Expropiacion: se le acabo el quantum y vuelve al final de la cola */
            actual->estado = READY;
            encolar(&listos, actual);
            if (verboso) printf("  t=%-3d P%d (%s) agota el quantum: RUNNING -> READY\n", t, actual->pid, actual->nombre);
            ultimo = actual;
            actual = NULL;
        }
    }

    r->tiempo_total = t;
    for (int i = 0; i < n; i++) {
        int retorno = procs[i].fin - procs[i].llegada;
        r->retorno_prom   += retorno;
        r->espera_prom    += retorno - procs[i].rafaga;
        r->respuesta_prom += procs[i].inicio - procs[i].llegada;
    }
    r->retorno_prom /= n;
    r->espera_prom /= n;
    r->respuesta_prom /= n;
}

/* ----------------------------------------------------------------- salida */

static void imprimir_gantt(const resultado_t *r)
{
    char barras[512], tiempos[512];
    int i = 0;
    while (i < r->n_gantt) {
        int pb = 0, pt = 0;
        while (i < r->n_gantt && pb < ANCHO_GANTT) {
            const segmento_t *s = &r->gantt[i];
            char etq[8];
            if (s->quien == OCIOSA)      strcpy(etq, "--");
            else if (s->quien == CAMBIO) strcpy(etq, "cc");
            else                         snprintf(etq, sizeof(etq), "P%d", s->quien);

            /* 2 columnas por unidad de tiempo, con un minimo para que entre la etiqueta */
            int ancho = 2 * (s->fin - s->ini);
            if (ancho < (int)strlen(etq) + 1) ancho = (int)strlen(etq) + 1;
            if (ancho > 40) ancho = 40;
            pb += sprintf(barras + pb, "|%-*s", ancho, etq);
            pt += sprintf(tiempos + pt, "%-*d", ancho + 1, s->ini);
            i++;
        }
        printf("  %s|\n  %s%d\n", barras, tiempos, r->gantt[i - 1].fin);
    }
}

static void imprimir_resultado(algoritmo_t alg, int quantum, const pcb_t *procs, int n, const resultado_t *r)
{
    printf("\n==================== %s", NOMBRE_ALG[alg]);
    if (alg == ALG_RR) printf(", quantum = %d", quantum);
    printf(" ====================\n\nDiagrama de Gantt:\n");
    imprimir_gantt(r);

    printf("\nOrden de despacho: ");
    for (int i = 0; i < r->n_despacho; i++) printf("%sP%d", i ? " -> " : "", r->despacho[i]);

    printf("\n\n  %-4s %-10s %8s %7s %10s %5s %8s %7s %10s  %s\n",
           "PID", "Nombre", "Llegada", "Rafaga", "Prioridad", "Fin", "Retorno", "Espera", "Respuesta", "Estado");
    for (int i = 0; i < n; i++) {
        const pcb_t *p = &procs[i];
        int retorno = p->fin - p->llegada;
        printf("  P%-3d %-10s %8d %7d %10d %5d %8d %7d %10d  %s\n",
               p->pid, p->nombre, p->llegada, p->rafaga, p->prioridad, p->fin,
               retorno, retorno - p->rafaga, p->inicio - p->llegada, nombre_estado(p->estado));
    }
    printf("\n  Promedios: espera = %.2f   retorno = %.2f   respuesta = %.2f\n",
           r->espera_prom, r->retorno_prom, r->respuesta_prom);
    printf("  Cambios de contexto: %d   Tiempo total: %d\n", r->cambios_contexto, r->tiempo_total);
}

static void imprimir_lote(const char *ruta, const pcb_t *lote, int n, int costo)
{
    printf("Lote: %s (%d procesos)", ruta, n);
    if (costo) printf("   Costo de cada cambio de contexto: %d", costo);
    printf("\n\n  %-4s %-10s %8s %7s %10s\n", "PID", "Nombre", "Llegada", "Rafaga", "Prioridad");
    for (int i = 0; i < n; i++)
        printf("  P%-3d %-10s %8d %7d %10d\n",
               lote[i].pid, lote[i].nombre, lote[i].llegada, lote[i].rafaga, lote[i].prioridad);
}

/* Round Robin con varios quantum: muestra el efecto del quantum y del costo del cambio */
static void barrido_quantum(const pcb_t *lote, int n, int costo)
{
    static const int QUANTUMS[] = { 1, 2, 3, 4, 6, 8, 12, 16 };
    static pcb_t procs[MAX_PROCESOS];
    static resultado_t r;

    printf("\n=========== Round Robin: efecto del quantum (costo de cambio de contexto = %d) ===========\n\n", costo);
    printf("  %-8s %10s %10s %11s %9s %13s\n", "Quantum", "Espera", "Retorno", "Respuesta", "Cambios", "Tiempo total");
    for (size_t i = 0; i < sizeof(QUANTUMS) / sizeof(QUANTUMS[0]); i++) {
        simular(ALG_RR, lote, n, QUANTUMS[i], costo, 0, procs, &r);
        printf("  %-8d %10.2f %10.2f %11.2f %9d %13d\n", QUANTUMS[i],
               r.espera_prom, r.retorno_prom, r.respuesta_prom, r.cambios_contexto, r.tiempo_total);
    }
    simular(ALG_FCFS, lote, n, 0, costo, 0, procs, &r);
    printf("  %-8s %10.2f %10.2f %11.2f %9d %13d\n", "FCFS",
           r.espera_prom, r.retorno_prom, r.respuesta_prom, r.cambios_contexto, r.tiempo_total);
}

int main(int argc, char *argv[])
{
    const char *ruta = "cargas/mixta.txt", *cual = "todos";
    int quantum = 3, costo = 0, verboso = 0, barrido = 0, opt;

    while ((opt = getopt(argc, argv, "f:a:q:c:vbh")) != -1) {
        switch (opt) {
        case 'f': ruta = optarg; break;
        case 'a': cual = optarg; break;
        case 'q': quantum = atoi(optarg); break;
        case 'c': costo = atoi(optarg); break;
        case 'v': verboso = 1; break;
        case 'b': barrido = 1; break;
        default:
            fprintf(stderr, "Uso: %s [-f archivo] [-a fcfs|sjf|prio|rr|todos] [-q quantum] [-c costo] [-v] [-b]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }
    if (quantum < 1 || costo < 0) { fprintf(stderr, "El quantum debe ser >= 1 y el costo >= 0\n"); return EXIT_FAILURE; }

    int elegido[CANT_ALG] = { 0 };
    if      (strcmp(cual, "todos") == 0) for (int a = 0; a < CANT_ALG; a++) elegido[a] = 1;
    else if (strcmp(cual, "fcfs") == 0)  elegido[ALG_FCFS] = 1;
    else if (strcmp(cual, "sjf") == 0)   elegido[ALG_SJF] = 1;
    else if (strcmp(cual, "prio") == 0)  elegido[ALG_PRIO] = 1;
    else if (strcmp(cual, "rr") == 0)    elegido[ALG_RR] = 1;
    else { fprintf(stderr, "Algoritmo desconocido: %s\n", cual); return EXIT_FAILURE; }

    static pcb_t lote[MAX_PROCESOS], procs[MAX_PROCESOS];
    static resultado_t res[CANT_ALG];
    int n = leer_lote(ruta, lote);

    imprimir_lote(ruta, lote, n, costo);

    if (barrido) {
        barrido_quantum(lote, n, costo);
        return EXIT_SUCCESS;
    }

    int corridos = 0;
    for (int a = 0; a < CANT_ALG; a++) {
        if (!elegido[a]) continue;
        if (verboso) printf("\nCambios de estado con %s:\n", SIGLA_ALG[a]);
        simular(a, lote, n, quantum, costo, verboso, procs, &res[a]);
        imprimir_resultado(a, quantum, procs, n, &res[a]);
        corridos++;
    }

    if (corridos > 1) {
        printf("\n==================== TABLA COMPARATIVA ====================\n\n");
        printf("  %-14s %10s %10s %11s %9s %13s\n", "Algoritmo", "Espera", "Retorno", "Respuesta", "Cambios", "Tiempo total");
        for (int a = 0; a < CANT_ALG; a++) {
            if (!elegido[a]) continue;
            char nombre[32];
            if (a == ALG_RR) snprintf(nombre, sizeof(nombre), "RR (q=%d)", quantum);
            else             snprintf(nombre, sizeof(nombre), "%s", SIGLA_ALG[a]);
            printf("  %-14s %10.2f %10.2f %11.2f %9d %13d\n", nombre, res[a].espera_prom,
                   res[a].retorno_prom, res[a].respuesta_prom, res[a].cambios_contexto, res[a].tiempo_total);
        }
        printf("\n  (promedios en unidades de tiempo; menos es mejor)\n");
    }
    return EXIT_SUCCESS;
}

/*
 * pcb.h - Bloque de Control de Proceso (PCB) del simulador.
 *
 * Caso: Optimizador de Tareas en una Nube Privada (Opcion 1 - Planificacion
 * de Procesos). Cada tarea batch del lote se representa con un PCB, igual que
 * el sistema operativo representa a cada proceso real.
 */
#ifndef PCB_H
#define PCB_H

#define MAX_PROCESOS 64
#define MAX_NOMBRE   16

/* Estados por los que pasa un proceso en la simulacion */
typedef enum {
    NUEVO,        /* todavia no llego al sistema                  */
    READY,        /* en la cola de listos, esperando la CPU       */
    RUNNING,      /* usando la CPU                                */
    TERMINATED    /* termino su rafaga                            */
} estado_t;

typedef struct {
    /* --- Atributos que pide la consigna --- */
    int      pid;
    char     nombre[MAX_NOMBRE];
    int      prioridad;     /* numero mas chico = mas prioridad     */
    int      llegada;       /* instante en que llega al sistema     */
    int      rafaga;        /* tiempo total de CPU que necesita     */
    estado_t estado;

    /* --- Contabilidad para calcular las metricas --- */
    int      restante;      /* rafaga que todavia le falta ejecutar */
    int      inicio;        /* primera vez que uso la CPU (-1 = nunca) */
    int      fin;           /* instante en que termino              */
} pcb_t;

static inline const char *nombre_estado(estado_t e)
{
    switch (e) {
    case NUEVO:      return "NUEVO";
    case READY:      return "READY";
    case RUNNING:    return "RUNNING";
    case TERMINATED: return "TERMINATED";
    default:         return "?";
    }
}

#endif /* PCB_H */

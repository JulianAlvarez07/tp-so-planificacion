/*
 * cola.h - Cola de procesos preparados (READY).
 *
 * Guarda punteros a PCB en orden de llegada a la cola. FCFS y Round Robin
 * sacan siempre al primero; SJF y Prioridades recorren la cola y sacan al
 * que corresponde segun su criterio.
 */
#ifndef COLA_H
#define COLA_H

#include "pcb.h"

typedef struct {
    pcb_t *items[MAX_PROCESOS];
    int    cantidad;
} cola_t;

void   cola_iniciar(cola_t *c);
int    cola_vacia(const cola_t *c);
void   encolar(cola_t *c, pcb_t *p);          /* agrega al final            */
pcb_t *desencolar(cola_t *c);                 /* saca al primero            */
pcb_t *cola_extraer(cola_t *c, int pos);      /* saca al de la posicion pos */

#endif /* COLA_H */

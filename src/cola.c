/*
 * cola.c - Implementacion de la cola de listos sobre un arreglo.
 */
#include <stddef.h>

#include "cola.h"

void cola_iniciar(cola_t *c)
{
    c->cantidad = 0;
}

int cola_vacia(const cola_t *c)
{
    return c->cantidad == 0;
}

void encolar(cola_t *c, pcb_t *p)
{
    if (c->cantidad < MAX_PROCESOS)
        c->items[c->cantidad++] = p;
}

/* Saca al de la posicion pos y corre a los de atras un lugar hacia adelante,
 * para que los demas conserven su orden de llegada a la cola. */
pcb_t *cola_extraer(cola_t *c, int pos)
{
    if (pos < 0 || pos >= c->cantidad) return NULL;
    pcb_t *p = c->items[pos];
    for (int i = pos; i < c->cantidad - 1; i++)
        c->items[i] = c->items[i + 1];
    c->cantidad--;
    return p;
}

pcb_t *desencolar(cola_t *c)
{
    return cola_extraer(c, 0);
}

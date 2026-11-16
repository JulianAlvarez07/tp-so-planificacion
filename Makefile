CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -std=c11 -g

all: planificador

planificador: src/planificador.c src/cola.c src/cola.h src/pcb.h
	$(CC) $(CFLAGS) -o $@ src/planificador.c src/cola.c

clean:
	rm -f planificador planificador.exe

.PHONY: all clean

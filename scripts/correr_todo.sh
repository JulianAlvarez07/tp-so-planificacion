#!/usr/bin/env bash
# correr_todo.sh - Ejecuta el simulador con los tres lotes y guarda las salidas
# en resultados/. Tambien corre el barrido de quantum con costo de cambio de
# contexto.
#
# Uso: ./scripts/correr_todo.sh
set -eu
cd "$(dirname "$0")/.."
make -s
mkdir -p resultados

for LOTE in mixta convoy parejas; do
    ./planificador -f "cargas/$LOTE.txt" > "resultados/$LOTE.txt"
    echo "=== $LOTE ==="
    sed -n '/TABLA COMPARATIVA/,$p' "resultados/$LOTE.txt"
done

./planificador -f cargas/mixta.txt -b -c 1 > resultados/barrido_quantum.txt
sed -n '/Round Robin: efecto/,$p' resultados/barrido_quantum.txt
echo
echo "Salidas completas en resultados/"

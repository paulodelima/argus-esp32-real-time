#!/usr/bin/env bash
# Escala com solucionadores EXTERNOS via SMT-LIB (--smtlib --smtlib-solver-prog).
# Mesma grade e configuracao do run_escala.sh. Rodar com o Mac livre:
#   bash verification/esbmc/run_escala_ext.sh "yices" "yices-smt2"
# 1o argumento: nome no CSV; 2o: comando do solucionador (entre aspas).
# Obs.: o tempo inclui escrever/ler a formula em SMT-LIB (declarar no artigo).
set -u
NAME=${1:?nome}; PROG=${2:?comando}
H=verification/esbmc/argus_harness.c
OUT=verification/logs/escala; mkdir -p "$OUT"
one() {
  local steps=$1 dets=$2
  local log="$OUT/${NAME}_f${steps}_d${dets}.log" t0=$SECONDS v
  esbmc $H firmware_v2/argus_fsm.c -I"$PWD/firmware_v2" -DARGUS_V2 -DENV_E2 -DFIX_Y \
    -DNO_P4 -DNO_P5 -DSTEPS=$steps -DMAX_DETS=$dets \
    --no-bounds-check --no-pointer-check --no-align-check \
    --smtlib --smtlib-solver-prog "$PROG" --unwind 9 --timeout 5m > "$log" 2>&1
  local dt=$((SECONDS - t0))
  if grep -q "VERIFICATION FAILED" "$log"; then v=FALHA
  elif grep -q "VERIFICATION SUCCESSFUL" "$log"; then v=PASSA
  elif [ "$dt" -lt 10 ]; then v="ERRO:$(grep -m1 -i error "$log" | tr ',' ';' | cut -c1-60)"
  else v=SEM_VEREDICTO; fi
  local vcc dp
  vcc=$(grep -o 'Generated [0-9]* VCC' "$log" | grep -o '[0-9]*' | tail -1)
  dp=$(grep -o 'Runtime decision procedure: [0-9.]*' "$log" | grep -o '[0-9.]*$' | tail -1)
  echo "$NAME,$steps,$dets,$v,$dt,${vcc:-},${dp:-}"
}
export -f one; export H OUT NAME PROG
F="$OUT/escala_${NAME}.csv"
echo "solver,frames,objetos_por_frame,veredicto,tempo_total_s,vccs,tempo_solver_s" > "$F"
{ for f in 2 3 4 5 6 7; do echo "$f 1"; done; for d in 2 3 4; do echo "4 $d"; done; } \
| xargs -P 2 -n 2 bash -c 'one "$@"' _ | tee -a "$F"

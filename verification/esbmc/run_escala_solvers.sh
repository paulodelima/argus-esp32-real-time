#!/usr/bin/env bash
# Escala com os demais solucionadores (mesma configuracao do run_escala.sh).
# Rodar com o Mac livre:  bash verification/esbmc/run_escala_solvers.sh
set -u
H=verification/esbmc/argus_harness.c
OUT=verification/logs/escala; mkdir -p "$OUT"
one() {
  local solver=$1 steps=$2 dets=$3
  local log="$OUT/${solver}_f${steps}_d${dets}.log" t0=$SECONDS v
  esbmc $H firmware_v2/argus_fsm.c -I"$PWD/firmware_v2" -DARGUS_V2 -DENV_E2 -DFIX_Y \
    -DNO_P4 -DNO_P5 -DSTEPS=$steps -DMAX_DETS=$dets \
    --no-bounds-check --no-pointer-check --no-align-check \
    --$solver --unwind 9 --timeout 5m > "$log" 2>&1
  local dt=$((SECONDS - t0))
  if grep -q "VERIFICATION FAILED" "$log"; then v=FALHA
  elif grep -q "VERIFICATION SUCCESSFUL" "$log"; then v=PASSA
  elif [ "$dt" -lt 10 ]; then v="ERRO:$(tail -1 "$log" | tr ',' ';' | cut -c1-60)"
  else v=SEM_VEREDICTO; fi
  local vcc dp
  vcc=$(grep -o 'Generated [0-9]* VCC' "$log" | grep -o '[0-9]*' | tail -1)
  dp=$(grep -o 'Runtime decision procedure: [0-9.]*' "$log" | grep -o '[0-9.]*$' | tail -1)
  echo "$solver,$steps,$dets,$v,$dt,${vcc:-},${dp:-}"
}
export -f one; export H OUT
F="$OUT/escala_solvers.csv"
echo "solver,frames,objetos_por_frame,veredicto,tempo_total_s,vccs,tempo_solver_s" > "$F"
{
  for s in yices cvc5 cvc4 mathsat; do
    for f in 2 3 4 5; do echo "$s $f 1"; done
    echo "$s 3 2"
  done
} | xargs -P 2 -n 3 bash -c 'one "$@"' _ | tee -a "$F"

#!/usr/bin/env bash
# Prova sem limite de frames com invariante de laco anotada (__ESBMC_loop_invariant).
# Executar na raiz:  bash verification/esbmc/run_loopinv.sh
# Firmware corrigido (v2), ambiente realista, 1 objeto por frame, y fixo.
# Modo havoc (--loop-invariant-check): corta o laco em 1 volta a partir de
# qualquer estado que satisfaca a invariante. 1 execucao por vez, ate 10 min.
set -u
H=verification/esbmc/argus_harness.c
OUT=verification/logs/loopinv; mkdir -p "$OUT"
run() {  # nome, modo, propriedades-desligadas...
  local name=$1 mode=$2; shift 2
  local log="$OUT/$name.log" t0=$SECONDS
  esbmc $H firmware_v2/argus_fsm.c -I"$PWD/firmware_v2" -DARGUS_V2 \
    -DENV_E2 -DUNBOUNDED -DLOOP_INV -DMAX_DETS=1 -DFIX_Y "$@" \
    --no-bounds-check --no-pointer-check --no-align-check \
    --boolector $mode --unwind 9 --timeout 10m > "$log" 2>&1
  local v; v=$(grep -oE "VERIFICATION (SUCCESSFUL|FAILED|UNKNOWN)" "$log" | tail -1)
  local why; why=$(grep -A2 "Violated property" "$log" | tail -1 | sed 's/^ *//')
  printf "%-10s %-24s %5ss  %s\n" "$name" "${v:-SEM_VEREDICTO}" "$((SECONDS - t0))" "$why" | tee -a "$OUT/RESUMO.txt"
}
: > "$OUT/RESUMO.txt"
run P3_chk --loop-invariant-check -DNO_P4 -DNO_P5
run P5_chk --loop-invariant-check -DNO_P3 -DNO_P4
run P4_chk --loop-invariant-check -DNO_P3 -DNO_P5

#!/usr/bin/env bash
# Prova sem limite de frames por inducao, escrita a mao no harness:
#  BASE: o estado inicial satisfaz a invariante;
#  PASSO: de QUALQUER estado que satisfaca a invariante, um frame preserva a
#         invariante e as propriedades.  Executar na raiz:
#   bash verification/esbmc/run_induct.sh
set -u
H=verification/esbmc/argus_harness.c
OUT=verification/logs/induct; mkdir -p "$OUT"
: > "$OUT/RESUMO.txt"
run() {  # nome, opcoes...
  local name=$1; shift; local log="$OUT/$name.log" t0=$SECONDS v why
  esbmc $H firmware_v2/argus_fsm.c -I"$PWD/firmware_v2" -DARGUS_V2 -DENV_E2 -DINV \
    -DMAX_DETS=1 -DFIX_Y "$@" \
    --no-bounds-check --no-pointer-check --no-align-check \
    --boolector --unwind 9 --timeout 15m > "$log" 2>&1
  if grep -q "VERIFICATION FAILED" "$log"; then v=FALHA
  elif grep -q "VERIFICATION SUCCESSFUL" "$log"; then v=PASSA; else v=SEM_VEREDICTO; fi
  why=$(grep -A2 "Violated property" "$log" | tail -1 | sed 's/^ *//')
  printf "%-10s %-13s %4ss  %s\n" "$name" "$v" "$((SECONDS - t0))" "$why" | tee -a "$OUT/RESUMO.txt"
}
run base     -DSTEPS=0 -DNO_P3 -DNO_P4 -DNO_P5
run passo_P3 -DINDUCT_STEP -DSTEPS=1 -DNO_P4 -DNO_P5
run passo_P5 -DINDUCT_STEP -DSTEPS=1 -DNO_P3 -DNO_P4
run passo_P4 -DINDUCT_STEP -DSTEPS=1 -DNO_P3 -DNO_P5

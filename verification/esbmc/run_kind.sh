#!/usr/bin/env bash
# Prova sem limite de frames (k-indução). Executar na raiz do repositório:
#   bash verification/esbmc/run_kind.sh
# O laço de frames vira while(1) (-DUNBOUNDED). Mesmas hipóteses da tabela:
# ambiente realista, até 1 objeto por frame, y fixo.
set -u
H=verification/esbmc/argus_harness.c
OUT=verification/logs/kind; mkdir -p "$OUT"
BASE="-DENV_E2 -DUNBOUNDED -DMAX_DETS=1 -DFIX_Y --no-bounds-check --no-pointer-check --no-align-check --boolector --k-induction --max-k-step 12 --timeout 10m"

run() {  # nome, pasta, flag, propriedade-off...
  local name=$1 dir=$2 flag=$3; shift 3
  local log="$OUT/$name.log" t0=$SECONDS
  esbmc $H "$dir/argus_fsm.c" -I"$PWD/$dir" $flag $BASE "$@" > "$log" 2>&1
  local v; v=$(grep -oE "VERIFICATION (SUCCESSFUL|FAILED|UNKNOWN)|Timed out" "$log" | tail -1)
  local k; k=$(grep -oE "(Solution found by the [a-z ]+ step|k = [0-9]+)" "$log" | tail -1)
  printf "%-14s %-26s %5ss  %s\n" "$name" "${v:-SEM VEREDICTO}" "$((SECONDS - t0))" "$k" | tee -a "$OUT/RESUMO.txt"
}

: > "$OUT/RESUMO.txt"
run v2_P5_kind firmware_v2 -DARGUS_V2 -DNO_P3 -DNO_P4
run v2_P3_kind firmware_v2 -DARGUS_V2 -DNO_P4 -DNO_P5
run v1_P3_kind firmware    ""         -DNO_P4 -DNO_P5
run v1_P5_kind firmware    ""         -DNO_P3 -DNO_P4
echo; echo "Resumo em $OUT/RESUMO.txt (detalhes nos .log)"

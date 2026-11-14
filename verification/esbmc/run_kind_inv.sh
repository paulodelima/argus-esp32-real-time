#!/usr/bin/env bash
# k-indução com invariante indutiva (-DINV). Executar na raiz do repositório:
#   bash verification/esbmc/run_kind_inv.sh
# Roda 2 execuções por vez, até 15 min cada (no máximo ~30 min no total).
set -u
H=verification/esbmc/argus_harness.c
OUT=verification/logs/kind_inv; mkdir -p "$OUT"

run() {  # nome, pasta, flag-da-versao, propriedades-desligadas...
  local name=$1 dir=$2 flag=$3; shift 3
  local log="$OUT/$name.log" t0=$SECONDS
  esbmc $H "$dir/argus_fsm.c" -I"$PWD/$dir" $flag \
    -DENV_E2 -DUNBOUNDED -DINV -DMAX_DETS=1 -DFIX_Y "$@" \
    --no-bounds-check --no-pointer-check --no-align-check \
    --boolector --k-induction --max-k-step 12 --timeout 15m > "$log" 2>&1
  local v; v=$(grep -oE "VERIFICATION (SUCCESSFUL|FAILED|UNKNOWN)|Timed out" "$log" | tail -1)
  local fase; fase=$(grep -E "^Checking (base case|forward condition|inductive step), k = [0-9]+|Solution found by the [a-z ]+ step|Violated property" "$log" | tail -1)
  printf "%-14s %-24s %5ss  %s\n" "$name" "${v:-SEM VEREDICTO}" "$((SECONDS - t0))" "$fase" >> "$OUT/RESUMO.txt"
}

: > "$OUT/RESUMO.txt"
run v2_P5_inv firmware_v2 -DARGUS_V2 -DNO_P3 -DNO_P4 &
run v1_P5_inv firmware    ""         -DNO_P3 -DNO_P4 &
wait
run v2_P3_inv firmware_v2 -DARGUS_V2 -DNO_P4 -DNO_P5 &
run v1_P3_inv firmware    ""         -DNO_P4 -DNO_P5 &
wait
cat "$OUT/RESUMO.txt"

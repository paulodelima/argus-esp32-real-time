#!/usr/bin/env bash
# Invariante de laco com o estado em variaveis globais (-DGLOBAL_STATE).
# Executar na raiz:  bash verification/esbmc/run_loopinv2.sh
set -u
H=verification/esbmc/argus_harness.c
OUT=verification/logs/loopinv2; mkdir -p "$OUT"
: > "$OUT/RESUMO.txt"
for cfg in "P3_chk -DNO_P4 -DNO_P5" "P5_chk -DNO_P3 -DNO_P4" "P4_chk -DNO_P3 -DNO_P5"; do
  set -- $cfg; name=$1; shift; log="$OUT/$name.log"; t0=$SECONDS
  esbmc $H firmware_v2/argus_fsm.c -I"$PWD/firmware_v2" -DARGUS_V2 \
    -DENV_E2 -DUNBOUNDED -DLOOP_INV -DGLOBAL_STATE -DMAX_DETS=1 -DFIX_Y "$@" \
    --no-bounds-check --no-pointer-check --no-align-check \
    --boolector --loop-invariant-check --unwind 9 --timeout 10m > "$log" 2>&1
  v=$(grep -oE "VERIFICATION (SUCCESSFUL|FAILED|UNKNOWN)" "$log" | tail -1)
  printf "%-7s %-24s %4ss\n" "$name" "${v:-SEM_VEREDICTO}" "$((SECONDS - t0))" | tee -a "$OUT/RESUMO.txt"
  grep -m3 -i -E "warning|invariant" "$log" | sed 's/^/    /' | tee -a "$OUT/RESUMO.txt"
done

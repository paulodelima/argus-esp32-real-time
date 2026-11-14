#!/usr/bin/env bash
# Repete so o passo indutivo de "nenhum bip perdido" com a invariante reforcada (ids).
#   bash verification/esbmc/run_induct_ids.sh
set -u
H=verification/esbmc/argus_harness.c
OUT=verification/logs/induct; mkdir -p "$OUT"; log="$OUT/passo_P4_ids.log"; t0=$SECONDS
esbmc $H firmware_v2/argus_fsm.c -I"$PWD/firmware_v2" -DARGUS_V2 -DENV_E2 -DINV -DINV_IDS \
  -DMAX_DETS=1 -DFIX_Y -DINDUCT_STEP -DSTEPS=1 -DNO_P3 -DNO_P5 \
  --no-bounds-check --no-pointer-check --no-align-check \
  --boolector --unwind 9 --timeout 15m > "$log" 2>&1
if grep -q "VERIFICATION FAILED" "$log"; then v=FALHA
elif grep -q "VERIFICATION SUCCESSFUL" "$log"; then v=PASSA; else v=SEM_VEREDICTO; fi
printf "passo_P4_ids %-13s %4ss  %s\n" "$v" "$((SECONDS - t0))" "$(grep -A2 'Violated property' "$log" | tail -1 | sed 's/^ *//')" | tee -a "$OUT/RESUMO.txt"

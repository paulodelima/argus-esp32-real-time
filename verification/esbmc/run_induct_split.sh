#!/usr/bin/env bash
# Passo indutivo dividido em formulas menores (2 em paralelo, ate 15 min cada).
#   bash verification/esbmc/run_induct_split.sh
set -u
H=verification/esbmc/argus_harness.c
OUT=verification/logs/induct; mkdir -p "$OUT"
one() {
  IFS='|' read -r name solver flags <<< "$1"
  local log="$OUT/$name.log" t0=$SECONDS v
  esbmc $H firmware_v2/argus_fsm.c -I"$PWD/firmware_v2" -DARGUS_V2 -DENV_E2 -DINV -DINV_IDS \
    -DMAX_DETS=1 -DFIX_Y -DINDUCT_STEP -DSTEPS=1 -DNO_P3 -DNO_P5 $flags \
    --no-bounds-check --no-pointer-check --no-align-check \
    --$solver --unwind 9 --timeout 15m > "$log" 2>&1
  if grep -q "VERIFICATION FAILED" "$log"; then v=FALHA
  elif grep -q "VERIFICATION SUCCESSFUL" "$log"; then v=PASSA; else v=SEM_VEREDICTO; fi
  printf "%-16s %-13s %4ss  %s\n" "$name" "$v" "$((SECONDS - t0))" \
    "$(grep -A2 'Violated property' "$log" | tail -1 | sed 's/^ *//')"
}
export -f one; export H OUT
printf '%s\n' \
  "so_bip_perdido|boolector|-DNO_INV_ASSERT" \
  "so_invariante|boolector|-DNO_P4" \
  "so_bip_bitwuzla|bitwuzla|-DNO_INV_ASSERT" \
  "so_inv_bitwuzla|bitwuzla|-DNO_P4" \
| xargs -P 2 -I{} bash -c 'one "{}"' | tee -a "$OUT/RESUMO.txt"

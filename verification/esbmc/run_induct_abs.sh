#!/usr/bin/env bash
# EXPLORATORIO (abstracao a aprovar): mesmo passo indutivo com o firmware
# corrigido limitado a K slots de objetos (copia em verification/abs/kK).
#   bash verification/esbmc/run_induct_abs.sh
set -u
H=verification/esbmc/argus_harness.c
OUT=verification/logs/induct_abs; mkdir -p "$OUT"
: > "$OUT/RESUMO.txt"
for K in 2 4; do
  D=verification/abs/k$K; mkdir -p "$D"
  cp firmware_v2/argus_fsm.c "$D/"
  sed "s/#define ARGUS_MAX_TRACKS    8/#define ARGUS_MAX_TRACKS    $K/" firmware_v2/argus_fsm.h > "$D/argus_fsm.h"
  grep -q "ARGUS_MAX_TRACKS    $K" "$D/argus_fsm.h" || { echo "sed falhou para K=$K"; exit 1; }
  for P in "bip -DNO_INV_ASSERT" "inv -DNO_P4"; do
    set -- $P; name="k${K}_$1"; log="$OUT/$name.log"; t0=$SECONDS
    esbmc $H "$D/argus_fsm.c" -I"$PWD/$D" -DARGUS_V2 -DENV_E2 -DINV -DINV_IDS \
      -DMAX_DETS=1 -DFIX_Y -DINDUCT_STEP -DSTEPS=1 -DNO_P3 -DNO_P5 $2 \
      --no-bounds-check --no-pointer-check --no-align-check \
      --boolector --unwind 9 --timeout 15m > "$log" 2>&1
    if grep -q "VERIFICATION FAILED" "$log"; then v=FALHA
    elif grep -q "VERIFICATION SUCCESSFUL" "$log"; then v=PASSA; else v=SEM_VEREDICTO; fi
    printf "%-8s %-13s %4ss  %s\n" "$name" "$v" "$((SECONDS - t0))" \
      "$(grep -A2 'Violated property' "$log" | tail -1 | sed 's/^ *//')" | tee -a "$OUT/RESUMO.txt"
    [ "$v" = SEM_VEREDICTO ] && [ "$K" = 2 ] && echo "  (com 2 slots ainda nao fecha; K=4 sera pior)"
  done
done

#!/usr/bin/env bash
# Passo indutivo (INV_IDS) com K slots: K=3 completa a serie 2-3-4; K=8 e o
# firmware real, com timeout longo. Um caso por vez, maquina livre.
#   bash verification/esbmc/run_slots.sh
set -u
H=verification/esbmc/argus_harness.c
OUT=verification/logs/slots; mkdir -p "$OUT"
: > "$OUT/RESUMO.txt"
run() {   # K nome flags timeout
  local K=$1 name=$2 flags=$3 to=$4 D
  if [ "$K" = 8 ]; then D=firmware_v2; else
    D=verification/abs/k$K; mkdir -p "$D"; cp firmware_v2/argus_fsm.c "$D/"
    sed "s/#define ARGUS_MAX_TRACKS    8/#define ARGUS_MAX_TRACKS    $K/" firmware_v2/argus_fsm.h > "$D/argus_fsm.h"
    grep -q "ARGUS_MAX_TRACKS    $K" "$D/argus_fsm.h" || { echo "sed falhou K=$K"; exit 1; }
  fi
  local log="$OUT/k${K}_$name.log" t0=$SECONDS v
  esbmc $H "$D/argus_fsm.c" -I"$PWD/$D" -DARGUS_V2 -DENV_E2 -DINV -DINV_IDS \
    -DMAX_DETS=1 -DFIX_Y -DINDUCT_STEP -DSTEPS=1 -DNO_P3 -DNO_P5 $flags \
    --no-bounds-check --no-pointer-check --no-align-check \
    --boolector --unwind 9 --timeout $to > "$log" 2>&1
  if grep -q "VERIFICATION FAILED" "$log"; then v=FALHA
  elif grep -q "VERIFICATION SUCCESSFUL" "$log"; then v=PASSA; else v=SEM_VEREDICTO; fi
  printf "k%s_%-4s %-13s %6ss  %s\n" "$K" "$name" "$v" "$((SECONDS - t0))" \
    "$(grep -A2 'Violated property' "$log" | tail -1 | sed 's/^ *//')" | tee -a "$OUT/RESUMO.txt"
}
run 3 bip "-DNO_INV_ASSERT" 30m
run 3 inv "-DNO_P4" 30m
run 8 bip "-DNO_INV_ASSERT" 150m
run 8 inv "-DNO_P4" 150m

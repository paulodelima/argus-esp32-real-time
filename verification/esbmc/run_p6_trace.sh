#!/usr/bin/env bash
# P6 em tracos concretos: k objetos somem no mesmo frame; varia k e o periodo do quadro.
# Executar na raiz:  bash verification/esbmc/run_p6_trace.sh
set -u
H=verification/esbmc/argus_harness.c
OUT=verification/logs/p6_trace; mkdir -p "$OUT"
: > "$OUT/RESUMO.txt"
for cfg in "1 50000" "2 50000" "3 50000" "2 100000" "2 120000"; do
  set -- $cfg; k=$1; tf=$2; log="$OUT/k${k}_T${tf}.log"; t0=$SECONDS
  esbmc $H firmware_v2/argus_fsm.c -I"$PWD/firmware_v2" -DARGUS_V2 -DENV_E2 -DP6 \
    -DP6_TRACE=$k -DMAX_DETS=3 -DSTEPS=5 -DT_FRAME_US=$tf -DNO_P3 -DNO_P4 -DNO_P5 \
    --no-bounds-check --no-pointer-check --no-align-check \
    --boolector --unwind 9 --timeout 5m > "$log" 2>&1
  if grep -q "VERIFICATION FAILED" "$log"; then v=FALHA
  elif grep -q "VERIFICATION SUCCESSFUL" "$log"; then v=PASSA; else v=SEM_VEREDICTO; fi
  printf "objetos=%s T_frame=%sus  %-13s %4ss\n" "$k" "$tf" "$v" "$((SECONDS - t0))" | tee -a "$OUT/RESUMO.txt"
done

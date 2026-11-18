#!/usr/bin/env bash
# P6 com entrada em QUALQUER frame (busca simbolica): em todo frame entram de
# 0 a MAX_DETS objetos em posicoes x quaisquer (y fixo). Mede ate onde o ESBMC
# termina, para declarar o limite. Executar na raiz:
#   bash verification/esbmc/run_p6_any.sh
# Linha: nome | objetos max por frame | frames | T_FRAME_US
set -u
H=verification/esbmc/argus_harness.c
OUT=verification/logs/p6_any; mkdir -p "$OUT"
one() {
  IFS='|' read -r name dets steps tf <<< "$1"
  local log="$OUT/$name.log" t0=$SECONDS v
  esbmc $H firmware_v2/argus_fsm.c -I"$PWD/firmware_v2" -DARGUS_V2 -DENV_E2 -DP6 \
    -DP6_SYM=$steps -DMAX_DETS=$dets -DSTEPS=$steps -DT_FRAME_US=$tf -DNO_P3 -DNO_P4 -DNO_P5 \
    --no-bounds-check --no-pointer-check --no-align-check \
    --boolector --unwind 10 --timeout 15m > "$log" 2>&1
  if grep -q "VERIFICATION FAILED" "$log"; then v=FALHA
  elif grep -q "VERIFICATION SUCCESSFUL" "$log"; then v=PASSA; else v=SEM_VEREDICTO; fi
  printf "%-3s ate %s objeto(s) em QUALQUER frame  frames=%s T=%sus  %-13s %4ss\n" \
    "$name" "$dets" "$steps" "$tf" "$v" "$((SECONDS - t0))"
}
export -f one; export H OUT
printf '%s\n' \
  "A5|1|5|50000" "A6|1|6|50000" "A7|1|7|50000" "A8|1|8|50000" \
  "B5|2|5|50000" "B6|2|6|50000" \
| xargs -P 2 -I{} bash -c 'one "{}"' | tee "$OUT/RESUMO.txt"

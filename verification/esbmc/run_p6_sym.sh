#!/usr/bin/env bash
# P6 com busca simbolica: nos primeiros frames entram de 0 a MAX_DETS objetos em
# posicoes x quaisquer (y fixo); depois nenhum. Executar na raiz:
#   bash verification/esbmc/run_p6_sym.sh
# Linha: nome | frames com entrada | objetos max por frame | frames total | T_FRAME_US
set -u
H=verification/esbmc/argus_harness.c
OUT=verification/logs/p6_sym; mkdir -p "$OUT"
one() {
  IFS='|' read -r name ent dets steps tf <<< "$1"
  local log="$OUT/$name.log" t0=$SECONDS v
  esbmc $H firmware_v2/argus_fsm.c -I"$PWD/firmware_v2" -DARGUS_V2 -DENV_E2 -DP6 \
    -DP6_SYM=$ent -DMAX_DETS=$dets -DSTEPS=$steps -DT_FRAME_US=$tf -DNO_P3 -DNO_P4 -DNO_P5 \
    --no-bounds-check --no-pointer-check --no-align-check \
    --boolector --unwind 9 --timeout 15m > "$log" 2>&1
  if grep -q "VERIFICATION FAILED" "$log"; then v=FALHA
  elif grep -q "VERIFICATION SUCCESSFUL" "$log"; then v=PASSA; else v=SEM_VEREDICTO; fi
  printf "%-3s entrada=%s frame(s) ate %s objetos  frames=%s T=%sus  %-13s %4ss\n" \
    "$name" "$ent" "$dets" "$steps" "$tf" "$v" "$((SECONDS - t0))"
}
export -f one; export H OUT
printf '%s\n' \
  "S1|1|2|5|50000" \
  "S2|1|3|5|50000" \
  "S3|1|2|5|100000" \
  "S4|1|2|5|120000" \
  "S5|2|1|6|50000" \
| xargs -P 2 -I{} bash -c 'one "{}"' | tee "$OUT/RESUMO.txt"

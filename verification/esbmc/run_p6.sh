#!/usr/bin/env bash
# P6: prazo de 800 ms ponta a ponta (ultimo quadro com o objeto -> inicio do bip).
# Executar na raiz do repositorio:  bash verification/esbmc/run_p6.sh
# Firmware corrigido (v2), ambiente realista (E2), y fixo, so P6 ligada.
# Cada linha: nome | STEPS | MAX_DETS | T_FRAME_US (hipotese do periodo do quadro)
set -u
H=verification/esbmc/argus_harness.c
OUT=verification/logs/p6; mkdir -p "$OUT"
one() {
  IFS='|' read -r name steps dets tf <<< "$1"
  local log="$OUT/$name.log" t0=$SECONDS v
  esbmc $H firmware_v2/argus_fsm.c -I"$PWD/firmware_v2" -DARGUS_V2 -DENV_E2 -DFIX_Y -DP6 \
    -DNO_P3 -DNO_P4 -DNO_P5 -DSTEPS=$steps -DMAX_DETS=$dets -DT_FRAME_US=$tf \
    --no-bounds-check --no-pointer-check --no-align-check \
    --boolector --unwind 9 --timeout 10m > "$log" 2>&1
  if grep -q "VERIFICATION FAILED" "$log"; then v="FALHA"
  elif grep -q "VERIFICATION SUCCESSFUL" "$log"; then v="PASSA"
  else v="SEM_VEREDICTO"; fi
  printf "%-4s frames=%s objetos/frame=%s T_frame=%sus  %-13s %4ss  %s\n" "$name" "$steps" "$dets" "$tf" "$v" \
    "$((SECONDS - t0))" "$(grep -A2 'Violated property' "$log" | tail -1 | sed 's/^ *//')"
}
export -f one; export H OUT
printf '%s\n' \
  "A|5|1|50000"  \
  "B|5|2|50000"  \
  "C|5|3|50000"  \
  "D|7|1|50000"  \
  "E|5|2|100000" \
  "F|5|2|120000" \
| xargs -P 3 -I{} bash -c 'one "{}"' | tee "$OUT/RESUMO.txt"

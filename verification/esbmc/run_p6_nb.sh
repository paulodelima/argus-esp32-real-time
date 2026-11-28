#!/usr/bin/env bash
# P6 com o buzzer NAO bloqueante (firmware_v3, -DBEEP_NB) + P9 (agenda do buzzer).
# Mesmos cenarios da v2 (run_p6_sym.sh e run_p6_any.sh) para comparar, e o pulso
# encurtado para 120 ms (80 ms de tom + 40 ms de silencio). Um caso por vez.
#   bash verification/esbmc/run_p6_nb.sh
# Linha: nome | frames com entrada | objetos max por frame | frames | T_FRAME_US | periodo do pulso (us) | timeout
set -u
H=verification/esbmc/argus_harness.c
F=firmware_v3
OUT=verification/logs/p6_nb; mkdir -p "$OUT"
: > "$OUT/RESUMO.txt"
one() {
  IFS='|' read -r name ent dets steps tf per to <<< "$1"
  local log="$OUT/$name.log" t0=$SECONDS v
  esbmc $H $F/argus_fsm.c $F/argus_buzzer.c -I"$PWD/$F" -DARGUS_V2 -DENV_E2 -DP6 -DBEEP_NB -DARGUS_NO_STDINT \
    -DP6_SYM=$ent -DMAX_DETS=$dets -DSTEPS=$steps -DT_FRAME_US=$tf -DBEEP_PERIOD_US=$per \
    -DNO_P3 -DNO_P4 -DNO_P5 \
    --no-bounds-check --no-pointer-check --no-align-check \
    --boolector --unwind 10 --timeout $to > "$log" 2>&1
  if grep -q "VERIFICATION FAILED" "$log"; then v=FALHA
  elif grep -q "VERIFICATION SUCCESSFUL" "$log"; then v=PASSA; elif grep -q "^ERROR" "$log"; then v=ERRO_ESBMC; else v=SEM_VEREDICTO; fi
  printf "%-3s entrada=%s frame(s) ate %s obj  frames=%s T=%sus pulso=%sus  %-13s %5ss  %s\n" \
    "$name" "$ent" "$dets" "$steps" "$tf" "$per" "$v" "$((SECONDS - t0))" \
    "$(grep -A2 'Violated property' "$log" | tail -1 | sed 's/^ *//')" | tee -a "$OUT/RESUMO.txt"
}
for c in \
  "N1|1|2|5|50000|180000|15m" \
  "N2|1|3|5|50000|180000|15m" \
  "N3|1|3|5|50000|120000|15m" \
  "N4|1|4|5|50000|120000|20m" \
  "N5|1|2|5|120000|180000|15m" \
  "N6|6|1|6|50000|180000|20m" \
  "N7|5|2|5|50000|180000|30m" ; do one "$c"; done

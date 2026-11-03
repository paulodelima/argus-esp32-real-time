#!/usr/bin/env bash
# Bateria rápida para o status de 03/11. Roda em sequência, com Bitwuzla.
# Executar na raiz do repositório:  bash verification/esbmc/run_rapido.sh
# Cada execução tem limite de 10 min (--timeout); a saída vai para verification/logs/.
set -u
H=verification/esbmc/argus_harness.c
OUT=verification/logs; mkdir -p "$OUT"
NOMEM="--no-bounds-check --no-pointer-check --no-align-check"
BASE="--bitwuzla --unwind 9 --timeout 10m"
V1="firmware/argus_fsm.c -I$PWD/firmware"
V2="firmware_v2/argus_fsm.c -I$PWD/firmware_v2 -DARGUS_V2"

run() {  # nome, args...
  local name="$1"; shift
  ( local t0=$SECONDS
    { echo "# esbmc $*"; esbmc --version; } > "$OUT/$name.log" 2>&1
    esbmc "$@" >> "$OUT/$name.log" 2>&1
    echo "# tempo_s=$((SECONDS - t0))" >> "$OUT/$name.log" )
}

# P5 (ordem dos estados), ambiente realista, 1 objeto por frame, 5 frames (minimo para encerrar um track)
run v1_E2_P5_n1_s5  $H $V1 -DENV_E2 -DSTEPS=5 -DMAX_DETS=1 -DNO_P3 -DNO_P4 $NOMEM $BASE
run v2_E2_P5_n1_s5  $H $V2 -DENV_E2 -DSTEPS=5 -DMAX_DETS=1 -DNO_P3 -DNO_P4 $NOMEM $BASE

# P1 e P2 (memória e overflow), ambiente irrestrito, 1 objeto, 2 frames
run v1_E1_P1P2_n1_s2 $H $V1 -DENV_E1 -DSTEPS=2 -DMAX_DETS=1 -DNO_P3 -DNO_P4 -DNO_P5 --overflow-check $BASE
run v2_E1_P1P2_n1_s2 $H $V2 -DENV_E1 -DSTEPS=2 -DMAX_DETS=1 -DNO_P3 -DNO_P4 -DNO_P5 --overflow-check $BASE

# P3 (contadores coerentes), ambiente realista, 2 objetos, 6 frames
run v1_E2_P3_n2_s6  $H $V1 -DENV_E2 -DSTEPS=6 -DMAX_DETS=2 -DNO_P4 -DNO_P5 $NOMEM $BASE
run v2_E2_P3_n2_s6  $H $V2 -DENV_E2 -DSTEPS=6 -DMAX_DETS=2 -DNO_P4 -DNO_P5 $NOMEM $BASE

wait
echo; echo "===== RESUMO ====="
for f in "$OUT"/*_n[12]_s[256].log; do
  printf "%-22s " "$(basename "$f" .log)"
  v=$(grep -oE "VERIFICATION (SUCCESSFUL|FAILED)|Timed out" "$f" | tail -1)
  p=$(grep -A1 "Violated property" "$f" | tail -1 | sed 's/^ *//')
  t=$(grep -oE "tempo_s=[0-9]+" "$f")
  echo "${v:-SEM VEREDICTO}  $t  $p"
done

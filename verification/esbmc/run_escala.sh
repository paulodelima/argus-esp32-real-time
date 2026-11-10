#!/usr/bin/env bash
# Escalabilidade: tempo de verificacao x tamanho da instancia x solucionador.
# Executar na raiz:  bash verification/esbmc/run_escala.sh
# Propriedade fixa (contadores, P3) no corrigido (v2), ambiente realista, y fixo,
# a mesma configuracao do teste de mutacao, para comparar com os 31 s ja medidos.
set -u
H=verification/esbmc/argus_harness.c
OUT=verification/logs/escala; mkdir -p "$OUT"
one() {
  local solver=$1 steps=$2 dets=$3
  local log="$OUT/${solver}_f${steps}_d${dets}.log" t0=$SECONDS v
  esbmc $H firmware_v2/argus_fsm.c -I"$PWD/firmware_v2" -DARGUS_V2 -DENV_E2 -DFIX_Y \
    -DNO_P4 -DNO_P5 -DSTEPS=$steps -DMAX_DETS=$dets \
    --no-bounds-check --no-pointer-check --no-align-check \
    --$solver --unwind 9 --timeout 5m > "$log" 2>&1
  if grep -q "VERIFICATION FAILED" "$log"; then v=FALHA
  elif grep -q "VERIFICATION SUCCESSFUL" "$log"; then v=PASSA
  else v=SEM_VEREDICTO; fi
  local vcc dp
  vcc=$(grep -o 'Generated [0-9]* VCC' "$log" | grep -o '[0-9]*' | tail -1)
  dp=$(grep -o 'Runtime decision procedure: [0-9.]*' "$log" | grep -o '[0-9.]*$' | tail -1)
  echo "$solver,$steps,$dets,$v,$((SECONDS - t0)),${vcc:-},${dp:-}"
}
export -f one; export H OUT
echo "solver,frames,objetos_por_frame,veredicto,tempo_total_s,vccs,tempo_solver_s" > "$OUT/escala.csv"
{
  for s in boolector bitwuzla z3; do
    for f in 2 3 4 5 6 7; do echo "$s $f 1"; done     # cresce o numero de frames
    for d in 2 3 4;       do echo "$s 4 $d"; done     # cresce o numero de objetos
  done
} | xargs -P 2 -n 3 bash -c 'one "$@"' _ | tee -a "$OUT/escala.csv"

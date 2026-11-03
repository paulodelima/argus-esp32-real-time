#!/usr/bin/env bash
# Teste de mutação (O4). Executar na raiz do repositório:
#   bash verification/esbmc/run_mutantes.sh
# Primeiro roda o firmware corrigido (referência, deve PASSAR) em cada configuração;
# depois cada mutante na configuração da propriedade que deveria pegá-lo.
# MORTO = ESBMC acusou violação. SOBREVIVEU = passou (a propriedade não discrimina o defeito).
set -u
H=verification/esbmc/argus_harness.c
OUT=verification/logs/mutantes; mkdir -p "$OUT"

cfg() {  # devolve as opções de cada configuração
  case "$1" in
    P3) echo "-DENV_E2 -DSTEPS=5 -DMAX_DETS=1 -DFIX_Y -DNO_P4 -DNO_P5 --no-bounds-check --no-pointer-check --no-align-check" ;;
    P4) echo "-DENV_E2 -DSTEPS=5 -DMAX_DETS=1 -DFIX_Y -DNO_P3 -DNO_P5 --no-bounds-check --no-pointer-check --no-align-check" ;;
    P5) echo "-DENV_E2 -DSTEPS=6 -DMAX_DETS=1 -DFIX_Y -DNO_P3 -DNO_P4 --no-bounds-check --no-pointer-check --no-align-check" ;;
    E1) echo "-DENV_E1 -DSTEPS=2 -DMAX_DETS=1 -DNO_P3 -DNO_P4 -DNO_P5 --overflow-check" ;;
  esac
}

run() {  # nome, pasta, propriedade
  local name=$1 dir=$2 prop=$3 log="$OUT/$1_$3.log" t0=$SECONDS
  esbmc $H "$dir/argus_fsm.c" -I"$PWD/$dir" -DARGUS_V2 $(cfg "$prop") \
    --boolector --unwind 9 --timeout 5m > "$log" 2>&1
  local v
  if grep -q "VERIFICATION FAILED" "$log"; then v="FALHA"
  elif grep -q "VERIFICATION SUCCESSFUL" "$log"; then v="PASSA"
  else v="SEM VEREDICTO"; fi
  local why; why=$(grep -A2 "Violated property" "$log" | tail -1 | sed 's/^ *//')
  printf "%-6s %-4s %-14s %5ss  %s\n" "$name" "$prop" "$v" "$((SECONDS - t0))" "$why" | tee -a "$OUT/RESUMO.txt"
}

: > "$OUT/RESUMO.txt"
echo "== Referência: firmware corrigido (todas devem PASSAR)" | tee -a "$OUT/RESUMO.txt"
for p in P3 P4 P5 E1; do run v2 firmware_v2 $p; done

echo "== Mutantes (MORTO se FALHA)" | tee -a "$OUT/RESUMO.txt"
while IFS='|' read -r m prop desc; do
  [ -z "$m" ] && continue
  echo "-- $m: $desc" | tee -a "$OUT/RESUMO.txt"
  run "$m" "verification/mutants/$m" "$prop"
done < verification/mutants/LISTA.txt

echo; echo "Resumo em $OUT/RESUMO.txt"

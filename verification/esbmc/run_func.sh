#!/usr/bin/env bash
# Propriedades funcionais P7 (classificacao + bip unico) e P8 (momento do bip).
# Executar na raiz:  bash verification/esbmc/run_func.sh
# Alvos: corrigido (v2), original (v1) e o mutante M5. PRES = frames com o objeto.
set -u
H=verification/esbmc/argus_func.c
OUT=verification/logs/func; mkdir -p "$OUT"
one() {
  IFS='|' read -r name dir pres <<< "$1"
  local log="$OUT/${name}_p${pres}.log" t0=$SECONDS v
  esbmc $H "$dir/argus_fsm.c" -I"$PWD/$dir" -DPRES=$pres \
    --boolector --unwind 10 --timeout 10m > "$log" 2>&1
  if grep -q "VERIFICATION FAILED" "$log"; then v=FALHA
  elif grep -q "VERIFICATION SUCCESSFUL" "$log"; then v=PASSA; else v=SEM_VEREDICTO; fi
  printf "%-4s objeto em %s frames  %-13s %4ss  %s\n" "$name" "$pres" "$v" "$((SECONDS - t0))" \
    "$(grep -A2 'Violated property' "$log" | tail -1 | sed 's/^ *//')"
}
export -f one; export H OUT
{ for p in 2 3 4; do echo "v2|firmware_v2|$p"; done
  for p in 2 3 4; do echo "M5|verification/mutants/M5|$p"; done
  for p in 2 3 4; do echo "v1|firmware|$p"; done; } \
| xargs -P 2 -I{} bash -c 'one "{}"' | tee "$OUT/RESUMO.txt"

#!/usr/bin/env bash
# Fila do fechamento (feedback de 24/11). Um processo por vez, maquina livre.
#   bash verification/run_fechamento.sh            # tudo (varias horas)
#   bash verification/run_fechamento.sh p6         # so P6 com bip nao bloqueante
#   bash verification/run_fechamento.sh qwen       # so as rodadas extras dos Qwen
#   bash verification/run_fechamento.sh slots      # so o passo com 3 e 8 slots
set -u
cd "$(dirname "$0")/.."
what=${1:-tudo}
if [ "$what" = tudo ] || [ "$what" = p6 ]; then
  echo "== P6 com bip nao bloqueante =="; bash verification/esbmc/run_p6_nb.sh
fi
if [ "$what" = tudo ] || [ "$what" = qwen ]; then
  echo "== Qwen: mais 2 rodadas de cada (total 3, igual ao Claude) =="
  for m in qwen2.5-coder:7b qwen2.5-coder:7b qwen2.5-coder:14b qwen2.5-coder:14b; do
    python3 verification/inv_llm/qwen_inv.py --model "$m" --rounds 8 --slots 2 | tail -15
  done
fi
if [ "$what" = tudo ] || [ "$what" = slots ]; then
  echo "== Passo indutivo com 3 e 8 slots =="; bash verification/esbmc/run_slots.sh
fi
echo "== fim: $(date) =="

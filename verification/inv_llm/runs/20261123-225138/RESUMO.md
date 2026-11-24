# Ciclo LLM + ESBMC (20261123-225138)

- modelo: qwen2.5-coder:14b; slots: 2; MAX_DETS: 1; filtro: 300 execucoes de 80 frames
- resultado: sem prova em 9 voltas (incluindo a volta 0, sem reforco)

| volta | origem | filtro | base | passo | violada | t_LLM (s) | t_passo (s) |
|---|---|---|---|---|---|---|---|
| 0 | base | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado |  | 2.6 |
| 1 | llm | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado | 184.2 | 11.0 |
| 2 | llm | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado | 143.2 | 3.9 |
| 3 | llm | OK | PASSA | FALHA | INV: invariante preservada | 71.5 | 4.4 |
| 4 | llm | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado | 35.6 | 2.8 |
| 5 | llm | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado | 50.2 | 5.3 |
| 6 | llm | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado | 38.8 | 7.0 |
| 7 | llm | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado | 67.7 | 3.5 |
| 8 | llm | OK | PASSA | FALHA | INV: invariante preservada | 58.9 | 3.0 |

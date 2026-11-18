# Ciclo LLM + ESBMC (20261118-084658)

- modelo: qwen2.5-coder:7b; slots: 2; MAX_DETS: 1; filtro: 300 execucoes de 80 frames
- resultado: sem prova em 7 voltas (incluindo a volta 0, sem reforco)

| volta | origem | filtro | base | passo | violada | t_LLM (s) | t_passo (s) |
|---|---|---|---|---|---|---|---|
| 0 | base | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado |  | 35.1 |
| 1 | llm | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado | 106.8 | 27.5 |
| 2 | llm | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado | 62.0 | 26.0 |
| 3 | llm | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado | 48.9 | 23.3 |
| 4 | llm | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado | 44.1 | 28.9 |
| 5 | llm | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado | 62.3 | 33.2 |
| 6 | llm | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado | 59.4 | 169.2 |

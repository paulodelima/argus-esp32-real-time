# Ciclo LLM + ESBMC (20261118-100114)

- modelo: qwen2.5-coder:14b; slots: 2; MAX_DETS: 1; filtro: 300 execucoes de 80 frames
- resultado: sem prova em 9 voltas (incluindo a volta 0, sem reforco)

| volta | origem | filtro | base | passo | violada | t_LLM (s) | t_passo (s) |
|---|---|---|---|---|---|---|---|
| 0 | base | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado |  | 2.5 |
| 1 | llm | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado | 49.3 | 5.9 |
| 2 | llm | OK | PASSA | FALHA | INV: invariante preservada | 28.5 | 4.2 |
| 3 | llm | OK | PASSA | FALHA | INV: invariante preservada | 53.2 | 2.3 |
| 4 | llm | REPETIDA |  |  |  | 71.8 |  |
| 5 | llm | REPETIDA |  |  |  | 67.5 |  |
| 6 | llm | REPETIDA |  |  |  | 72.7 |  |
| 7 | llm | REPETIDA |  |  |  | 70.7 |  |
| 8 | llm | REPETIDA |  |  |  | 68.2 |  |

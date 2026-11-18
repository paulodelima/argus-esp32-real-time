# Ciclo LLM + ESBMC (20261118-093203)

- modelo: qwen2.5-coder:7b; slots: 2; MAX_DETS: 1; filtro: 300 execucoes de 80 frames
- resultado: sem prova em 9 voltas (incluindo a volta 0, sem reforco)

| volta | origem | filtro | base | passo | violada | t_LLM (s) | t_passo (s) |
|---|---|---|---|---|---|---|---|
| 0 | base | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado |  | 35.0 |
| 1 | llm | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado | 83.1 | 4.5 |
| 2 | llm | REPETIDA |  |  |  | 40.1 |  |
| 3 | llm | REPETIDA |  |  |  | 53.9 |  |
| 4 | llm | REPETIDA |  |  |  | 38.2 |  |
| 5 | llm | REPETIDA |  |  |  | 51.2 |  |
| 6 | llm | REPETIDA |  |  |  | 31.2 |  |
| 7 | llm | REPETIDA |  |  |  | 36.2 |  |
| 8 | llm | REPETIDA |  |  |  | 40.1 |  |

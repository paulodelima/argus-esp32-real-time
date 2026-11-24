# Ciclo LLM + ESBMC (20261123-224611)

- modelo: qwen2.5-coder:7b; slots: 2; MAX_DETS: 1; filtro: 300 execucoes de 80 frames
- resultado: sem prova em 9 voltas (incluindo a volta 0, sem reforco)

| volta | origem | filtro | base | passo | violada | t_LLM (s) | t_passo (s) |
|---|---|---|---|---|---|---|---|
| 0 | base | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado |  | 2.6 |
| 1 | llm | REGRAS |  |  |  | 29.1 |  |
| 2 | llm | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado | 21.2 | 4.0 |
| 3 | llm | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado | 29.0 | 6.2 |
| 4 | llm | REPETIDA |  |  |  | 43.2 |  |
| 5 | llm | REPETIDA |  |  |  | 52.2 |  |
| 6 | llm | REPETIDA |  |  |  | 37.4 |  |
| 7 | llm | REPETIDA |  |  |  | 47.8 |  |
| 8 | llm | REPETIDA |  |  |  | 42.5 |  |

# Ciclo LLM + ESBMC (20261123-224158)

- modelo: claude-opus-5-5; slots: 2; MAX_DETS: 1; filtro: 300 execucoes de 80 frames
- resultado: PROVA (passo PASSA) em 5 voltas (incluindo a volta 0, sem reforco)

| volta | origem | filtro | base | passo | violada | t_LLM (s) | t_passo (s) |
|---|---|---|---|---|---|---|---|
| 0 | base | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado |  | 2.6 |
| 1 | llm | SEM_CODIGO |  |  |  | 42.1 |  |
| 2 | llm | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado | 31.0 | 5.4 |
| 3 | llm | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado | 20.5 | 2.2 |
| 4 | llm | OK | PASSA | PASSA |  | 28.8 | 15.2 |

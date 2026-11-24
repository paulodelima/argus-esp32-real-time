# Ciclo LLM + ESBMC (20261123-222645)

- modelo: claude-opus-5-5; slots: 2; MAX_DETS: 1; filtro: 300 execucoes de 80 frames
- resultado: PROVA (passo PASSA) em 5 voltas (incluindo a volta 0, sem reforco)

| volta | origem | filtro | base | passo | violada | t_LLM (s) | t_passo (s) |
|---|---|---|---|---|---|---|---|
| 0 | base | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado |  | 2.6 |
| 1 | llm | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado | 12.8 | 4.8 |
| 2 | llm | REGRAS |  |  |  | 27.2 |  |
| 3 | llm | REGRAS |  |  |  | 23.1 |  |
| 4 | llm | OK | PASSA | PASSA |  | 25.2 | 17.3 |

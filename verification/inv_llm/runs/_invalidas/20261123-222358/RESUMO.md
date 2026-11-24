# Ciclo LLM + ESBMC (20261123-222358)

- modelo: claude-opus-5-5; slots: 2; MAX_DETS: 1; filtro: 300 execucoes de 80 frames
- resultado: PROVA (passo PASSA) em 3 voltas (incluindo a volta 0, sem reforco)

| volta | origem | filtro | base | passo | violada | t_LLM (s) | t_passo (s) |
|---|---|---|---|---|---|---|---|
| 0 | base | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado |  | 2.5 |
| 1 | llm | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado | 13.5 | 4.8 |
| 2 | llm | OK | PASSA | PASSA |  | 19.2 | 17.0 |

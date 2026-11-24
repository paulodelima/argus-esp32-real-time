# Ciclo LLM + ESBMC (20261123-223026)

- modelo: claude-opus-5-5; slots: 2; MAX_DETS: 1; filtro: 300 execucoes de 80 frames
- resultado: PROVA (passo PASSA) em 8 voltas (incluindo a volta 0, sem reforco)

| volta | origem | filtro | base | passo | violada | t_LLM (s) | t_passo (s) |
|---|---|---|---|---|---|---|---|
| 0 | base | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado |  | 2.6 |
| 1 | llm | REGRAS |  |  |  | 28.1 |  |
| 2 | llm | REGRAS |  |  |  | 32.3 |  |
| 3 | llm | REGRAS |  |  |  | 30.0 |  |
| 4 | llm | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado | 12.8 | 4.8 |
| 5 | llm | REGRAS |  |  |  | 26.3 |  |
| 6 | llm | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado | 15.8 | 3.5 |
| 7 | llm | OK | PASSA | PASSA |  | 27.4 | 15.6 |

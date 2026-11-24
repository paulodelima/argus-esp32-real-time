# Ciclo LLM + ESBMC (20261123-224053)

- modelo: claude-opus-5-5; slots: 2; MAX_DETS: 1; filtro: 300 execucoes de 80 frames
- resultado: PROVA (passo PASSA) em 2 voltas (incluindo a volta 0, sem reforco)

| volta | origem | filtro | base | passo | violada | t_LLM (s) | t_passo (s) |
|---|---|---|---|---|---|---|---|
| 0 | base | OK | PASSA | FALHA | P4: todo evento e entregue, pendente ou contado como descartado |  | 2.7 |
| 1 | llm | OK | PASSA | PASSA |  | 31.1 | 23.3 |

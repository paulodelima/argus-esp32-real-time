# ARGUS v2 (candidata) — mudanças guiadas por verificação

Cada item responde a uma propriedade. Só vira "v2" de fato depois que o ESBMC
confirmar a violação na v1 e a ausência dela aqui.

| # | Propriedade | Defeito na v1 | Mudança na v2 |
|---|---|---|---|
| 1 | P1, P2 | `argus_update` não valida `dets`: NULL com `n_dets>0` desreferencia; coordenada irrestrita estoura `dets[d].x - last_x` (linha 97) | Contrato de entrada: NULL ou `n_dets<=0` vale como frame vazio; `n_dets` limitado a 64; detecções fora de `[0,w)×[0,h)` são ignoradas |
| 2 | P2 | `frame_w * 25` estoura com largura grande (linha 84) | `argus_init` limita `w` e `h` a `[2, 4096]` |
| 3 | P2 | `next_id++` estoura após 2^31 tracks (linha 139) | `next_id` reinicia em 1 ao atingir `0x3FFFFFFF` |
| 4 | P4 | `ev_push` descarta em silêncio com a fila cheia | `ev_push` retorna 0 e incrementa `ev_dropped` (novo campo). Com o host drenando a cada ciclo (E2), `ev_dropped` deve ser provado igual a 0 |
| 5 | P5 | Estado recalculado dos contadores; TRACKING vai direto a FEEDBACK (DECIDING pulado) | Transição explícita por estado atual (`switch`), fiel à Figura 3 para um objeto; para vários objetos, FEEDBACK→TRACKING é admitido e documentado |

Não muda: algoritmo de casamento, classificação, limites (8 tracks, 8 eventos,
3 frames perdidos, idade 60), API pública. Os 8 testes de `tests/test_fsm.c`
continuam passando (não checam estados).

# Invariantes propostas por LLM e checadas pelo ESBMC

Ciclo usado (guiado por contraexemplo):

1. **LLM propõe** uma invariante a partir do código (`argus_fsm.c/.h`) e, a partir da 2ª volta, do contraexemplo do ESBMC.
2. **Filtro barato:** execuções aleatórias com gcc (`-DCHECK_WITH_GCC -DFUZZ -DINV`). Se a invariante falha numa execução real, é rejeitada antes de ir ao ESBMC.
3. **Caso base** no ESBMC (`-DINV -DSTEPS=0`): o estado inicial satisfaz a invariante?
4. **Passo indutivo** no ESBMC (`-DINV -DINDUCT_STEP -DSTEPS=1`): de qualquer estado que a satisfaz, um frame a preserva e as propriedades valem?
5. FALHA no passo → o contraexemplo volta ao passo 1. PASSA nos dois → prova para qualquer número de frames.

- **LLM:** Claude (Anthropic), modelo `claude-opus-5-5`, na sessão de trabalho do projeto. As sugestões foram escritas pelo LLM; a decisão de aceitar cada uma coube ao ESBMC e ao filtro de execuções.
- **Verificador:** ESBMC 8.6.0 + Boolector 3.2.4, MacBook Air M5.

## Modelo de prompt (volta ≥ 2)

```
Contexto: firmware argus_fsm.c (anexo) e harness argus_harness.c (anexo).
A invariante atual é inv_ok() (anexo). O passo indutivo do ESBMC falhou.
Contraexemplo (estado de partida e propriedade violada): <trecho do log>
Tarefa: (1) diga se o estado de partida é alcançável a partir de argus_init;
(2) se não for, proponha o MENOR reforço de inv_ok que o exclua e que
continue verdadeiro em toda execução real; (3) justifique por que o reforço
se preserva em um frame.
```

## Voltas

| Volta | Invariante | Filtro gcc | Base | Passo (8 slots) | Contraexemplo / decisão |
|---|---|---|---|---|---|
| 0 | `inv_ok`: estado, faixas dos campos, contadores coerentes, fila vazia após o host | 0/1200 violações | PASSA | P4: **FALHA** (109 s) | Objeto novo recebe o id do que saiu (`next_id = 0x3FFFFFFF`) → harness não vê o encerramento. Estado inalcançável. |
| 1 | + ids ativos válidos, distintos e `≠ next_id` (`-DINV_IDS`, 1ª versão) | 0/600 | — | INV: **FALHA** (188 s) | Contador dá a volta com um id antigo (1) vivo → id = próximo id. Inalcançável: objeto vive ≤ 60 frames. |
| 2 | troca `≠ next_id` por **id recente**: 1 ≤ dist(next_id, id) ≤ idade × objetos por frame | 0/600 (+600 perto da volta) | PASSA | 8 slots: sem veredicto em 15 min · **2 slots: PASSA** (P3, P4, P5, INV) · **4 slots: PASSA** (P4, INV) | Aceita. |

## O que não foi automatizado

A chamada ao LLM foi feita na sessão de trabalho (copiar o contraexemplo e colar a resposta), não por script. O ciclo é o mesmo; automatizá-lo é trabalho futuro.

## Comparação controlada (23/11/2026)

Mesmo script (`qwen_inv.py`), mesmo prompt, 2 slots, até 8 voltas, MAX_DETS=1.
Claude via API (`--backend anthropic --model claude-opus-5-5`, sem temperatura: a API recusou o parâmetro).

Correções no script antes das rodadas válidas:
1. `inv_ok_text()` passou a retirar o bloco `#ifdef INV_IDS` do prompt (era a invariante já provada).
2. `rule_violations()` passou a ignorar comentários (texto como `ciclica (no anel)` virava "chamada de função").
Rodadas afetadas guardadas em `runs/_invalidas/`.

| Rodada | LLM | Resultado | Propostas até a prova |
|---|---|---|---|
| 20261123-224053 | Claude | PROVA | 1 |
| 20261123-224158 | Claude | PROVA | 4 (1 sem código) |
| 20261123-224441 | Claude | PROVA | 1 |
| 20261123-224611 | Qwen 7B | sem prova | volta 1 rejeitada (escreve em `c->ev_head`, usa `while`); 2 testadas; repetidas desde a volta 4 |
| 20261123-225138 | Qwen 14B | sem prova | 8 testadas; passo falhou em todas (P4 6x, INV 2x) |

Invariantes aceitas (nenhuma usa `last_y`, logo nenhuma depende de FIX_Y):
- 224053 e 224441: ids válidos, recentes (`dist <= age*MAX_DETS`, com volta do contador) e distintos.
- 224158: recência dos ids + `missed <= age-1` + coerência estado/n_tracks + no máximo MAX_DETS objetos com a mesma idade
  e com o mesmo `missed`. Depende de MAX_DETS, que já é hipótese do ambiente.

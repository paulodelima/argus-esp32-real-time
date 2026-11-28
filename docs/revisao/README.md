# Revisão sistemática de trabalhos relacionados — ARGUS (PGENE601)

Executada em 27/11/2026, seguindo a orientação do Prof. Lucas Cordeiro (palavras-chave, bases, refinamento, material reprodutível).
O prompt recebido tinha o contexto de outro artigo (LLM + RAG em atendimento); as perguntas abaixo foram adaptadas ao ARGUS.

## 1. Perguntas de pesquisa (QP)

| QP | Pergunta | Liga com |
|---|---|---|
| QP1 | Como BMC e k-indução são aplicados ao código C de firmware embarcado (não a modelos)? | P1–P5, harness, RQ1–RQ4 |
| QP2 | Como restrições de tempo/prazo são verificadas com verificadores de software (sem tempo)? | P6, bip, 800 ms |
| QP3 | Como LLMs propõem invariantes indutivas que um verificador decide, e onde falham? | RQ5, ciclo LLM–ESBMC, Claude × Qwen |
| QP4 | Como sistemas embarcados/CPS orientados a eventos (máquinas de estado) são verificados formalmente? | FSM IDLE→TRACKING→DECIDING→FEEDBACK |
| QP5 | Como provas para N componentes idênticos se reduzem a poucos (simetria, abstração por contadores, cutoff)? | 2 → 8 slots |

## 2. Protocolo

- **Período:** 2010–2026 (clássicos anteriores entram só por *snowballing* para trás).
- **Inclusão:** verificação formal (BMC, k-indução, model checking, invariantes) de software/firmware ou CPS; LLM combinado com verificador; artigo revisado por pares, ou arXiv de grupo identificável quando muito recente (2025–2026).
- **Exclusão:** sem verificação formal; só hardware/RTL; ruído de outras áreas (física, química); Zenodo/preprint sem autoria ou revisão; teses quando existe o artigo correspondente; duplicatas (fica a versão de periódico/conferência).
- **Triagem:** título → resumo → leitura da parte relevante. Triagem por título feita nos resultados ordenados por citações (todos em S1 e S2; 60 primeiros em S3 e S4; 40 primeiros em S5b).
- **Snowballing:** para trás a partir de Gadelha 2017, Alhawi 2021 e Pirzada 2024 (já citados); para frente a partir do survey do ESBMC (Dantas et al., 2026).

## 3. Strings e contagens

Base usada aqui: **OpenAlex** (`title_and_abstract.search`, booleano, a partir de 2010-01-01), que indexa IEEE, ACM, Springer e arXiv.
IEEE Xplore, ACM DL, SpringerLink e Scopus **não são acessíveis por API desta sessão**: as mesmas strings estão prontas para rodar no Portal de Periódicos CAPES; as colunas ficam em branco até lá (não invento números).

| ID | String (título + resumo) | OpenAlex | IEEE Xplore | ACM DL | Springer | Scopus |
|---|---|---|---|---|---|---|
| S1 | `("bounded model checking" OR "k-induction") AND (firmware OR "embedded software" OR microcontroller OR "embedded C")` | 46 | | | | |
| S2 | `("bounded model checking" OR "model checking") AND ("timing constraint" OR "timing constraints" OR deadline OR "execution time") AND (embedded OR firmware)` | 54 | | | | |
| S3 | `("large language model" OR "large language models" OR LLM OR LLMs) AND ("loop invariant" OR "loop invariants" OR "inductive invariant" OR "inductive invariants" OR "invariant generation")` | 72 | | | | |
| S4 | `("model checking" OR "formal verification") AND ("finite state machine" OR "event-driven") AND ("cyber-physical" OR embedded OR firmware)` | 73 | | | | |
| S5 | `("parameterized verification" OR "parameterized model checking" OR "symmetry reduction" OR "counter abstraction") AND (software OR "C program" OR "C programs" OR embedded OR protocol)` | 241 | | | | |
| S5b | refinada: `(... OR cutoff) AND ("model checking" OR "software verification")` | 192 | | | | |

**Refinamento:** S5 trazia ruído de física ("symmetry reduction" em equações diferenciais); exigir `"model checking"` resolveu (S5b). Uma primeira versão das strings no campo `search` geral (sem o filtro de título/resumo) dava 734 a 274 813 resultados e foi descartada.

Para Scopus, prefixe com `TITLE-ABS-KEY( ... ) AND PUBYEAR > 2009`; no IEEE Xplore use *Command Search* com `("All Metadata": ...)`.

## 4. Resultado da triagem

| String | Triados por título | Incluídos (novos) | Já no artigo |
|---|---|---|---|
| S1 | 46 | Rocha 2015; Schrammel 2015; Beyer 2026 | Cordeiro 2010, 2012; Alhawi 2021 |
| S2 | 54 | — (quase tudo é UPPAAL sobre modelos de autômatos, não código C) | Barreto 2011 (via *snowballing*: sem resumo no índice) |
| S3 | 60 | Kamath 2023; Janßen 2024; Pirzada 2026; Su 2026 (CIll) | Wu 2024; Pirzada 2024 (via *snowballing*) |
| S4 | 60 | Mousavi 2023 | — |
| S5b + *snowballing* | 40 | Emerson & Sistla 1996; Emerson & Kahlon 2000 | — |

Achado de S2: a verificação de prazo em código C com BMC é rara; quase todo trabalho de tempo usa autômatos temporizados (UPPAAL) sobre um modelo. Isso reforça a lacuna do ARGUS.

## 5. Tabela dos trabalhos incluídos (metadados conferidos no DOI)

| Trabalho | QP | Técnica / ferramenta | Objeto | Relação com o ARGUS |
|---|---|---|---|---|
| Rocha, Ismail, Cordeiro, Barreto. SBESC 2015, pp. 90–95. doi:10.1109/SBESC.2015.24 | QP1 | k-indução + invariantes poliédricas (ESBMC) | C embarcado com laços | Mesma família; aqui a invariante veio de um LLM e a indução está no harness |
| Schrammel et al. LNCS 9128, 2015, pp. 62–77. doi:10.1007/978-3-319-19458-5_5 | QP1 | BMC incremental | Código automotivo industrial | BMC em produção; o ARGUS mostra o limite em objetos por quadro |
| Beyer et al. LNCS, 2026, pp. 42–64. doi:10.1007/978-3-032-22749-2_3 | QP1 | Vários verificadores de software | Firmware Intel TDX | Estudo de caso de firmware real, como o ARGUS, em escala maior |
| Kamath et al. arXiv:2311.07948, 2023 | QP3 | LLM propõe, verificador checa | Laços de *benchmarks* | Mesmo ciclo propor–checar; aqui o estado é de um firmware |
| Janßen, Richter, Wehrheim. LNCS, 2024, pp. 266–279. doi:10.1007/978-3-031-57259-3_13 | QP3 | ChatGPT gera invariantes para verificadores | Laços de C | Avalia LLM como fonte de invariantes, sem decidir |
| Pirzada, Parsert, Wang, Korovin, Cordeiro. arXiv:2606.16886, 2026 | QP3 | Modelos abertos locais + ESBMC (VerIbmc) | Laços de *benchmarks* | Usa modelos locais com o ESBMC; o ARGUS mostra o Qwen local falhando num estado de firmware |
| Su et al. (CIll). arXiv:2602.23389, 2026 | QP3 | LLM guiado por contraexemplo de indução (CTI) | Model checking de projetos | Mesma ideia do ciclo do ARGUS: o contraexemplo do passo volta ao LLM |
| Mousavi, Ebnenasir, Mahmoudzadeh. ACM TCPS 7(4), 2023. doi:10.1145/3605948 | QP4 | Abstração em máquina de estados + Promela/SPIN | Escalonador do Contiki (IoT orientado a eventos) | Extrai um modelo; o ARGUS verifica o C diretamente |
| Emerson, Sistla. FMSD 9(1–2), 1996, pp. 105–131. doi:10.1007/BF00625970 | QP5 | Redução por simetria | Processos idênticos | Base do argumento 2 → 8 slots |
| Emerson, Kahlon. 2000, LNCS, pp. 236–254. doi:10.1007/10721959_19 | QP5 | *Cutoff* ("muitos para poucos") | Sistemas parametrizados | Justifica verificar poucos slots e generalizar |
| Dantas, Cordeiro, Junior. arXiv:2605.26169, 2026 | hub | Survey do ESBMC | — | Fonte de *snowballing*; não precisa entrar no artigo |

Observação: os dois arXiv de 2026 (Pirzada; Su) foram lidos só no resumo.

## 6. Lacuna (parágrafo para o artigo)

Os trabalhos de LLM com verificador tratam laços isolados de *benchmarks* (Kamath 2023; Janßen 2024; Wu 2024; Pirzada 2024, 2026), e os de firmware real usam invariantes escritas à mão ou inferidas por domínio abstrato (Rocha 2015; Beyer 2026). A verificação de prazo em código C embarcado com BMC aparece pouco: o tempo costuma ser verificado em autômatos temporizados sobre um modelo, e não no código. Não encontramos trabalho que combine, no mesmo firmware, BMC do código C, propriedade de prazo com custos medidos em hardware e uma invariante de estado proposta por LLM e decidida pelo verificador. Essa combinação é a contribuição do ARGUS.

## 7. Reprodução

```bash
python3 docs/revisao/buscar_openalex.py   # regrava openalex_S1-S5.json com as contagens do dia
```
Os resultados mudam com o tempo; as contagens acima são de 27/11/2026.

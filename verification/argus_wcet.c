/* =====================================================================
 * argus_wcet.c  --  Verificacao temporal do ARGUS no ESBMC
 *
 * Objetivo: certificar formalmente, sob as estimativas de WCET MEDIDAS
 * em hardware (ESP32-CAM, esp_timer), o maior N tal que
 * Ctotal(N) <= Dergo, e portanto derivar Nmax de modo certificavel.
 *
 * COMO RODAR:
 *   esbmc argus_wcet.c --unwind 16 --no-unwinding-assertions
 *
 * ESBMC procura um N (nao-deterministico, 0..ARGUS_MAX_TRACKS) que VIOLE
 * a asserao Ctotal(N) <= Dergo. Se NENHUM N viola, o sistema cumpre o
 * deadline para todo N ate o limite fisico -> Nmax = ARGUS_MAX_TRACKS.
 * Se algum N viola, o menor que viola eh (Nmax + 1).
 *
 * --------------------------------------------------------------------
 * MODELO DE CUSTO (revisado apos medicao em hardware):
 *
 *   Ctotal(N) = C_detect + C_update(N) + C_feedback
 *
 * Nota sobre a revisao: a equacao (1) original da proposta era
 * Ctotal(N) = Cdetect + N*Ctrack + Cclassify + Cfeedback, assumindo que
 * o custo de processar N objetos cresce linearmente (N*Ctrack). A medicao
 * em hardware mostrou que isso NAO se sustenta: o tracking + classificacao
 * acontecem dentro de UMA funcao (argus_update), cuja carga depende de N
 * mas NAO eh uma soma de N tarefas independentes. Isso eh coerente com o
 * argumento da proposta de que Liu-Layland nao se aplica (nao ha conjunto
 * de tarefas periodicas independentes; ha uma funcao unica parametrizada
 * por N). Portanto medimos C_update(N) DIRETAMENTE por N, em vez de
 * assumir a forma N*Ctrack.
 *
 * SOBRE AS ANOTACOES //@ DE [Barreto, Cordeiro, Fischer 2011]:
 * A semantica (acumular WCET medido e assertar o deadline) eh a abaixo.
 * A forma anotada //@ DEFINE-TIMER / RESET-TIMER / WCET-FUNCTION /
 * ASSERT-TIMER vai no relatorio final; aqui esta a forma C-pura que o
 * ESBMC verifica diretamente.
 * ===================================================================== */

/* nondet_uint(): o ESBMC fornece a implementacao (valor nao-deterministico).
 * Apenas declaramos aqui. */
unsigned int nondet_uint(void);

#define ARGUS_MAX_TRACKS 8u

/* --------- estimativas de WCET MEDIDAS em hardware (microssegundos) ---
 * Fonte: ESP32-CAM (AI-Thinker, OV5660), FRAMESIZE_QQVGA 160x120 grayscale,
 * deteccao por frame-diff + malha 8x8, esp_timer_get_time().
 * Medicao por frame; maximos observados em cenario de movimento sustentado.
 *
 * C_DETECT  : varredura de pixels p/ deteccao de movimento (dominante).
 * C_FEEDBACK: custo de CPU p/ despachar o evento (poll). O tempo fisico do
 *             buzzer NAO entra aqui: eh latencia de atuador, modelada a
 *             parte (os delays de UI do beep() ficam fora da medicao).
 * C_UPDATE[N]: WCET de argus_update() para N tracks ativos (matching +
 *             tracking + classificacao + criacao de tracks). */
#define C_DETECT     3528u   /* medido (dominante) */
#define C_CLASSIFY      6u   /* medido (classificacao de massa/acao) */
#define C_FEEDBACK      5u   /* medido (so despacho do evento) */
#define D_ERGO     800000u   /* deadline ergonomico = 800 ms (Nielsen) */

/* tabela de WCET medido por N; defensiva contra N > MAX.
 * Maximos observados: [0]=18 [1]=6 [2]=8 [3]=9 [4]=11 [5]=14 [6]=12 [7]=14 [8]=19 */
static unsigned int c_update(unsigned int N) {
    if (N == 0u) return 18u;
    if (N == 1u) return 6u;
    if (N == 2u) return 8u;
    if (N == 3u) return 9u;
    if (N == 4u) return 11u;
    if (N == 5u) return 14u;
    if (N == 6u) return 12u;
    if (N == 7u) return 14u;
    return 19u;               /* N >= 8 */
}

/* Ctotal(N) = C_detect + C_update(N) + C_feedback */
static unsigned long ctotal(unsigned int N) {
    /* //@ DEFINE-TIMER t
     * //@ RESET-TIMER t
     * //@ WCET-FUNCTION detectMovimento  C_DETECT
     * //@ WCET-FUNCTION atualizaTracks    C_UPDATE(N)
     * //@ WCET-FUNCTION feedback          C_FEEDBACK
     * forma em C puro equivalente: */
    unsigned long t = 0;
    t += C_DETECT;
    t += C_CLASSIFY;
    t += c_update(N);
    t += C_FEEDBACK;
    return t;
}

int main(void) {
    /* N nao-deterministico, limitado pelo limite fisico de tracks. */
    unsigned int N = nondet_uint();
    __ESBMC_assume(N <= ARGUS_MAX_TRACKS);

    unsigned long total = ctotal(N);

    /* //@ ASSERT-TIMER t <= D_ERGO */
    __ESBMC_assert(total <= D_ERGO, "deadline ergonomico violado");

    return 0;
}

/* ---------------------------------------------------------------------
 * RESULTADO ESPERADO E INTERPRETACAO
 *
 * Pior caso medido: Ctotal(N) = C_DETECT + max(C_update) + C_FEEDBACK
 *                             = 3041 + 18 + 3
 *                             = 3062 us  ~=  3,06 ms
 *
 * Contra D_ergo = 800000 us (800 ms): uso de ~0,38% do orcamento.
 *
 * ESBMC NAO encontra nenhum N em [0, 8] que viole a asserao.
 * Logo Nmax = ARGUS_MAX_TRACKS = 8 (o limite FISICO de tracks), e nao um
 * limite TEMPORAL: o tempo nunca chega perto de estourar.
 *
 * Conclusao de pesquisa: para deteccao frame-diff + tracking geometrico
 * no ESP32, a restricao temporal de 800 ms eh folgada por ~2 ordens de
 * grandeza. O fator limitante eh memoria/arquitetura (8 tracks), nao
 * tempo. A folga eh justamente a margem que acomodaria um detector mais
 * robusto (oclusao, perspectiva +-15 graus) ou inferencia neural
 * embarcada (linha futura) — onde a analise de WCET volta a ser critica.
 *
 * RESSALVAS (a levantar na defesa, antes que a banca o faca):
 *  1. C_detect eh POR FRAME; o ciclo ergonomico de 800 ms eh PONTA-A-PONTA
 *     (multiplos frames, do inicio do movimento ate o objeto sair). Um
 *     modelo ponta-a-ponta somaria ~N_frames * C_detect — ainda assim, a
 *     centenas de frames de folga.
 *  2. C_update plano reflete o detector ATUAL (frame-diff simples). Um
 *     detector mais pesado muda C_detect, nao a estrutura do argumento.
 *  3. Medicao empirica garante apenas os cenarios medidos; a certificacao
 *     ESBMC garante a relacao logica Ctotal(N) <= Dergo SOB esses numeros.
 * ------------------------------------------------------------------- */
/* =====================================================================
 * argus_wcet.c  --  Verificacao temporal do ARGUS no ESBMC
 *
 * Objetivo: certificar formalmente, sob as estimativas de WCET medidas
 * em hardware, o maior N tal que  Ctotal(N) <= Dergo  (equacao 2 da
 * proposta), e portanto derivar Nmax de modo certificavel.
 *
 * COMO RODAR:
 *   esbmc argus_wcet.c --unwind 32 --no-unwinding-assertions
 *
 * ESBMC vai procurar um N que VIOLE a asserao. O menor N que viola eh
 * (Nmax + 1); logo Nmax = (esse N) - 1. Rode tambem a versao "prova"
 * abaixo para confirmar que ate Nmax a restricao SEMPRE vale.
 *
 * --------------------------------------------------------------------
 * SOBRE AS ANOTACOES //@ DE [Barreto, Cordeiro, Fischer 2011]:
 * A proposta exige anotar com //@ DEFINE-TIMER, //@ RESET-TIMER,
 * //@ WCET-FUNCTION e //@ ASSERT-TIMER. Esses comentarios sao expandidos
 * pelo pre-processador do metodo em manipulacoes de variaveis-relogio +
 * assert(). Abaixo eu mostro AMBAS as formas:
 *   (1) a forma anotada //@  (o que vai no relatorio, fiel ao metodo)
 *   (2) a forma em C puro equivalente, que o ESBMC verifica HOJE com
 *       __ESBMC_assert -- util para validar antes de aplicar as macros.
 * Confira a sintaxe exata das macros no artigo [1] antes de fechar o
 * relatorio; a semantica (acumular WCET e assertar o deadline) eh esta.
 * ===================================================================== */

/* Permite compilar com gcc tambem (sanity check), nao so com esbmc. */
#ifndef __ESBMC__
  #include <assert.h>
  #define __ESBMC_assert(c,m) assert(c)
  #define __ESBMC_assume(c)   ((void)0)
  static unsigned nondet_uint(void){ return 3u; } /* valor qualquer p/ gcc */
#endif

/* --------- estimativas de WCET (em microssegundos) ---------------
 * PREENCHA com os valores MAXIMOS medidos no Dia 4 (esp_timer).
 * Os numeros abaixo sao PLACEHOLDERS ilustrativos -- troque pelos seus. */
#define C_DETECT    120000u   /* Cdetect:   deteccao de movimento / frame */
#define C_TRACK      90000u   /* Ctrack:    atualizar 1 trajetoria        */
#define C_CLASSIFY   40000u   /* Cclassify: decidir IN/OUT                */
#define C_FEEDBACK   15000u   /* Cfeedback: emitir bip                    */
#define D_ERGO      800000u   /* Dergo:     deadline ergonomico = 800 ms  */

/* Ctotal(N) = Cdetect + N*Ctrack + Cclassify + Cfeedback   (equacao 1) */
static unsigned long ctotal(unsigned int N) {
    /* //@ DEFINE-TIMER t
     * //@ RESET-TIMER t
     * //@ WCET-FUNCTION detectMovimento  C_DETECT
     * //@ WCET-FUNCTION trackObjeto       C_TRACK     (chamada N vezes)
     * //@ WCET-FUNCTION classifica        C_CLASSIFY
     * //@ WCET-FUNCTION feedback          C_FEEDBACK
     * forma em C puro equivalente: */
    unsigned long t = 0;
    t += C_DETECT;
    for (unsigned int i = 0; i < N; i++) t += C_TRACK;
    t += C_CLASSIFY;
    t += C_FEEDBACK;
    return t;
}

int main(void) {
    /* N nao-deterministico, limitado pelo limite fisico de tracks. */
    unsigned int N = nondet_uint();
    __ESBMC_assume(N <= 8u);              /* ARGUS_MAX_TRACKS */

    unsigned long total = ctotal(N);

    /* //@ ASSERT-TIMER t <= D_ERGO
     * forma em C puro equivalente: */
    __ESBMC_assert(total <= D_ERGO, "deadline ergonomico violado");

    return 0;
}

/* ---------------------------------------------------------------------
 * Calculo analitico de conferencia (equacao 2), util no relatorio:
 *
 *   Ctotal(N) <= Dergo
 *   Cdetect + N*Ctrack + Cclassify + Cfeedback <= Dergo
 *   N <= (Dergo - Cdetect - Cclassify - Cfeedback) / Ctrack
 *
 * Com os placeholders acima:
 *   N <= (800000 - 120000 - 40000 - 15000) / 90000
 *   N <= 625000 / 90000  =  6.94  ->  Nmax = 6
 *
 * Ou seja: com esses numeros ilustrativos o ESBMC deveria provar a
 * asserao para N<=6 e exibir contraexemplo a partir de N=7. Quando
 * voce trocar pelos WCET REAIS medidos no ESP32, o Nmax muda -- e esse
 * eh exatamente o resultado da sua pesquisa.
 * ------------------------------------------------------------------- */

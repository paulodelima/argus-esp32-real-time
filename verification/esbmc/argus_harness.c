/* =====================================================================
 * argus_harness.c -- harness de ambiente para verificar argus_fsm.c
 *
 * Verifica O CODIGO do nucleo (nao o modelo de custo). O firmware e
 * chamado em laco com entradas nao deterministicas; o ESBMC explora
 * todas as entradas ate o limite de desenrolamento.
 *
 * Ambientes (escolher UM com -D):
 *   -DENV_E1  irrestrito: coordenadas sao int quaisquer, o ponteiro
 *             pode ser NULL, o host pode nao drenar a fila. n_dets em
 *             [0,8] (o buffer da camada de visao tem 8 centroides; o
 *             codigo aceita ate 64, mas isso so alonga o laco).
 *   -DENV_E2  realista: 0<=x<160, 0<=y<120, 0<=n_dets<=8, ponteiro
 *             valido, host drena a fila a cada ciclo (como o .ino).
 *
 * Propriedades (todas ativas por padrao; -DNO_P3 etc. desliga):
 *   P1 memoria   : checagens embutidas do ESBMC (bounds, pointer)
 *   P2 overflow  : --overflow-check
 *   P3 invariantes: 0<=n_tracks<=8 e == slots ativos;
 *                  0<=ev_count<=8; ev_tail == (ev_head+ev_count) % 8
 *   P4 sem perda : finalizados == entregues ao host + pendentes na fila
 *   P5 transicoes: monitor da maquina IDLE->TRACKING->DECIDING->
 *                  FEEDBACK->IDLE (Figura 3; laços proprios permitidos)
 *
 * Laco principal: -DSTEPS=n frames (BMC) ou -DUNBOUNDED (k-inducao).
 * -DFIX_Y: y fixo em 60 (so x simbolico). -DCONCRETE_TRACE: traco concreto
 *           de reserva (sem entrada simbolica; nao e prova).
 * -DARGUS_V2: usa o contrato da v2 (ev_dropped contado em P4 e provado 0
 *             em E2; FEEDBACK->TRACKING admitido em P5).
 * -DLOOP_INV (com -DUNBOUNDED -DENV_E2): anota inv_ok (+ contadores de P4)
 *       como invariante do laco para --loop-invariant-check / --loop-invariant
 *       (ESBMC: base, havoc + assume, passo indutivo de uma volta).
 * -DP6 (so com ENV_E2): prazo ponta a ponta de 800 ms, do ultimo frame em
 *       que o objeto foi visto ate o inicio do seu bip. Modelo de tempo com
 *       variavel auxiliar t (us), como em Barreto, Cordeiro e Fischer:
 *       cada volta do loop() = espera do quadro (T_FRAME_US, HIPOTESE) +
 *       C_detect + C_classify + C_update(N) (WCET medidos no trabalho
 *       anterior) + por evento: C_feedback + bip bloqueante (beep(): 180 ms
 *       por pulso; AMBIGUOUS = 2 pulsos, IN/OUT = 1). -DT_FRAME_US=n.
 *       Hipotese: STEPS < ARGUS_MAX_AGE (o encerramento por idade nao ocorre).
 * ===================================================================== */
#include "argus_fsm.h"

#ifdef CHECK_WITH_GCC   /* so para conferir sintaxe com gcc */
#include <stdlib.h>
#include <assert.h>
#ifdef FUZZ
#include <unistd.h>
#include <time.h>
static int fuzz_seeded = 0;
static int fuzz_rand(int m) {
    if (!fuzz_seeded) { srand((unsigned)(getpid() ^ time(0))); fuzz_seeded = 1; }
    return rand() % m;
}
static int __VERIFIER_nondet_int(void) { return fuzz_rand(160); }
#else
static int __VERIFIER_nondet_int(void) { return rand(); }
#endif
#define __ESBMC_assume(c) do { if (!(c)) exit(0); } while (0)
#define __ESBMC_loop_invariant(c) ((void)0)
#define __ESBMC_assert(c, m) assert((c) && (m))
#else
int __VERIFIER_nondet_int(void);
void __ESBMC_assume(_Bool);
void __ESBMC_assert(_Bool, const char *);
#endif

#ifndef STEPS
#define STEPS 4
#endif

#if !defined(ENV_E1) && !defined(ENV_E2)
#error "defina -DENV_E1 ou -DENV_E2"
#endif

/* Detecções por frame. Padrão 8 (limite da câmera). Para instâncias
 * menores, mais rápidas: -DMAX_DETS=1 ou 2 (hipótese declarada no resultado). */
#ifndef MAX_DETS
#define MAX_DETS 8
#endif

static const char *S_NAME[4] = {"IDLE", "TRACKING", "DECIDING", "FEEDBACK"};

/* P5: transicoes permitidas pela Figura 3 (linear, com laco proprio). */
#if defined(INV) || defined(LOOP_INV)
/* Invariante indutiva para a k-inducao (-DINV, so faz sentido com ENV_E2).
 * Vale no estado inicial e e preservada por um frame; o harness a ASSUME no
 * inicio de cada volta (restringe o estado arbitrario do passo indutivo) e a
 * AFIRMA no fim (assim a prova continua valida). */
#ifdef INV_LLM
/* -DINV_LLM: reforco proposto pelo LLM, em inv_llm_current.h, com a forma
 *   static int inv_extra(const argus_ctx_t *c) { ... return 1; }
 * (gerado por verification/inv_llm/qwen_inv.py). */
#include "inv_llm_current.h"
#endif
static int inv_ok(const argus_ctx_t *c, argus_state_t prev) {
    int i, ativos = 0;
    if (prev != c->state) return 0;
    if (c->state < ARGUS_IDLE || c->state > ARGUS_FEEDBACK) return 0;
    if (c->frame_w != 160 || c->frame_h != 120 || c->center_x != 80) return 0;
    if (c->n_tracks < 0 || c->n_tracks > ARGUS_MAX_TRACKS) return 0;
    if (c->ev_count != 0) return 0;                       /* host drenou */
    if (c->ev_head < 0 || c->ev_head >= ARGUS_MAX_EVENTS) return 0;
    if (c->ev_tail != c->ev_head) return 0;
    if (c->next_id < 1 || c->next_id > 0x3FFFFFFF) return 0;
#ifdef ARGUS_V2
    if (c->ev_dropped != 0) return 0;
#endif
    for (i = 0; i < ARGUS_MAX_TRACKS; i++) {
        if (!c->tracks[i].active) continue;
        ativos++;
        if (c->tracks[i].start_x < 0 || c->tracks[i].start_x >= 160) return 0;
        if (c->tracks[i].last_x  < 0 || c->tracks[i].last_x  >= 160) return 0;
        if (c->tracks[i].last_y  < 0 || c->tracks[i].last_y  >= 120) return 0;
        if (c->tracks[i].age < 1 || c->tracks[i].age > ARGUS_MAX_AGE) return 0;
        if (c->tracks[i].missed < 0 || c->tracks[i].missed > ARGUS_MAX_MISSED) return 0;
#ifdef INV_IDS
        /* ids dos objetos ativos: validos, distintos entre si e diferentes
         * do proximo id (um objeto vive <= 60 frames; o contador so daria a
         * volta depois de ~1e9 objetos). */
        if (c->tracks[i].id < 1 || c->tracks[i].id > 0x3FFFFFFF) return 0;
        {
            /* "idade" do id: quantos ids foram dados depois dele, contando a
             * volta do contador (1..0x3FFFFFFF). No maximo MAX_DETS por frame
             * de vida do objeto, entao 1 <= dist <= age * MAX_DETS. */
            int dist = c->next_id - c->tracks[i].id;   /* cabe em int: |dif| < 2^30 */
            if (dist <= 0) dist += 0x3FFFFFFF;
            if (dist < 1 || dist > c->tracks[i].age * MAX_DETS) return 0;
        }
        {
            int j;
            for (j = i + 1; j < ARGUS_MAX_TRACKS; j++)
                if (c->tracks[j].active && c->tracks[j].id == c->tracks[i].id) return 0;
        }
#endif
    }
    if (ativos != c->n_tracks) return 0;
#ifdef INV_LLM
    if (!inv_extra(c)) return 0;
#endif
    return 1;
}
#endif

#ifdef P6
#ifndef ENV_E2
#error "P6 exige -DENV_E2 (host drena a fila a cada volta, como o .ino)"
#endif
#ifndef T_FRAME_US
#define T_FRAME_US 50000UL      /* HIPOTESE: periodo do quadro (nao medido) */
#endif
#define C_DETECT_US    3709UL   /* medidos no trabalho anterior (Tabela 2) */
#define C_CLASSIFY_US     6UL
#define C_FEEDBACK_US     5UL
#define BEEP_PULSE_US 180000UL  /* beep(): tone 80 ms; delay(120) + delay(60) */
#define D_ERGO_US     800000UL
static unsigned long c_update_us(int N) {
    static const unsigned long tab[9] = {18, 6, 8, 9, 11, 14, 12, 14, 19};
    return (N >= 0 && N <= 8) ? tab[N] : 19UL;
}
#endif

static int p5_allowed(argus_state_t a, argus_state_t b) {
    if (a == b) return 1;
    if (a == ARGUS_IDLE     && b == ARGUS_TRACKING) return 1;
    if (a == ARGUS_TRACKING && b == ARGUS_DECIDING) return 1;
    if (a == ARGUS_DECIDING && b == ARGUS_FEEDBACK) return 1;
    if (a == ARGUS_FEEDBACK && b == ARGUS_IDLE)     return 1;
#ifdef ARGUS_V2
    if (a == ARGUS_FEEDBACK && b == ARGUS_TRACKING) return 1; /* varios objetos */
#endif
    return 0;
}

#ifdef GLOBAL_STATE
/* -DGLOBAL_STATE: o estado que o laco altera fica em variaveis globais,
 * em vez de locais de main. O firmware continua recebendo &c, mas agora
 * &c e um endereco fixo e conhecido (para --loop-invariant-check). */
argus_ctx_t c;
argus_point_t dets[MAX_DETS];
const argus_point_t *p;
argus_state_t prev;
unsigned long finalized, polled;
int was_active[ARGUS_MAX_TRACKS], was_id[ARGUS_MAX_TRACKS];
#endif

int main(void) {
    int n, d, i;
#ifndef UNBOUNDED
    int step;
#endif
#ifndef GLOBAL_STATE
    argus_ctx_t c;
    argus_point_t dets[MAX_DETS];
    const argus_point_t *p;
    argus_state_t prev;
    unsigned long finalized, polled;    /* contadores de P4 */
    int was_active[ARGUS_MAX_TRACKS], was_id[ARGUS_MAX_TRACKS];
#endif
    finalized = 0; polled = 0;
#ifdef P6
    /* t: relogio auxiliar (us). seen[i]: instante de captura do ultimo
     * quadro em que o objeto do slot i foi visto. q_seen: fila-sombra,
     * na mesma ordem da fila de eventos do firmware. */
    unsigned long t = 0, t_cap, lat;
    unsigned long seen[ARGUS_MAX_TRACKS], seen_prev[ARGUS_MAX_TRACKS];
    unsigned long q_seen[ARGUS_MAX_EVENTS];
    int q_h = 0, q_n = 0, n_before, e;
    for (i = 0; i < ARGUS_MAX_TRACKS; i++) seen[i] = 0;
#endif

#ifdef INDUCT_STEP
    /* Passo indutivo escrito a mao (sem laco): c e prev ficam SEM valor
     * inicial = valores quaisquer para o ESBMC. Com -DINV -DSTEPS=1, o
     * harness assume inv_ok no inicio da volta, executa UM frame e afirma
     * inv_ok e as propriedades no fim. Junto com o caso base, prova por
     * inducao que valem para qualquer numero de frames. */
    prev = (argus_state_t)__VERIFIER_nondet_int();
#else
    argus_init(&c, 160, 120);
    prev = c.state;
#ifdef INV
    __ESBMC_assert(inv_ok(&c, prev), "INV: vale no estado inicial (caso base)");
#endif
#endif

#ifdef UNBOUNDED
#ifdef LOOP_INV
#if defined(ARGUS_V2) && !defined(NO_P4)
    __ESBMC_loop_invariant(inv_ok(&c, prev) && finalized == polled);
#else
    __ESBMC_loop_invariant(inv_ok(&c, prev));
#endif
#endif
    while (1) {
#else
    for (step = 0; step < STEPS; step++) {
#endif
#ifdef INV
        __ESBMC_assume(inv_ok(&c, prev));    /* hipotese indutiva */
#endif
#ifdef P6
        t += T_FRAME_US;                 /* espera/captura do quadro */
        t_cap = t;
        for (i = 0; i < ARGUS_MAX_TRACKS; i++) seen_prev[i] = seen[i];
        n_before = c.n_tracks;
#endif
        /* ---------- ambiente: um frame de deteccoes ---------- */
#if defined(P6_SYM) && !defined(UNBOUNDED)
        /* P6, busca simbolica: nos P6_SYM primeiros frames entram de 0 a
         * MAX_DETS objetos em posicoes x QUAISQUER (y fixo); depois nenhum.
         * O ESBMC prova (ou refuta) P6 para todas essas entradas. */
        if (step < P6_SYM) {
            n = __VERIFIER_nondet_int();
            __ESBMC_assume(n >= 0 && n <= MAX_DETS);
        } else n = 0;
        for (d = 0; d < MAX_DETS; d++) {
            dets[d].x = __VERIFIER_nondet_int();
            __ESBMC_assume(dets[d].x >= 0 && dets[d].x < 160);
            dets[d].y = 60;
        }
#elif defined(P6_TRACE) && !defined(UNBOUNDED)
        /* P6, traco concreto: P6_TRACE objetos aparecem juntos no frame 0
         * (x = 10, 60, 110: longe entre si) e somem. Sem entrada simbolica:
         * o ESBMC executa o codigo real neste cenario e checa P6. */
        n = (step == 0) ? P6_TRACE : 0;
        for (d = 0; d < MAX_DETS; d++) { dets[d].x = 10 + 50 * (d % 3); dets[d].y = 60; }
#elif defined(CONCRETE_TRACE) && !defined(UNBOUNDED)
        /* RESERVA: traco concreto (o mesmo da sondagem dinamica): um objeto
         * entra no frame 0 em x=20 e some. Nao ha entrada simbolica; o ESBMC
         * apenas checa P5 neste traco. NAO e prova sobre todas as entradas. */
        n = (step == 0) ? 1 : 0;
        for (d = 0; d < MAX_DETS; d++) { dets[d].x = 20; dets[d].y = 60; }
#else
#ifdef FUZZ
        n = fuzz_rand(MAX_DETS + 1);
#else
        n = __VERIFIER_nondet_int();
#endif
        __ESBMC_assume(n >= 0 && n <= MAX_DETS);
        for (d = 0; d < MAX_DETS; d++) {
            dets[d].x = __VERIFIER_nondet_int();
#ifdef FIX_Y
            dets[d].y = 60;   /* y fixo: o firmware so classifica por x */
#else
            dets[d].y = __VERIFIER_nondet_int();
#endif
#ifdef ENV_E2
            __ESBMC_assume(dets[d].x >= 0 && dets[d].x < 160);
            __ESBMC_assume(dets[d].y >= 0 && dets[d].y < 120);
#endif
        }
#endif
#ifdef ENV_E1
        p = __VERIFIER_nondet_int() ? dets : (const argus_point_t *)0;
#else
        p = dets;
#endif

        /* fotografia dos slots antes do frame (para P4) */
        for (i = 0; i < ARGUS_MAX_TRACKS; i++) {
            was_active[i] = c.tracks[i].active;
            was_id[i]     = c.tracks[i].id;
        }

        /* ---------- sistema sob verificacao ----------
         * n e simbolico, mas a chamada e feita com n CONSTANTE em cada ramo:
         * assim o ESBMC desenrola os lacos internos de argus_update so ate n,
         * em vez de ate --unwind. Mesmo comportamento, formula muito menor. */
        if (n == 0) argus_update(&c, p, 0);
#if MAX_DETS >= 1
        else if (n == 1) argus_update(&c, p, 1);
#endif
#if MAX_DETS >= 2
        else if (n == 2) argus_update(&c, p, 2);
#endif
#if MAX_DETS >= 3
        else if (n == 3) argus_update(&c, p, 3);
#endif
#if MAX_DETS >= 4
        else if (n == 4) argus_update(&c, p, 4);
#endif
#if MAX_DETS >= 5
        else if (n == 5) argus_update(&c, p, 5);
#endif
#if MAX_DETS >= 6
        else if (n == 6) argus_update(&c, p, 6);
#endif
#if MAX_DETS >= 7
        else if (n == 7) argus_update(&c, p, 7);
#endif
#if MAX_DETS >= 8
        else argus_update(&c, p, 8);
#endif

        /* finalizado = estava ativo e (ficou inativo OU o slot foi
         * reutilizado por um track novo no mesmo frame) */
        for (i = 0; i < ARGUS_MAX_TRACKS; i++)
            if (was_active[i] && (!c.tracks[i].active || c.tracks[i].id != was_id[i])) {
                finalized++;
#ifdef P6
                if (q_n < ARGUS_MAX_EVENTS) {          /* mesma ordem do ev_push */
                    q_seen[(q_h + q_n) % ARGUS_MAX_EVENTS] = seen_prev[i];
                    q_n++;
                }
#endif
            }
#ifdef P6
        t += C_DETECT_US + C_CLASSIFY_US + c_update_us(n_before);
        for (i = 0; i < ARGUS_MAX_TRACKS; i++)
            if (c.tracks[i].active && c.tracks[i].missed == 0)
                seen[i] = t_cap;                       /* visto neste quadro */
        __ESBMC_assert(q_n == c.ev_count, "P6-aux: fila-sombra alinhada com a fila");
#endif

        /* ---------- host ---------- */
#ifdef ENV_E1
        if (__VERIFIER_nondet_int())
#endif
        {
            for (i = 0; i < ARGUS_MAX_EVENTS; i++) {
#ifdef P6
                e = argus_poll_event(&c);
                if (e == ARGUS_EVENT_NONE) break;
                polled++;
                t += C_FEEDBACK_US;
                lat = t - q_seen[q_h];                 /* do ultimo quadro visto ao bip */
                __ESBMC_assert(lat <= D_ERGO_US, "P6: bip em ate 800 ms apos o objeto sair");
                q_h = (q_h + 1) % ARGUS_MAX_EVENTS; q_n--;
                t += (e == ARGUS_EVENT_AMBIGUOUS ? 2UL : 1UL) * BEEP_PULSE_US;
#else
                if (argus_poll_event(&c) == ARGUS_EVENT_NONE) break;
                polled++;
#endif
            }
        }

        /* ---------- propriedades ---------- */
#ifndef NO_P3
        {
            int ativos = 0;
            for (i = 0; i < ARGUS_MAX_TRACKS; i++) ativos += (c.tracks[i].active != 0);
            __ESBMC_assert(c.n_tracks >= 0 && c.n_tracks <= ARGUS_MAX_TRACKS, "P3: 0 <= n_tracks <= 8");
            __ESBMC_assert(c.n_tracks == ativos, "P3: n_tracks == slots ativos");
            __ESBMC_assert(c.ev_count >= 0 && c.ev_count <= ARGUS_MAX_EVENTS, "P3: 0 <= ev_count <= 8");
            __ESBMC_assert(c.ev_head >= 0 && c.ev_head < ARGUS_MAX_EVENTS, "P3: ev_head no intervalo");
            __ESBMC_assert(c.ev_tail == (c.ev_head + c.ev_count) % ARGUS_MAX_EVENTS, "P3: ev_tail consistente");
        }
#endif
#ifndef NO_P4
#ifdef ARGUS_V2
        __ESBMC_assert(finalized == polled + (unsigned long)c.ev_count + (unsigned long)c.ev_dropped,
                       "P4: todo evento e entregue, pendente ou contado como descartado");
#ifdef ENV_E2
        __ESBMC_assert(c.ev_dropped == 0, "P4: com o host drenando, nenhum evento e descartado");
#endif
#else
        __ESBMC_assert(finalized == polled + (unsigned long)c.ev_count, "P4: nenhum evento perdido");
#endif
#endif
#ifndef NO_P5
        __ESBMC_assert(p5_allowed(prev, c.state), "P5: transicao fora da Figura 3");
#endif
        prev = c.state;
#ifdef INV
#ifndef NO_INV_ASSERT
        __ESBMC_assert(inv_ok(&c, prev), "INV: invariante preservada");
#endif
#endif
    }
    (void)S_NAME;
    return 0;
}

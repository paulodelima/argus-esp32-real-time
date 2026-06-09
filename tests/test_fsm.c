/* =====================================================================
 * test_fsm.c  --  Testes unitarios da maquina de estados ARGUS
 * Compilar e rodar em PC (gcc). Valida a logica IN/OUT/AMBIGUOUS e
 * o comportamento com N objetos simultaneos, SEM hardware.
 *
 *   gcc -I../firmware -o test_fsm test_fsm.c ../firmware/argus_fsm.c
 *   ./test_fsm
 * ===================================================================== */
#include <stdio.h>
#include "argus_fsm.h"

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do {                                   \
    if (cond) { g_pass++; }                                     \
    else { g_fail++; printf("  [FALHOU] %s\n", msg); }          \
} while (0)

static const char* ev_name(argus_event_t e) {
    switch (e) {
        case ARGUS_EVENT_IN:        return "IN";
        case ARGUS_EVENT_OUT:       return "OUT";
        case ARGUS_EVENT_AMBIGUOUS: return "AMBIGUOUS";
        default:                    return "NONE";
    }
}

/* Roda uma sequencia de frames de UM objeto andando de x0 a x1 e
 * depois sumindo (frames vazios para forcar a finalizacao). */
static argus_event_t run_single(int w, int h, int x0, int x1, int steps) {
    argus_ctx_t c; argus_init(&c, w, h);
    int i;
    for (i = 0; i <= steps; i++) {
        argus_point_t p;
        p.x = x0 + (x1 - x0) * i / steps;
        p.y = h / 2;
        argus_update(&c, &p, 1);
    }
    /* objeto sumiu: frames vazios ate finalizar */
    for (i = 0; i <= ARGUS_MAX_MISSED + 1; i++) argus_update(&c, 0, 0);
    return argus_poll_event(&c);
}

int main(void) {
    const int W = 320, H = 240;
    printf("== Testes ARGUS FSM ==\n");

    /* T1: porta(esq) -> interior(dir) deve dar IN */
    argus_event_t e = run_single(W, H, 20, 300, 10);
    printf("T1 esq->dir : %s\n", ev_name(e));
    CHECK(e == ARGUS_EVENT_IN, "trajetoria porta->interior deveria ser IN");

    /* T2: interior(dir) -> porta(esq) deve dar OUT */
    e = run_single(W, H, 300, 20, 10);
    printf("T2 dir->esq : %s\n", ev_name(e));
    CHECK(e == ARGUS_EVENT_OUT, "trajetoria interior->porta deveria ser OUT");

    /* T3: deslocamento minusculo deve dar AMBIGUOUS */
    e = run_single(W, H, 150, 165, 10);
    printf("T3 jitter   : %s\n", ev_name(e));
    CHECK(e == ARGUS_EVENT_AMBIGUOUS, "deslocamento pequeno deveria ser AMBIGUOUS");

    /* T4: movimento amplo mas sem cruzar o eixo -> AMBIGUOUS */
    e = run_single(W, H, 20, 140, 10); /* fica todo no lado da porta */
    printf("T4 sem cruz : %s\n", ev_name(e));
    CHECK(e == ARGUS_EVENT_AMBIGUOUS, "sem cruzar o eixo deveria ser AMBIGUOUS");

    /* T5: tres objetos simultaneos -> N chega a 3 */
    {
        argus_ctx_t c; argus_init(&c, W, H);
        int t, maxN = 0;
        /* 3 objetos entrando em faixas y diferentes, da esq pra dir */
        for (t = 0; t <= 10; t++) {
            argus_point_t d[3];
            int x = 20 + 280 * t / 10;
            d[0].x = x; d[0].y = 60;
            d[1].x = x; d[1].y = 120;
            d[2].x = x; d[2].y = 180;
            argus_update(&c, d, 3);
            if (argus_active_tracks(&c) > maxN) maxN = argus_active_tracks(&c);
        }
        printf("T5 N maximo : %d\n", maxN);
        CHECK(maxN == 3, "deveria rastrear 3 objetos simultaneos");

        /* somem: deve gerar 3 eventos IN */
        for (t = 0; t <= ARGUS_MAX_MISSED + 1; t++) argus_update(&c, 0, 0);
        int ins = 0, ev_total = 0;
        argus_event_t ev;
        while ((ev = argus_poll_event(&c)) != ARGUS_EVENT_NONE) {
            ev_total++;
            if (ev == ARGUS_EVENT_IN) ins++;
        }
        printf("T5 eventos  : %d (IN=%d)\n", ev_total, ins);
        CHECK(ev_total == 3, "deveria emitir 3 eventos");
        CHECK(ins == 3, "os 3 deveriam ser IN");
    }

    /* T6: nao casa mais que ARGUS_MAX_TRACKS (saturacao) */
    {
        argus_ctx_t c; argus_init(&c, W, H);
        argus_point_t d[20];
        for (int k = 0; k < 20; k++) { d[k].x = 10 + k*15; d[k].y = 120; }
        argus_update(&c, d, 20);
        printf("T6 satur.   : N=%d (limite=%d)\n",
               argus_active_tracks(&c), ARGUS_MAX_TRACKS);
        CHECK(argus_active_tracks(&c) <= ARGUS_MAX_TRACKS,
              "N nao pode exceder ARGUS_MAX_TRACKS");
    }

    printf("\n== Resultado: %d passou, %d falhou ==\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}

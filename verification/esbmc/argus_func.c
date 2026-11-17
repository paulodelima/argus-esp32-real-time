/* =====================================================================
 * argus_func.c -- harness FUNCIONAL: o bip reflete a trajetoria?
 *
 * Um objeto aparece por PRES frames (1 deteccao por frame), em posicoes x
 * QUAISQUER (y fixo), andando no maximo GATE px por frame (o que o tracking
 * consegue seguir), e depois some. O host drena a fila a cada frame.
 *
 * Propriedades (especificacao independente do codigo de classify):
 *   P7 classificacao: travessia porta->interior (inicio x<80, fim x>80,
 *      deslocamento >= 20% da largura) gera IN; interior->porta gera OUT;
 *      qualquer outro caso gera AMBIGUOUS.
 *   P7 unicidade: exatamente UM bip por objeto (nem zero, nem duplicado).
 *   P8 momento: o bip sai no 4o quadro sem o objeto (ARGUS_MAX_MISSED+1),
 *      nem antes, nem depois.
 * Convencao do lado da porta: Figura 2 do relatorio (porta em x < centro).
 * -DPRES=n (frames com o objeto, padrao 3). -DNO_P7 / -DNO_P8.
 * ===================================================================== */
#include "argus_fsm.h"

#ifdef CHECK_WITH_GCC
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
static int seeded = 0;
static int __VERIFIER_nondet_int(void) {
    if (!seeded) { srand((unsigned)(getpid() ^ time(0))); seeded = 1; }
    return rand() % 160;
}
#define __ESBMC_assume(c) do { if (!(c)) exit(0); } while (0)
#define __ESBMC_assert(c, m) assert((c) && (m))
#else
int __VERIFIER_nondet_int(void);
void __ESBMC_assume(_Bool);
void __ESBMC_assert(_Bool, const char *);
#endif

#ifndef PRES
#define PRES 3
#endif
#define W 160
#define H 120
#define CENTER (W / 2)
#define GATE ((W * 25) / 100)           /* 40 px: alcance do tracking */
#define MIN_TRAVEL ((W * 20) / 100)     /* 32 px: deslocamento minimo  */
#define ABSENT_TO_BIP 4                 /* MAX_MISSED + 1 quadros       */
#define STEPS (PRES + ABSENT_TO_BIP + 1) /* +1: confere que nao ha 2o bip */

static int iabs_(int v) { return v < 0 ? -v : v; }

int main(void) {
    argus_ctx_t c;
    argus_point_t det;
    int x[PRES], k, step, n, e;
    int n_events = 0, ev_type = ARGUS_EVENT_NONE, ev_step = -1;

    for (k = 0; k < PRES; k++) {
        x[k] = __VERIFIER_nondet_int();
        __ESBMC_assume(x[k] >= 0 && x[k] < W);
        if (k > 0) __ESBMC_assume(iabs_(x[k] - x[k - 1]) <= GATE);
    }

    argus_init(&c, W, H);
    for (step = 0; step < STEPS; step++) {
        n = (step < PRES) ? 1 : 0;
        det.x = (step < PRES) ? x[step] : 0;
        det.y = 60;
        argus_update(&c, &det, n);
        while ((e = argus_poll_event(&c)) != ARGUS_EVENT_NONE) {
            n_events++;
            ev_type = e;
            ev_step = step;
        }
    }

    {
        int s = x[0], f = x[PRES - 1], expected;
        if (s < CENTER && f > CENTER && f - s >= MIN_TRAVEL)      expected = ARGUS_EVENT_IN;
        else if (s > CENTER && f < CENTER && s - f >= MIN_TRAVEL) expected = ARGUS_EVENT_OUT;
        else                                                      expected = ARGUS_EVENT_AMBIGUOUS;
#ifndef NO_P7
        __ESBMC_assert(n_events == 1, "P7: exatamente um bip por objeto");
        __ESBMC_assert(n_events != 1 || ev_type == expected, "P7: tipo do bip corresponde a trajetoria");
#endif
#ifndef NO_P8
        __ESBMC_assert(n_events == 0 || ev_step == PRES - 1 + ABSENT_TO_BIP,
                       "P8: bip no 4o quadro sem o objeto");
#endif
        (void)expected;
    }
    return 0;
}

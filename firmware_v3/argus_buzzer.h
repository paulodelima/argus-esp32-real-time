/* argus_buzzer.h -- buzzer NAO bloqueante do ARGUS (v3).
 *
 * Na v2 o loop() chamava beep(), que fazia tone() + delay(): o laco ficava
 * parado 180 ms por pulso. Aqui o buzzer vira uma agenda de pulsos:
 *   - buzzer_push(): agenda n pulsos para um evento e devolve o instante
 *     em que o PRIMEIRO pulso comeca (o "inicio do bip" da propriedade P6);
 *   - buzzer_level(): diz se o buzzer deve estar ligado no instante now.
 * O laco nunca espera. O buzzer continua sendo UM atuador: os pulsos de
 * eventos diferentes saem em sequencia, sem sobreposicao, na ordem dos
 * eventos (propriedade P9 no harness).
 * Codigo C puro, sem Arduino: o mesmo arquivo roda na placa e e verificado.
 */
#ifndef ARGUS_BUZZER_H
#define ARGUS_BUZZER_H
/* O ESBMC 8.6 (macOS) nao traz stdint.h: os scripts passam -DARGUS_NO_STDINT. */
#ifdef ARGUS_NO_STDINT
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
#else
#include <stdint.h>
#endif

#define BUZZER_MAX_SEGS 8            /* = ARGUS_MAX_EVENTS */

typedef struct {
    uint64_t start;                  /* inicio do primeiro pulso (us) */
    uint32_t pulses;                 /* numero de pulsos */
} buzzer_seg_t;

typedef struct {
    uint32_t on_us;                  /* duracao do tom de cada pulso */
    uint32_t period_us;              /* tom + silencio de cada pulso */
    uint64_t free_at;                /* instante em que o buzzer fica livre */
    buzzer_seg_t seg[BUZZER_MAX_SEGS];
    int head, count;
    uint32_t dropped;                /* agendamentos recusados (agenda cheia) */
} argus_buzzer_t;

void     buzzer_init(argus_buzzer_t *b, uint32_t on_us, uint32_t period_us);
/* agenda n pulsos a partir de now; devolve o inicio do 1o pulso
 * (0 se a agenda estiver cheia ou os parametros forem invalidos) */
uint64_t buzzer_push(argus_buzzer_t *b, uint32_t n, uint64_t now);
/* 1 = tom ligado em now, 0 = desligado; descarta segmentos ja tocados */
int      buzzer_level(argus_buzzer_t *b, uint64_t now);
#endif

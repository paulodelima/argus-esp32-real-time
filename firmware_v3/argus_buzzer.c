/* argus_buzzer.c -- ver argus_buzzer.h */
#include "argus_buzzer.h"

void buzzer_init(argus_buzzer_t *b, uint32_t on_us, uint32_t period_us) {
    int i;
    if (!b) return;
    b->on_us = on_us;
    b->period_us = (period_us > on_us) ? period_us : on_us + 1u;
    b->free_at = 0;
    b->head = 0; b->count = 0; b->dropped = 0;
    for (i = 0; i < BUZZER_MAX_SEGS; i++) { b->seg[i].start = 0; b->seg[i].pulses = 0; }
}

uint64_t buzzer_push(argus_buzzer_t *b, uint32_t n, uint64_t now) {
    uint64_t start;
    int k;
    if (!b || n == 0 || n > 4) return 0;
    if (b->count >= BUZZER_MAX_SEGS) { b->dropped++; return 0; }
    start = (now > b->free_at) ? now : b->free_at;   /* espera o pulso anterior */
    k = (b->head + b->count) % BUZZER_MAX_SEGS;
    b->seg[k].start = start;
    b->seg[k].pulses = n;
    b->count++;
    b->free_at = start + (uint64_t)n * b->period_us;
    return start;
}

int buzzer_level(argus_buzzer_t *b, uint64_t now) {
    uint64_t end, off;
    if (!b) return 0;
    while (b->count > 0) {
        end = b->seg[b->head].start + (uint64_t)b->seg[b->head].pulses * b->period_us;
        if (now < end) break;
        b->head = (b->head + 1) % BUZZER_MAX_SEGS;   /* segmento ja tocado */
        b->count--;
    }
    if (b->count == 0 || now < b->seg[b->head].start) return 0;
    off = (now - b->seg[b->head].start) % b->period_us;
    return off < b->on_us;
}

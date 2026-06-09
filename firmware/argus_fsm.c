/* =====================================================================
 * argus_fsm.c  --  Implementacao da maquina de estados ARGUS
 * Sem dependencia de hardware, sem malloc, sem float obrigatorio.
 * ===================================================================== */
#include "argus_fsm.h"

/* abs inteiro local (evita depender de stdlib no embarcado) */
static int iabs(int v) { return v < 0 ? -v : v; }

/* --------------------------- helpers ---------------------------- */
static void ev_push(argus_ctx_t *c, argus_event_t e) {
    if (c->ev_count >= ARGUS_MAX_EVENTS) return; /* fila cheia: descarta */
    c->ev_q[c->ev_tail] = e;
    c->ev_tail = (c->ev_tail + 1) % ARGUS_MAX_EVENTS;
    c->ev_count++;
}

/* Classifica uma trajetoria finalizada comparando inicio x fim em
 * relacao ao eixo central. Convencao (ver Figura 2): a PORTA fica do
 * lado x < center_x; o INTERIOR fica do lado x > center_x.
 *   porta -> interior  => IN  (entrando)
 *   interior -> porta  => OUT (saindo)
 * Deslocamento pequeno ou sem cruzar o eixo => AMBIGUOUS. */
static argus_event_t classify(const argus_ctx_t *c, const argus_track_t *t) {
    int travel = iabs(t->last_x - t->start_x);
    int min_travel = (c->frame_w * ARGUS_MIN_TRAVEL_PCT) / 100;

    if (travel < min_travel)
        return ARGUS_EVENT_AMBIGUOUS;          /* mexeu de leve / ruido */

    int started_door    = (t->start_x < c->center_x);
    int ended_interior  = (t->last_x  > c->center_x);
    int started_interior= (t->start_x > c->center_x);
    int ended_door      = (t->last_x  < c->center_x);

    if (started_door && ended_interior)  return ARGUS_EVENT_IN;
    if (started_interior && ended_door)  return ARGUS_EVENT_OUT;
    return ARGUS_EVENT_AMBIGUOUS;               /* nao cruzou o eixo */
}

/* Finaliza um track: classifica, enfileira evento, desativa. */
static void finalize(argus_ctx_t *c, int idx) {
    argus_event_t e = classify(c, &c->tracks[idx]);
    ev_push(c, e);
    c->last_event = e;
    c->tracks[idx].active = 0;
    if (c->n_tracks > 0) c->n_tracks--;
}

/* Encontra slot livre; -1 se nao houver. */
static int free_slot(const argus_ctx_t *c) {
    for (int i = 0; i < ARGUS_MAX_TRACKS; i++)
        if (!c->tracks[i].active) return i;
    return -1;
}

/* ----------------------------- API ------------------------------ */
void argus_init(argus_ctx_t *c, int w, int h) {
    int i;
    c->state = ARGUS_IDLE;
    c->frame_w = w;
    c->frame_h = h;
    c->center_x = w / 2;
    c->next_id = 1;
    c->n_tracks = 0;
    c->ev_head = c->ev_tail = c->ev_count = 0;
    c->last_event = ARGUS_EVENT_NONE;
    for (i = 0; i < ARGUS_MAX_TRACKS; i++) c->tracks[i].active = 0;
}

int argus_active_tracks(const argus_ctx_t *c) { return c->n_tracks; }

argus_event_t argus_poll_event(argus_ctx_t *c) {
    if (c->ev_count == 0) return ARGUS_EVENT_NONE;
    argus_event_t e = c->ev_q[c->ev_head];
    c->ev_head = (c->ev_head + 1) % ARGUS_MAX_EVENTS;
    c->ev_count--;
    return e;
}

argus_state_t argus_update(argus_ctx_t *c,
                           const argus_point_t *dets, int n_dets) {
    int i, d;
    int gate = (c->frame_w * ARGUS_GATE_PCT) / 100;
    int matched_det[ARGUS_MAX_TRACKS]; /* indice da deteccao casada, -1 nao */
    int det_used[64];                  /* deteccoes ja consumidas           */

    for (d = 0; d < n_dets && d < 64; d++) det_used[d] = 0;
    for (i = 0; i < ARGUS_MAX_TRACKS; i++) matched_det[i] = -1;

    /* --- 1. casamento guloso vizinho-mais-proximo track<-deteccao --- */
    for (i = 0; i < ARGUS_MAX_TRACKS; i++) {
        if (!c->tracks[i].active) continue;
        int best = -1, best_dist = gate + 1;
        for (d = 0; d < n_dets && d < 64; d++) {
            if (det_used[d]) continue;
            int dist = iabs(dets[d].x - c->tracks[i].last_x)
                     + iabs(dets[d].y - c->tracks[i].last_y); /* L1 */
            if (dist <= gate && dist < best_dist) { best_dist = dist; best = d; }
        }
        if (best >= 0) {
            matched_det[i] = best;
            det_used[best] = 1;
        }
    }

    /* --- 2. atualiza tracks (casados ou perdidos) --- */
    for (i = 0; i < ARGUS_MAX_TRACKS; i++) {
        if (!c->tracks[i].active) continue;
        c->tracks[i].age++;
        if (matched_det[i] >= 0) {
            int d2 = matched_det[i];
            c->tracks[i].last_x = dets[d2].x;
            c->tracks[i].last_y = dets[d2].y;
            c->tracks[i].missed = 0;
        } else {
            c->tracks[i].missed++;
        }
        /* tracking perdido por idade -> falha (bip duplo) */
        if (c->tracks[i].age > ARGUS_MAX_AGE) {
            ev_push(c, ARGUS_EVENT_AMBIGUOUS);
            c->last_event = ARGUS_EVENT_AMBIGUOUS;
            c->tracks[i].active = 0;
            if (c->n_tracks > 0) c->n_tracks--;
            continue;
        }
        /* saiu do campo (missed demais) -> finaliza e classifica */
        if (c->tracks[i].missed > ARGUS_MAX_MISSED) {
            finalize(c, i);
        }
    }

    /* --- 3. deteccoes nao casadas viram tracks novos --- */
    for (d = 0; d < n_dets && d < 64; d++) {
        if (det_used[d]) continue;
        int slot = free_slot(c);
        if (slot < 0) break; /* saturacao de carga */
        c->tracks[slot].active  = 1;
        c->tracks[slot].id      = c->next_id++;
        c->tracks[slot].start_x = dets[d].x;
        c->tracks[slot].last_x  = dets[d].x;
        c->tracks[slot].last_y  = dets[d].y;
        c->tracks[slot].age     = 1;
        c->tracks[slot].missed  = 0;
        c->n_tracks++;
    }

    /* --- 4. transicao de estado (fiel a Figura 3) --- */
    if (c->ev_count > 0) {
        /* ha algo para classificar/sinalizar */
        c->state = (c->n_tracks > 0) ? ARGUS_DECIDING : ARGUS_FEEDBACK;
    } else if (c->n_tracks > 0) {
        c->state = ARGUS_TRACKING;
    } else {
        c->state = ARGUS_IDLE;
    }
    return c->state;
}

/* =====================================================================
 * argus_fsm.c  --  Implementacao da maquina de estados ARGUS  (MUTANTE M2: argus_poll_event nao decrementa ev_count)
 * Sem dependencia de hardware, sem malloc, sem float obrigatorio.
 * Mudancas em relacao a v1 marcadas com "v2:".
 * ===================================================================== */
#include "argus_fsm.h"

/* abs inteiro local (evita depender de stdlib no embarcado) */
static int iabs(int v) { return v < 0 ? -v : v; }

/* --------------------------- helpers ---------------------------- */
/* v2: retorna 1 se enfileirou; 0 se a fila estava cheia (e conta). */
static int ev_push(argus_ctx_t *c, argus_event_t e) {
    if (c->ev_count >= ARGUS_MAX_EVENTS) {
        if (c->ev_dropped < 0x7FFFFFFF) c->ev_dropped++;   /* v2: observavel */
        return 0;
    }
    c->ev_q[c->ev_tail] = e;
    c->ev_tail = (c->ev_tail + 1) % ARGUS_MAX_EVENTS;
    c->ev_count++;
    return 1;
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
#define ARGUS_MAX_DIM 4096   /* v2: limite de resolucao (evita overflow) */

void argus_init(argus_ctx_t *c, int w, int h) {
    int i;
    if (w < 2) w = 2;                                   /* v2: limites */
    if (w > ARGUS_MAX_DIM) w = ARGUS_MAX_DIM;
    if (h < 2) h = 2;
    if (h > ARGUS_MAX_DIM) h = ARGUS_MAX_DIM;
    c->state = ARGUS_IDLE;
    c->frame_w = w;
    c->frame_h = h;
    c->center_x = w / 2;
    c->next_id = 1;
    c->n_tracks = 0;
    c->ev_head = c->ev_tail = c->ev_count = 0;
    c->last_event = ARGUS_EVENT_NONE;
    c->ev_dropped = 0;                                             /* v2 */
    for (i = 0; i < ARGUS_MAX_TRACKS; i++) c->tracks[i].active = 0;
}

int argus_active_tracks(const argus_ctx_t *c) { return c->n_tracks; }

argus_event_t argus_poll_event(argus_ctx_t *c) {
    if (c->ev_count == 0) return ARGUS_EVENT_NONE;
    argus_event_t e = c->ev_q[c->ev_head];
    c->ev_head = (c->ev_head + 1) % ARGUS_MAX_EVENTS;
    return e;
}

argus_state_t argus_update(argus_ctx_t *c,
                           const argus_point_t *dets, int n_dets) {
    int i, d;
    int gate = (c->frame_w * ARGUS_GATE_PCT) / 100;
    int matched_det[ARGUS_MAX_TRACKS]; /* indice da deteccao casada, -1 nao */
    int det_used[64];                  /* deteccoes ja consumidas           */
    int finalized_now = 0;             /* v2: tracks encerrados neste frame */

    /* v2: contrato de entrada */
    if (dets == 0 || n_dets < 0) n_dets = 0;
    if (n_dets > 64) n_dets = 64;
    for (d = 0; d < n_dets; d++)
        det_used[d] = (dets[d].x < 0 || dets[d].x >= c->frame_w ||
                       dets[d].y < 0 || dets[d].y >= c->frame_h);   /* ignora */
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
            finalized_now++;                                       /* v2 */
            continue;
        }
        /* saiu do campo (missed demais) -> finaliza e classifica */
        if (c->tracks[i].missed > ARGUS_MAX_MISSED) {
            finalize(c, i);
            finalized_now++;                                       /* v2 */
        }
    }

    /* --- 3. deteccoes nao casadas viram tracks novos --- */
    for (d = 0; d < n_dets && d < 64; d++) {
        if (det_used[d]) continue;
        int slot = free_slot(c);
        if (slot < 0) break; /* saturacao de carga */
        c->tracks[slot].active  = 1;
        c->tracks[slot].id      = c->next_id;
        c->next_id = (c->next_id >= 0x3FFFFFFF) ? 1 : c->next_id + 1;  /* v2 */
        c->tracks[slot].start_x = dets[d].x;
        c->tracks[slot].last_x  = dets[d].x;
        c->tracks[slot].last_y  = dets[d].y;
        c->tracks[slot].age     = 1;
        c->tracks[slot].missed  = 0;
        c->n_tracks++;
    }

    /* --- 4. transicao de estado --- v2: explicita, por estado atual.
     *   IDLE     -> TRACKING  quando ha tracks ativos
     *   TRACKING -> DECIDING  quando algum track foi encerrado neste frame
     *   DECIDING -> FEEDBACK  no frame seguinte (decisao = evento na fila)
     *   FEEDBACK -> IDLE      quando o host esvaziou a fila e nao ha tracks
     *   FEEDBACK -> TRACKING  quando esvaziou a fila mas ainda ha tracks
     * (refinamento da Figura 3 para multiplos objetos) */
    switch (c->state) {
    case ARGUS_IDLE:
        if (c->n_tracks > 0) c->state = ARGUS_TRACKING;
        break;
    case ARGUS_TRACKING:
        if (finalized_now > 0) c->state = ARGUS_DECIDING;
        break;
    case ARGUS_DECIDING:
        c->state = ARGUS_FEEDBACK;
        break;
    case ARGUS_FEEDBACK:
    default:
        if (c->ev_count == 0)
            c->state = (c->n_tracks > 0) ? ARGUS_TRACKING : ARGUS_IDLE;
        break;
    }
    return c->state;
}

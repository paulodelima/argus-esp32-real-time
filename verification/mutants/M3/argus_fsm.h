/* =====================================================================
 * argus_fsm.h  --  Maquina de estados do firmware ARGUS  (v2, candidata)
 *
 * v2: mudancas guiadas por verificacao (ver firmware_v2/CHANGES.md).
 *
 * NUCLEO independente de hardware. O MESMO arquivo .c eh usado por:
 *   (a) o firmware ESP32-CAM (argus_esp32cam.ino)
 *   (b) os testes unitarios em PC (tests/test_fsm.c)
 *   (c) a verificacao formal no ESBMC (verification/argus_wcet.c)
 *
 * A camada de visao (deteccao de pixels de movimento -> centroides)
 * fica FORA daqui, de proposito: este modulo so consome centroides.
 * Essa separacao eh o que torna a logica testavel e verificavel.
 *
 * Estados (Figura 3 da proposta):
 *   IDLE -> TRACKING -> DECIDING -> FEEDBACK -> IDLE
 * ===================================================================== */
#ifndef ARGUS_FSM_H
#define ARGUS_FSM_H

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------- parametros de projeto --------------------- */
#define ARGUS_MAX_TRACKS    8   /* limite fisico de tracks simultaneos  */
#define ARGUS_MAX_MISSED    3   /* frames sem casar antes de finalizar   */
#define ARGUS_MAX_EVENTS    8   /* fila de eventos pendentes de feedback */

/* Distancia (em % da largura) para casar deteccao->track. Acima disso,
 * a deteccao vira um track novo. */
#define ARGUS_GATE_PCT      25

/* Deslocamento horizontal minimo (em % da largura) para considerar a
 * trajetoria valida. Abaixo disso -> AMBIGUOUS (bip duplo). */
#define ARGUS_MIN_TRAVEL_PCT 20

/* Idade maxima de um track (frames) antes de declarar tracking perdido.
 * Transicao tracejada da Figura 3 -> AMBIGUOUS. */
#define ARGUS_MAX_AGE       60

/* --------------------------- tipos ------------------------------ */
typedef enum {
    ARGUS_IDLE = 0,
    ARGUS_TRACKING,
    ARGUS_DECIDING,
    ARGUS_FEEDBACK
} argus_state_t;

typedef enum {
    ARGUS_EVENT_NONE = 0,
    ARGUS_EVENT_IN,        /* bip simples */
    ARGUS_EVENT_OUT,       /* bip simples */
    ARGUS_EVENT_AMBIGUOUS  /* bip duplo   */
} argus_event_t;

typedef struct { int x; int y; } argus_point_t;

typedef struct {
    int active;
    int id;
    int start_x;   /* x na primeira deteccao */
    int last_x;    /* x na deteccao mais recente */
    int last_y;
    int age;       /* frames vivos */
    int missed;    /* frames consecutivos sem casar */
} argus_track_t;

typedef struct {
    argus_state_t state;
    int frame_w, frame_h;
    int center_x;        /* eixo de classificacao (porta | interior) */
    int next_id;
    int n_tracks;        /* tracks ATIVOS == o N do modelo de custo */

    argus_track_t tracks[ARGUS_MAX_TRACKS];

    /* fila circular de eventos prontos para feedback */
    argus_event_t ev_q[ARGUS_MAX_EVENTS];
    int ev_head, ev_tail, ev_count;

    argus_event_t last_event; /* ultimo evento despachado (debug) */

    /* v2: eventos descartados por fila cheia. Nunca mais e silencioso:
     * o host pode ler e decidir (log, bip de erro, aumentar a fila). */
    int ev_dropped;
} argus_ctx_t;

/* ----------------------------- API ------------------------------ */

/* Inicializa o contexto. center_x default = w/2 (eixo central). */
void argus_init(argus_ctx_t *ctx, int w, int h);

/* Processa as deteccoes de UM frame. `dets` tem `n_dets` centroides.
 * Atualiza tracks, finaliza os que sairam e enfileira eventos.
 * Retorna o estado resultante.
 * v2, contrato de entrada: dets==NULL ou n_dets<=0 vale como frame vazio;
 * n_dets e limitado a 64; deteccoes fora de [0,w)x[0,h) sao ignoradas. */
argus_state_t argus_update(argus_ctx_t *ctx,
                           const argus_point_t *dets, int n_dets);

/* Retira um evento da fila (ARGUS_EVENT_NONE se vazia). O loop do host
 * chama isto para acionar o buzzer. */
argus_event_t argus_poll_event(argus_ctx_t *ctx);

/* Numero de tracks ativos = N corrente do modelo Ctotal(N). */
int argus_active_tracks(const argus_ctx_t *ctx);

#ifdef __cplusplus
}
#endif
#endif /* ARGUS_FSM_H */

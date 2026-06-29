/* =====================================================================
 * argus_esp32cam.ino  --  Firmware ARGUS para ESP32-CAM (AI-Thinker)
 *
 * Pipeline: captura grayscale -> deteccao de movimento (frame diff) ->
 * centroides -> argus_fsm (mesma logica do PC) -> buzzer.
 * Instrumentado com esp_timer para MEDIR Cdetect, Ctrack, Cclassify,
 * Cfeedback (Dia 4 do sprint). Os MAXIMOS impressos no Serial sao suas
 * estimativas de WCET para alimentar argus_wcet.c.
 *
 * IMPORTANTE:
 *  - Coloque argus_fsm.h e argus_fsm.c NA MESMA PASTA do .ino.
 *  - Placa: "AI Thinker ESP32-CAM". Confira o pinout se seu modulo
 *    for diferente.
 *  - ESP32-CAM NAO tem USB: grave via adaptador FTDI, GPIO0->GND no boot.
 *  - Buzzer no GPIO 13 (passivo: use tone(); ativo: digitalWrite).
 *    Evite GPIO 4 (LED flash) e os pinos usados pela camera.
 *
 * Arduino IDE: Tools -> PSRAM -> Enabled.
 * ===================================================================== */
#include "esp_camera.h"
#include "esp_timer.h"
extern "C" {
  #include "argus_fsm.h"
}

/* ---------------- pinos da camera (AI-Thinker) ------------------- */
#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22

#define BUZZER_PIN      13

/* ----------------- parametros de processamento ------------------ */
#define IMG_W           160          /* FRAMESIZE_QQVGA = 160x120 */
#define IMG_H           120
#define DIFF_THRESH     25           /* limiar de movimento (0-255) */
#define GRID            8            /* malha de blobs GRIDxGRID    */
#define CELL_MIN_PIX    20           /* pixels em movimento p/ celula "ativa" */

static uint8_t prev_frame[IMG_W * IMG_H];
static uint8_t baseline_frame[IMG_W * IMG_H]; /* cena em repouso p/ delta antes/depois */
static bool    baseline_valid = false;        /* baseline ja capturado ao menos 1x? */
static long g_motion_mass = 0;   /* total de pixels em movimento no ultimo frame */
static long g_peak_mass = 0;     /* pico de massa durante a passagem atual */
static bool g_in_pass   = false; /* true enquanto ha mao no quadro */
static bool    have_prev = false;
static argus_ctx_t argus;

/* --------- acumuladores de WCET (microssegundos) --------------- */
static uint32_t wc_detect = 0, wc_feedback = 0;
/* WCET de argus_update() indexado pelo N de tracks ATIVOS durante o frame.
 * Modelo: C_total(N) = C_detect + C_update(N) + C_feedback.
 * C_update(N) engloba matching, tracking, classificacao e criacao de tracks
 * — a "funcao unica cuja carga depende de N" (nao tarefas independentes,
 * por isso Liu-Layland nao se aplica). */
static uint32_t wc_update[ARGUS_MAX_TRACKS + 1] = {0};

/* ------------------------ buzzer -------------------------------- */
static void beep(int n) {
  for (int i = 0; i < n; i++) {
    tone(BUZZER_PIN, 2000, 80);   /* 2 kHz, 80 ms */
    delay(120);
    noTone(BUZZER_PIN);
    delay(60);
  }
}

/* ------------------------ camera init --------------------------- */
static bool camera_init() {
  camera_config_t c = {};
  c.ledc_channel = LEDC_CHANNEL_0; c.ledc_timer = LEDC_TIMER_0;
  c.pin_d0=Y2_GPIO_NUM; c.pin_d1=Y3_GPIO_NUM; c.pin_d2=Y4_GPIO_NUM;
  c.pin_d3=Y5_GPIO_NUM; c.pin_d4=Y6_GPIO_NUM; c.pin_d5=Y7_GPIO_NUM;
  c.pin_d6=Y8_GPIO_NUM; c.pin_d7=Y9_GPIO_NUM;
  c.pin_xclk=XCLK_GPIO_NUM; c.pin_pclk=PCLK_GPIO_NUM;
  c.pin_vsync=VSYNC_GPIO_NUM; c.pin_href=HREF_GPIO_NUM;
  c.pin_sccb_sda=SIOD_GPIO_NUM; c.pin_sccb_scl=SIOC_GPIO_NUM;
  c.pin_pwdn=PWDN_GPIO_NUM; c.pin_reset=RESET_GPIO_NUM;
  c.xclk_freq_hz=20000000;
  c.pixel_format=PIXFORMAT_GRAYSCALE;   /* 1 byte/pixel: leve e suficiente */
  c.frame_size=FRAMESIZE_QQVGA;         /* 160x120 */
  c.fb_count=1;
  c.grab_mode=CAMERA_GRAB_LATEST;
  return esp_camera_init(&c) == ESP_OK;
}

/* ---- deteccao de movimento -> ate ARGUS_MAX_TRACKS centroides ---- */
static int detect(const uint8_t *cur, argus_point_t *out) {
  /* contagem de pixels em movimento por celula da malha */
  static int cell_cnt[GRID][GRID];
  static long sx[GRID][GRID], sy[GRID][GRID];
  for (int gy=0; gy<GRID; gy++) for (int gx=0; gx<GRID; gx++)
    { cell_cnt[gy][gx]=0; sx[gy][gx]=0; sy[gy][gx]=0; }

  const int cw = IMG_W / GRID, ch = IMG_H / GRID;
  for (int y=0; y<IMG_H; y++) {
    for (int x=0; x<IMG_W; x++) {
      int idx = y*IMG_W + x;
      int d = (int)cur[idx] - (int)prev_frame[idx];
      if (d < 0) d = -d;
      if (d > DIFF_THRESH) {
        int gx = x/cw, gy = y/ch;
        if (gx>=GRID) gx=GRID-1; if (gy>=GRID) gy=GRID-1;
        cell_cnt[gy][gx]++; sx[gy][gx]+=x; sy[gy][gx]+=y;
      }
    }
  }
  /* massa de movimento = soma de pixels em movimento em todas as celulas.
   * proxy de "tamanho do blob": mao vazia -> pequena; mao + produto -> grande. */
  g_motion_mass = 0;
  for (int gy=0; gy<GRID; gy++)
    for (int gx=0; gx<GRID; gx++)
      g_motion_mass += cell_cnt[gy][gx];

  /* cada celula "ativa" vira um centroide candidato (simplificacao
   * deliberada: blobs adjacentes podem se fundir; suficiente p/ MVP) */
  int n = 0;
  for (int gy=0; gy<GRID && n<ARGUS_MAX_TRACKS; gy++)
    for (int gx=0; gx<GRID && n<ARGUS_MAX_TRACKS; gx++)
      if (cell_cnt[gy][gx] >= CELL_MIN_PIX) {
        out[n].x = (int)(sx[gy][gx]/cell_cnt[gy][gx]);
        out[n].y = (int)(sy[gy][gx]/cell_cnt[gy][gx]);
        n++;
      }
  return n;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(BUZZER_PIN, OUTPUT);
  if (!camera_init()) { Serial.println("ERRO: camera"); while(1) delay(1000); }
  argus_init(&argus, IMG_W, IMG_H);
  Serial.println("ARGUS pronto. Mova objetos sob a camera.");
}

void loop() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;

  argus_point_t dets[ARGUS_MAX_TRACKS];
  int n = 0;

  /* -- Cdetect -- */
  int64_t t0 = esp_timer_get_time();
  if (have_prev) n = detect(fb->buf, dets);
  memcpy(prev_frame, fb->buf, IMG_W*IMG_H);
  have_prev = true;
  uint32_t dt_detect = (uint32_t)(esp_timer_get_time() - t0);
  if (dt_detect > wc_detect) wc_detect = dt_detect;
  /* CALIBRACAO mao cheia/vazia por PASSAGEM (nao por frame):
   * acumula o PICO de massa enquanto ha mao no quadro; quando a mao sai
   * (sem deteccao), reporta o pico daquela passagem inteira e reseta.
   * Compare PICO de mao VAZIA vs mao COM PRODUTO. */
  if (n > 0) {
    g_in_pass = true;
    if (g_motion_mass > g_peak_mass) g_peak_mass = g_motion_mass;
  } else if (g_in_pass) {
    /* a mao acabou de sair: reporta o pico da passagem */
    Serial.printf("PICO_MASSA=%ld\n", g_peak_mass);
    g_peak_mass = 0;
    g_in_pass = false;
  }

  /* ===================================================================
   * INSTRUMENTACAO DE COLETA — duracao da interacao (REMOVIVEL)
   * -------------------------------------------------------------------
   * Mede empiricamente a DURACAO de uma interacao de inventario, para
   * calibrar cientificamente o timeout de pareamento. NAO decide MA/MB;
   * apenas cronometra e imprime.
   *
   * MODELO: "cacho de atividade de movimento".
   *   A deteccao e por diferenca de frames => n>0 significa MOVIMENTO
   *   agora; n==0 significa SEM movimento agora (mao parada OU cena vazia
   *   — indistinguivel, e nao importa). Como a mao NUNCA sai do quadro
   *   numa interacao real, nao existe "ausencia"; existem rajadas de
   *   movimento separadas por pausas curtas (ex.: pegar o objeto).
   *
   *   T_inicio = 1o movimento a partir do repouso
   *   ... rajadas + pausas curtas (costuradas) ...
   *   T_fim    = ultimo movimento antes de a cena ficar estatica
   *   DURACAO  = T_fim - T_inicio  (ms)
   *
   *   A interacao so FECHA apos IA_END_GAP_US sem movimento (cena voltou
   *   ao repouso). Pausas internas menores que isso sao costuradas, nao
   *   viram fronteira. Logamos 'rajadas' e 'maior_pausa' p/ verificar
   *   empiricamente se IA_END_GAP_US esta bem calibrado (nem corta nem
   *   funde interacoes). IA_MAX_US e so um teto de seguranca.
   *
   *   Tambem medimos massa_inicio (pico na fase de entrada, discrimina mao
   *   VAZIA vs CHEIA na entrada) e delta (pixels que mudaram entre a cena em
   *   repouso ANTES e DEPOIS do cacho = mudanca permanente de inventario,
   *   imune a velocidade do movimento) como DADOS CRUS p/ futura
   *   classificacao (entrou_vazio+delta->pegou; entrou_cheio+delta->deixou;
   *   sem delta->nada). Aqui NAO se classifica nada; so mede.
   * =================================================================== */
  static bool    ia_active   = false;  /* cacho em andamento? */
  static bool    ia_in_burst = false;  /* dentro de uma rajada de movimento? */
  static int64_t ia_t_first  = 0;      /* us: 1o movimento do cacho */
  static int64_t ia_t_last   = 0;      /* us: ultimo movimento visto */
  static long    ia_peak     = 0;      /* pico de massa de movimento no cacho */
  static int     ia_bursts   = 0;      /* nº de rajadas de movimento */
  static int64_t ia_max_gap  = 0;      /* maior pausa interna costurada (us) */
  /* DADOS CRUS p/ futura classificacao:
   *  - massa_inicio = pico de massa de movimento nos 1os IA_PHASE_US (fase de
   *    entrada). Discrimina mao VAZIA vs CHEIA na entrada (validado n=20).
   *  - delta = pixels que mudaram entre o baseline (repouso ANTES do cacho) e
   *    o frame atual (repouso DEPOIS). Mudanca permanente de inventario.
   * (massa_fim de saida foi REMOVIDA: provou-se redundante com a entrada.) */
  static long    ia_massa_inicio = 0;  /* pico de massa na fase de entrada */
  const  int64_t IA_END_GAP_US = 1500000;   /* 1,5 s sem movimento encerra o cacho */
  const  int64_t IA_MAX_US     = 60000000;  /* 60 s: teto de seguranca */
  const  int64_t IA_PHASE_US   = 600000;    /* 600 ms: janela de entrada/saida */

  bool    ia_present = (n > 0);         /* ha MOVIMENTO neste frame */
  int64_t ia_now     = esp_timer_get_time();

  if (!ia_active) {
    if (ia_present) {                   /* comeca um novo cacho */
      ia_active   = true;
      ia_in_burst = true;
      ia_t_first  = ia_now;
      ia_t_last   = ia_now;
      ia_peak     = g_motion_mass;
      ia_bursts   = 1;
      ia_max_gap  = 0;
      ia_massa_inicio = g_motion_mass;  /* abre a fase de entrada */
    }
  } else {
    if (ia_present) {                   /* movimento agora: estende o cacho */
      if (!ia_in_burst) {               /* retomou apos uma pausa: nova rajada */
        int64_t gap = ia_now - ia_t_last;
        if (gap > ia_max_gap) ia_max_gap = gap;
        ia_bursts++;
        ia_in_burst = true;
      }
      ia_t_last = ia_now;
      if (g_motion_mass > ia_peak) ia_peak = g_motion_mass;
      /* fase de ENTRADA: pico nos primeiros IA_PHASE_US do cacho */
      if ((ia_now - ia_t_first) < IA_PHASE_US &&
          g_motion_mass > ia_massa_inicio) ia_massa_inicio = g_motion_mass;
    } else {                            /* sem movimento agora */
      ia_in_burst = false;
      if ((ia_now - ia_t_last) >= IA_END_GAP_US) {   /* cena estatica: fecha */
        long ms        = (long)((ia_t_last - ia_t_first) / 1000);
        long maxgap_ms = (long)(ia_max_gap / 1000);
        /* delta = mudanca permanente da cena: pixels que diferem entre o
         * baseline (repouso ANTES do cacho) e o frame atual (repouso AGORA,
         * apos IA_END_GAP sem movimento). Roda 1x no fechamento, FORA das
         * janelas de WCET. delta=-1 => baseline ainda nao estava pronto. */
        long delta = -1;
        if (baseline_valid) {
          delta = 0;
          for (int i = 0; i < IMG_W * IMG_H; i++) {
            int d = (int)fb->buf[i] - (int)baseline_frame[i];
            if (d < 0) d = -d;
            if (d > DIFF_THRESH) delta++;
          }
        }
        Serial.printf("INTERACAO=%ld rajadas=%d maior_pausa=%ld "
                      "massa_inicio=%ld delta=%ld pico_massa=%ld\n",
                      ms, ia_bursts, maxgap_ms,
                      ia_massa_inicio, delta, ia_peak);
        ia_active = false;
      }
    }
    /* teto de seguranca: cacho longo demais (ex.: limpeza da geladeira) */
    if (ia_active && (ia_now - ia_t_first) > IA_MAX_US) {
      Serial.println("INTERACAO_DESCARTADA=excedeu_max");
      ia_active = false;
    }
  }
  /* =========== fim INSTRUMENTACAO DE COLETA — duracao da interacao ==== */

  /* manutencao do BASELINE de repouso: so atualiza quando a cena esta
   * GENUINAMENTE em repouso -> sem cacho ativo (!ia_active) E sem movimento
   * neste frame (!ia_present). Durante o cacho fica CONGELADO (cena pre-
   * interacao). Apos o fechamento NORMAL (frame ja sem movimento) re-baselina
   * p/ a nova cena. O !ia_present evita congelar uma cena COM a mao no caso de
   * descarte por IA_MAX (que dispara em pleno movimento). */
  if (!ia_active && !ia_present) {
    memcpy(baseline_frame, fb->buf, IMG_W * IMG_H);
    baseline_valid = true;
  }

  /* -- C_update(N): WCET do update inteiro, indexado pelo N do frame -- */
  int64_t t1 = esp_timer_get_time();
  argus_update(&argus, dets, n);
  uint32_t dt_update = (uint32_t)(esp_timer_get_time() - t1);
  /* N de referencia: o maior entre tracks antes e depois (captura o pico
   * de carga do frame, inclusive criacao/finalizacao de tracks) */
  int n_after = argus_active_tracks(&argus);
  int n_idx = n_after;  /* tracks ativos apos o update */
  if (n_idx > ARGUS_MAX_TRACKS) n_idx = ARGUS_MAX_TRACKS;
  if (dt_update > wc_update[n_idx]) wc_update[n_idx] = dt_update;

  /* -- Cfeedback: mede APENAS o custo de CPU de despachar o evento.
   * O Serial.println (transmissao serial) e o beep() (tom + delays de UI)
   * ficam FORA da janela cronometrada: o tempo fisico do buzzer eh tratado
   * como latencia de atuador, modelada separadamente, nao como custo de CPU. */
  argus_event_t e;
  while (1) {
    int64_t t2 = esp_timer_get_time();
    e = argus_poll_event(&argus);            /* o despacho do evento */
    uint32_t dt_fb = (uint32_t)(esp_timer_get_time() - t2);
    if (dt_fb > wc_feedback) wc_feedback = dt_fb;

    if (e == ARGUS_EVENT_NONE) break;        /* fila vazia: sai */

    /* atuacao (FORA da medicao de WCET) */
    if (e == ARGUS_EVENT_AMBIGUOUS) { Serial.println("AMBIGUOUS"); beep(2); }
    else if (e == ARGUS_EVENT_IN)   { Serial.println("IN");        beep(1); }
    else                            { Serial.println("OUT");       beep(1); }
  }

  esp_camera_fb_return(fb);

  /* relatorio de WCET a cada 5 s: ALIMENTE argus_wcet.c com isto */
  static int64_t last = 0;
  int64_t now = esp_timer_get_time();
  if (now - last > 5000000) {
    last = now;
    Serial.printf("=== WCET[us] ===  Cdetect=%u  Cfeedback=%u\n",
                  wc_detect, wc_feedback);
    Serial.print("  C_update(N) por N: ");
    for (int k = 0; k <= ARGUS_MAX_TRACKS; k++) {
      Serial.printf("[%d]=%u ", k, wc_update[k]);
    }
    Serial.println();
  }
}

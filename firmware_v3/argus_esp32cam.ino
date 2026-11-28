/* =====================================================================
 * argus_esp32cam.ino  --  Firmware ARGUS para ESP32-CAM (AI-Thinker) -- v3
 *
 * v3 (nov/2026): buzzer NAO bloqueante (argus_buzzer.c, verificado no
 * ESBMC com -DBEEP_NB) e medicao do periodo do quadro (Tloop, Tfb).
 * Coloque argus_fsm.* e argus_buzzer.* na mesma pasta do .ino.
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
  #include "argus_buzzer.h"
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
static long g_motion_mass = 0;   /* total de pixels em movimento no ultimo frame */
static long g_peak_mass = 0;     /* pico de massa durante a passagem atual */
static bool g_in_pass   = false; /* true enquanto ha mao no quadro */
static bool    have_prev = false;
static argus_ctx_t argus;

/* --------- acumuladores de WCET (microssegundos) --------------- */
static uint32_t wc_detect = 0, wc_feedback = 0;
static uint32_t wc_classify = 0;   /* WCET do bloco duracao+classificacao (CPU) */
/* WCET de argus_update() indexado pelo N de tracks ATIVOS durante o frame.
 * Modelo: C_total(N) = C_detect + C_classify + C_update(N) + C_feedback.
 * C_update(N) engloba matching, tracking, classificacao e criacao de tracks
 * — a "funcao unica cuja carga depende de N" (nao tarefas independentes,
 * por isso Liu-Layland nao se aplica). */
static uint32_t wc_update[ARGUS_MAX_TRACKS + 1] = {0};

/* ------------------------ buzzer (v3, nao bloqueante) ------------ */
/* O loop() so AGENDA os pulsos (buzzer_push); um timer periodico de 1 ms
 * liga e desliga o tom conforme buzzer_level(). O laco nunca espera. */
#ifndef BEEP_ON_US
#define BEEP_ON_US      80000      /* tom de cada pulso (igual a v2) */
#endif
#ifndef BEEP_PERIOD_US
#define BEEP_PERIOD_US 180000      /* tom + silencio (igual a v2) */
#endif
#define BEEP_FREQ        2000
#define BEEP_LEDC_CH        2      /* canal LEDC fora dos usados pela camera */
static argus_buzzer_t g_bz;
static portMUX_TYPE   g_bz_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile int   g_bz_on = 0;

static void bz_write(int on) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWriteTone(BUZZER_PIN, on ? BEEP_FREQ : 0);
#else
  ledcWriteTone(BEEP_LEDC_CH, on ? BEEP_FREQ : 0);
#endif
}
static void bz_tick(void *arg) {          /* timer de 1 ms */
  (void)arg;
  portENTER_CRITICAL(&g_bz_mux);
  int lv = buzzer_level(&g_bz, (uint64_t)esp_timer_get_time());
  portEXIT_CRITICAL(&g_bz_mux);
  if (lv != g_bz_on) { bz_write(lv); g_bz_on = lv; }
}
static void bz_setup() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(BUZZER_PIN, BEEP_FREQ, 8);
#else
  ledcSetup(BEEP_LEDC_CH, BEEP_FREQ, 8);
  ledcAttachPin(BUZZER_PIN, BEEP_LEDC_CH);
#endif
  bz_write(0);
  buzzer_init(&g_bz, BEEP_ON_US, BEEP_PERIOD_US);
  const esp_timer_create_args_t a = { .callback = &bz_tick, .arg = NULL,
                                      .dispatch_method = ESP_TIMER_TASK,
                                      .name = "argus_bz" };
  esp_timer_handle_t h;
  esp_timer_create(&a, &h);
  esp_timer_start_periodic(h, 1000);     /* 1 ms */
}
static void beep(int n) {                /* agenda; nao espera */
  portENTER_CRITICAL(&g_bz_mux);
  buzzer_push(&g_bz, (uint32_t)n, (uint64_t)esp_timer_get_time());
  portEXIT_CRITICAL(&g_bz_mux);
}

/* ---------- periodo do quadro (para substituir a hipotese de 50 ms) ---- */
static int64_t  t_prev_loop = 0;
static uint32_t wc_loop = 0, wc_fb = 0;  /* maximos: periodo do laco, espera do fb */
static uint64_t sum_loop = 0, sum_fb = 0;
static uint32_t n_loop = 0;

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
  bz_setup();
  if (!camera_init()) { Serial.println("ERRO: camera"); while(1) delay(1000); }
  argus_init(&argus, IMG_W, IMG_H);
  Serial.println("ARGUS pronto. Mova objetos sob a camera.");
}

void loop() {
  int64_t t_fb0 = esp_timer_get_time();
  camera_fb_t *fb = esp_camera_fb_get();
  int64_t t_fb1 = esp_timer_get_time();
  if (!fb) return;
  if (t_prev_loop != 0) {                /* periodo entre quadros consecutivos */
    uint32_t dl = (uint32_t)(t_fb1 - t_prev_loop), df = (uint32_t)(t_fb1 - t_fb0);
    if (dl > wc_loop) wc_loop = dl;
    if (df > wc_fb) wc_fb = df;
    sum_loop += dl; sum_fb += df; n_loop++;
  }
  t_prev_loop = t_fb1;

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
   * DURACAO DA INTERACAO + CLASSIFICACAO DE ACAO (intencao de entrada)
   * -------------------------------------------------------------------
   * Mede a DURACAO de cada interacao (modelo "cacho de atividade") e
   * classifica a ACAO por INTENCAO DE ENTRADA. NAO mexe na FSM antiga.
   *
   * MODELO do cacho: deteccao por diff de frames => n>0 = MOVIMENTO agora;
   * n==0 = SEM movimento (mao parada OU cena vazia). Um cacho abre no 1o
   * movimento, costura pausas curtas, e FECHA apos IA_END_GAP_US sem
   * movimento. DURACAO = ultimo - primeiro movimento. IA_MAX_US e teto.
   *
   * CLASSIFICACAO: a camera so ve a FAIXA DE PASSAGEM (nunca o interior), e
   * a massa de movimento da SAIDA e dominada pelo VIGOR da retirada, nao
   * pelo conteudo da mao (3 metodos falharam: massa_fim, delta, massa_volta;
   * validado 5x/caso). O sinal CONFIAVEL e a massa na ENTRADA (massa_inicio
   * = pico nos 1os IA_PHASE_US): entrou VAZIO (< MASSA_LIMIAR_CHEIO) =
   * intencao de PEGAR; entrou CHEIO = intencao de DEIXAR. Limite aceito:
   * erra em "browsing" (entra vazio, olha, nao leva -> falso PEGOU).
   * =================================================================== */
  static bool    ia_active   = false;  /* cacho em andamento? */
  static bool    ia_in_burst = false;  /* dentro de uma rajada de movimento? */
  static int64_t ia_t_first  = 0;      /* us: 1o movimento do cacho */
  static int64_t ia_t_last   = 0;      /* us: ultimo movimento visto */
  static long    ia_peak     = 0;      /* pico de massa de movimento no cacho */
  static int     ia_bursts   = 0;      /* nº de rajadas de movimento */
  static int64_t ia_max_gap  = 0;      /* maior pausa interna costurada (us) */
  static long    ia_massa_inicio = 0;  /* pico de massa na fase de ENTRADA */
  const  int64_t IA_END_GAP_US = 1500000;   /* 1,5 s sem movimento encerra o cacho */
  const  int64_t IA_MAX_US     = 60000000;  /* 60 s: teto de seguranca */
  const  int64_t IA_PHASE_US   = 600000;    /* 600 ms: janela da fase de entrada */
  const  long    MASSA_LIMIAR_CHEIO = 7000; /* massa_inicio >= isto = mao CHEIA */

  /* -- C_classify: WCET (CPU) do bloco de duracao+classificacao. O Serial
   * fica FORA da janela (transmissao = E/S, nao CPU), igual ao Cfeedback. -- */
  int64_t t_cls = esp_timer_get_time();

  bool        ia_emit = false, ia_desc = false; /* houve saida neste frame? */
  long        em_ms = 0, em_maxgap = 0, em_massa = 0, em_pico = 0;
  int         em_rajadas = 0;
  const char* em_acao = "";

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
        /* ACAO por INTENCAO DE ENTRADA: massa na entrada discrimina mao vazia
         * (vai PEGAR) vs cheia (vai DEIXAR). Saida e irrecuperavel da camera. */
        em_ms      = (long)((ia_t_last - ia_t_first) / 1000);
        em_maxgap  = (long)(ia_max_gap / 1000);
        em_acao    = (ia_massa_inicio >= MASSA_LIMIAR_CHEIO) ? "DEIXOU" : "PEGOU";
        em_massa   = ia_massa_inicio;
        em_pico    = ia_peak;
        em_rajadas = ia_bursts;
        ia_emit    = true;
        ia_active  = false;
      }
    }
    /* teto de seguranca: cacho longo demais (ex.: limpeza da geladeira) */
    if (ia_active && (ia_now - ia_t_first) > IA_MAX_US) {
      ia_desc   = true;
      ia_active = false;
    }
  }

  uint32_t dt_cls = (uint32_t)(esp_timer_get_time() - t_cls);
  if (dt_cls > wc_classify) wc_classify = dt_cls;

  /* relatorio/atuacao FORA da janela C_classify (Serial = E/S, nao CPU) */
  if (ia_emit)
    Serial.printf("INTERACAO=%ld rajadas=%d maior_pausa=%ld massa_inicio=%ld "
                  "pico=%ld acao=%s\n",
                  em_ms, em_rajadas, em_maxgap, em_massa, em_pico, em_acao);
  if (ia_desc)
    Serial.println("INTERACAO_DESCARTADA=excedeu_max");
  /* =========== fim DURACAO + CLASSIFICACAO ==== */

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
    Serial.printf("=== WCET[us] ===  Cdetect=%u  Cclassify=%u  Cfeedback=%u\n",
                  wc_detect, wc_classify, wc_feedback);
    if (n_loop > 0)
      Serial.printf("=== QUADRO[us] === Tloop max=%u media=%u  Tfb max=%u media=%u  n=%u\n",
                    wc_loop, (uint32_t)(sum_loop / n_loop), wc_fb,
                    (uint32_t)(sum_fb / n_loop), n_loop);
    Serial.print("  C_update(N) por N: ");
    for (int k = 0; k <= ARGUS_MAX_TRACKS; k++) {
      Serial.printf("[%d]=%u ", k, wc_update[k]);
    }
    Serial.println();
  }
}

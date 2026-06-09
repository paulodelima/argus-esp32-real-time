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
static bool    have_prev = false;
static argus_ctx_t argus;

/* --------- acumuladores de WCET (microssegundos) --------------- */
static uint32_t wc_detect = 0, wc_track = 0, wc_classify = 0, wc_feedback = 0;
static uint32_t track_calls = 0;

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

  /* -- N*Ctrack + Cclassify (dentro de argus_update) -- */
  int n_before = argus_active_tracks(&argus);
  int64_t t1 = esp_timer_get_time();
  argus_update(&argus, dets, n);
  uint32_t dt_update = (uint32_t)(esp_timer_get_time() - t1);
  /* normaliza por objeto para estimar Ctrack (WCET por track) */
  int Nref = n_before > 0 ? n_before : 1;
  uint32_t per_track = dt_update / Nref;
  if (per_track > wc_track) wc_track = per_track;
  track_calls += Nref;

  /* -- Cfeedback -- */
  argus_event_t e;
  while ((e = argus_poll_event(&argus)) != ARGUS_EVENT_NONE) {
    int64_t t2 = esp_timer_get_time();
    if (e == ARGUS_EVENT_AMBIGUOUS) { Serial.println("AMBIGUOUS"); beep(2); }
    else if (e == ARGUS_EVENT_IN)   { Serial.println("IN");        beep(1); }
    else                            { Serial.println("OUT");       beep(1); }
    uint32_t dt_fb = (uint32_t)(esp_timer_get_time() - t2);
    if (dt_fb > wc_feedback) wc_feedback = dt_fb;
  }

  esp_camera_fb_return(fb);

  /* relatorio de WCET a cada 5 s: ALIMENTE argus_wcet.c com isto */
  static int64_t last = 0;
  int64_t now = esp_timer_get_time();
  if (now - last > 5000000) {
    last = now;
    Serial.printf("WCET[us] Cdetect=%u Ctrack=%u Cclassify(incl)=%u Cfeedback=%u  N=%d\n",
                  wc_detect, wc_track, wc_track /*classify embutido*/, wc_feedback,
                  argus_active_tracks(&argus));
  }
}

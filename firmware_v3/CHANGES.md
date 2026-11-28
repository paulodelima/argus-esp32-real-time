# firmware_v3 (nov/2026)

Mudanca pedida no feedback de 24/11: bip nao bloqueante.

- `argus_buzzer.c/.h` (novo, C puro): agenda de pulsos. `beep()` agora so agenda
  (`buzzer_push`) e um timer de 1 ms liga e desliga o tom (`buzzer_level`).
  O laco `loop()` nao espera mais 180 ms por pulso.
- O mesmo `argus_buzzer.c` e verificado no ESBMC (`-DBEEP_NB`, propriedades P6 e P9).
- `argus_esp32cam.ino` passa a medir o periodo do quadro (`Tloop`) e a espera
  do `esp_camera_fb_get` (`Tfb`), maximo e media, impressos a cada 5 s.
- `argus_fsm.c/.h`: iguais a v2.

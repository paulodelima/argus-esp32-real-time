"""
argus_pc.py -- Prototipo no PC com webcam (Dia 1-2 do sprint).

Detecta movimento por subtracao de fundo, extrai centroides e alimenta
a MESMA logica de argus_tracker.py. Serve para validar a regra IN/OUT
ANTES de portar para o ESP32 (onde iterar eh lento).

  pip install opencv-python numpy
  python argus_pc.py            # usa a webcam 0
  python argus_pc.py video.mp4  # usa um arquivo

Teclas: 'q' sai | 'r' reseta o fundo | espaco pausa.
A linha vertical branca eh o eixo de classificacao (porta | interior).
"""
import sys
import cv2
import numpy as np
from argus_tracker import Argus, Event

# --- parametros de visao (ajuste para a sua cena) ---
PROC_W, PROC_H = 320, 240   # resolucao de processamento (mantenha baixa)
DIFF_THRESH = 25            # limiar de diferenca de movimento (0-255)
MIN_AREA = 1500              # area minima de blob (pixels) para virar deteccao
BG_ALPHA = 0.02             # taxa de atualizacao do fundo (running average)


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else 0
    cap = cv2.VideoCapture(src)
    if not cap.isOpened():
        print("Nao consegui abrir a fonte de video:", src)
        return

    argus = Argus(PROC_W, PROC_H)
    bg = None
    paused = False

    while True:
        if not paused:
            ok, frame = cap.read()
            if not ok:
                break
            frame = cv2.resize(frame, (PROC_W, PROC_H))
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            gray = cv2.GaussianBlur(gray, (5, 5), 0)

            if bg is None:
                bg = gray.astype("float32")
            # subtracao de fundo por media movel
            cv2.accumulateWeighted(gray, bg, BG_ALPHA)
            diff = cv2.absdiff(gray, cv2.convertScaleAbs(bg))
            _, mask = cv2.threshold(diff, DIFF_THRESH, 255, cv2.THRESH_BINARY)
            mask = cv2.dilate(mask, None, iterations=2)

            # centroides dos blobs de movimento
            cnts, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL,
                                       cv2.CHAIN_APPROX_SIMPLE)
            dets = []
            for c in cnts:
                if cv2.contourArea(c) < MIN_AREA:
                    continue
                M = cv2.moments(c)
                if M["m00"] == 0:
                    continue
                cx = int(M["m10"] / M["m00"])
                cy = int(M["m01"] / M["m00"])
                dets.append((cx, cy))
                cv2.circle(frame, (cx, cy), 5, (0, 0, 255), -1)

            argus.update(dets)
            while (e := argus.poll_event()) != Event.NONE:
                if e == Event.IN:
                    print(">>> IN  (bip simples)")
                elif e == Event.OUT:
                    print(">>> OUT (bip simples)")
                else:
                    print(">>> AMBIGUOUS (bip duplo)")

            # HUD
            cv2.line(frame, (PROC_W // 2, 0), (PROC_W // 2, PROC_H),
                     (255, 255, 255), 1)
            cv2.putText(frame, f"N={argus.n_tracks} estado={argus.state.name}",
                        (8, 20), cv2.FONT_HERSHEY_SIMPLEX, 0.5,
                        (0, 255, 0), 1)
            cv2.putText(frame, "porta", (8, PROC_H - 8),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.4, (200, 200, 0), 1)
            cv2.putText(frame, "interior", (PROC_W - 70, PROC_H - 8),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.4, (200, 200, 0), 1)
            cv2.imshow("ARGUS PC", frame)

        k = cv2.waitKey(1) & 0xFF
        if k == ord("q"):
            break
        elif k == ord("r"):
            bg = None
            argus = Argus(PROC_W, PROC_H)
        elif k == ord(" "):
            paused = not paused

    cap.release()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()

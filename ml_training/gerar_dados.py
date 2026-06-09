"""
gerar_dados.py  --  Gera dados de treino SINTETICOS para o classificador de
trajetoria do ARGUS (Camada 2 de ML).

Por que isto existe: para treinar um modelo voce precisa de DADOS rotulados
(trajetorias + se cada uma foi IN, OUT ou AMBIGUOUS). Enquanto voce ainda nao
coletou trajetorias reais no ESP32, este script cria exemplos realistas para
voce ver o pipeline de treino funcionando de ponta a ponta.

DEPOIS, e so trocar este CSV pelo arquivo de trajetorias REAIS logadas pelo
firmware (mesmas colunas) e retreinar.

Saida: trajetorias.csv  (uma trajetoria por linha)
Colunas (features) + rotulo:
  start_x, last_x, desloc, travel, idade, vel_media, rotulo
  rotulo: 0=OUT, 1=IN, 2=AMBIGUOUS

  python3 gerar_dados.py
"""
import csv
import random

W = 320                 # largura do frame (igual ao firmware)
CENTER = W // 2
N_POR_CLASSE = 400      # exemplos por classe
random.seed(42)


def features(start_x, last_x, idade):
    desloc = last_x - start_x                 # com sinal: + entrando, - saindo
    travel = abs(desloc)                      # magnitude
    vel_media = travel / idade if idade else 0
    # normaliza posicoes para [0,1] (modelo fica robusto a resolucao)
    return [start_x / W, last_x / W, desloc / W, travel / W, idade, vel_media]


def gerar():
    linhas = []

    # IN: comeca na porta (esquerda) e termina no interior (direita), cruza eixo
    for _ in range(N_POR_CLASSE):
        start_x = random.randint(10, CENTER - 30)
        last_x = random.randint(CENTER + 30, W - 10)
        idade = random.randint(6, 30)
        linhas.append(features(start_x, last_x, idade) + [1])

    # OUT: interior -> porta
    for _ in range(N_POR_CLASSE):
        start_x = random.randint(CENTER + 30, W - 10)
        last_x = random.randint(10, CENTER - 30)
        idade = random.randint(6, 30)
        linhas.append(features(start_x, last_x, idade) + [0])

    # AMBIGUOUS: ou deslocamento pequeno (jitter) ou nao cruza o eixo
    for _ in range(N_POR_CLASSE):
        if random.random() < 0.5:
            # jitter: comeca e termina perto, deslocamento minusculo
            start_x = random.randint(20, W - 20)
            last_x = start_x + random.randint(-25, 25)
        else:
            # mexe bastante mas fica do mesmo lado do eixo (nao cruza)
            lado = random.choice(["esq", "dir"])
            if lado == "esq":
                start_x = random.randint(10, CENTER - 50)
                last_x = random.randint(10, CENTER - 10)
            else:
                start_x = random.randint(CENTER + 50, W - 10)
                last_x = random.randint(CENTER + 10, W - 10)
        last_x = max(0, min(W - 1, last_x))
        idade = random.randint(4, 30)
        linhas.append(features(start_x, last_x, idade) + [2])

    random.shuffle(linhas)
    with open("trajetorias.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["start_x", "last_x", "desloc", "travel", "idade",
                    "vel_media", "rotulo"])
        w.writerows(linhas)
    print(f"Gerado trajetorias.csv com {len(linhas)} exemplos "
          f"({N_POR_CLASSE} por classe).")


if __name__ == "__main__":
    gerar()

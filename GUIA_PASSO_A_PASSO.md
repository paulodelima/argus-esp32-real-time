# Guia passo a passo do ARGUS (para seguir sem complicação)

Este guia te leva do zero até o projeto rodando, em linguagem simples. Faça os
passos **na ordem**. Cada passo diz: o objetivo, o que digitar, o que você
deve ver, e o que fazer se der erro.

> Dica de ouro: faça tudo primeiro no **computador** (passos 1, 2 e 6) e só
> depois mexa no ESP32 (passos 3, 4, 5). É mais rápido de testar no PC.

---

## O que é este projeto, em uma frase

Uma câmera barata em cima da geladeira que vê um produto entrando ou saindo,
apita uma vez (sucesso) ou duas (falha), e tudo isso precisa acontecer em menos
de **0,8 segundo**. Sua pesquisa responde: **quantos produtos ao mesmo tempo
(Nmax)** o sistema consegue acompanhar sem estourar esse tempo.

## O que você precisa

**Programas no computador (gratuitos):**
- **Python 3** (já vem no Linux/Mac; no Windows, baixe em python.org).
- Um **compilador C** (`gcc`). No Windows, instale o "MinGW" ou use o WSL.
- **Arduino IDE** (só para os passos do ESP32).
- **ESBMC** (só para o passo 5) — baixe em github.com/esbmc/esbmc/releases.

**Hardware (só a partir do passo 3):**
- Placa **ESP32-CAM** (modelo AI-Thinker é o mais comum).
- Um **adaptador FTDI / USB-TTL** para gravar (a ESP32-CAM não tem USB).
- Um **buzzer** (apito) e alguns fios jumper.
- Uma **webcam** comum, para o passo 2.

---

## Passo 1 — Provar que o cérebro do sistema funciona (no PC)

**Objetivo:** rodar os testes do núcleo, sem precisar de nenhum hardware.

Abra o terminal, entre na pasta `tests` e digite:

```bash
gcc -I../firmware -o test_fsm test_fsm.c ../firmware/argus_fsm.c
./test_fsm
```

**O que você deve ver:** uma lista terminando em `8 passou, 0 falhou`.
Em seguida, teste a versão em Python:

```bash
python3 test_tracker.py
```

Deve aparecer `6 passou, 0 falhou`.

**Se der erro:** se disser que `gcc` não existe, falta o compilador C (instale
o MinGW no Windows, ou `sudo apt install gcc` no Linux). Se for o Python,
instale pelo python.org.

---

## Passo 2 — Ver funcionando ao vivo com a webcam

**Objetivo:** testar a regra "entrou/saiu" usando sua webcam, antes de mexer no
ESP32 (assim você acerta a lógica num lugar fácil de testar).

Na pasta `pc_prototype`, instale as bibliotecas e rode:

```bash
pip install opencv-python numpy
python3 argus_pc.py
```

**O que você deve ver:** uma janela com a imagem da webcam, uma linha branca no
meio (o "eixo" porta/interior) e, no canto, `N=` (objetos vistos) e o estado.
Passe a mão da esquerda para a direita cruzando a linha: aparece `>>> IN` no
terminal. Da direita para a esquerda: `>>> OUT`. Mexidinha pequena: `AMBIGUOUS`.

Teclas: **q** sai, **r** reinicia o fundo, **espaço** pausa.

**Se der erro:** se a janela não abrir, confira se a webcam está livre (feche
Zoom/Meet). Se o movimento não for detectado, ajuste `DIFF_THRESH` (sensibilidade)
no topo do arquivo.

---

## Passo 3 — Gravar o programa no ESP32-CAM

**Objetivo:** colocar o firmware na placa de verdade.

1. Copie os arquivos `argus_fsm.h` e `argus_fsm.c` para **dentro da mesma pasta**
   do `argus_esp32cam.ino`.
2. Abra o `argus_esp32cam.ino` na Arduino IDE.
3. Em **Ferramentas → Placa**, escolha **AI Thinker ESP32-CAM**. Em **PSRAM**,
   marque **Enabled**.
4. Ligue a ESP32-CAM no FTDI. Para gravar, conecte **GPIO0 ao GND** e dê reset.
5. Ligue o buzzer no **GPIO 13**.
6. Clique em **Upload**. Quando terminar, tire o GPIO0 do GND e reset.

**O que você deve ver:** abrindo o **Monitor Serial** (115200), aparece
`ARGUS pronto`. Mova algo sob a câmera e ouça o apito.

**Se der erro de upload:** quase sempre é o GPIO0 que não está no GND na hora de
gravar, ou o cabo/FTDI. Confira a fiação e tente de novo.

---

## Passo 4 — Medir os tempos (os números da sua pesquisa)

**Objetivo:** descobrir quanto tempo cada etapa demora no hardware real.

Com o firmware rodando (passo 3), deixe o Monitor Serial aberto. A cada 5
segundos aparece uma linha tipo:

```
WCET[us] Cdetect=... Ctrack=... Cclassify=... Cfeedback=... N=...
```

Faça isso em três cenários, movendo objetos: **cena parada**, **movimento
moderado** e **muito movimento**. Anote os **maiores** valores que aparecerem —
esses são as suas estimativas de pior caso (WCET).

---

## Passo 5 — Descobrir o Nmax (a resposta final)

**Objetivo:** usar os tempos medidos para calcular, de forma certificada, quantos
objetos o sistema aguenta.

1. Abra `verification/argus_wcet.c` e troque os números `#define C_DETECT ...`
   (etc.) pelos **valores que você anotou no passo 4** (em microssegundos).
2. Rode o verificador:

```bash
esbmc argus_wcet.c --unwind 32 --no-unwinding-assertions
```

**O que isso faz:** o ESBMC procura o menor número de objetos que **estoura** o
limite de 0,8 s. Esse número **menos 1** é o seu **Nmax**. (O arquivo também
traz a conta no papel para você conferir.)

**Validação no mundo real:** no ESP32, coloque 1, 2, 3... objetos ao mesmo tempo
e veja a partir de quantos começa a sair o **apito duplo** (falha). Deve bater
com o Nmax calculado.

---

## Passo 6 (OPCIONAL) — Treinar o modelo de machine learning

> **Importante:** isto é um EXTRA. O sistema já funciona sem ML (a regra
> "entrou/saiu" é geométrica, de propósito, para o tempo ser previsível). Só faça
> este passo se quiser o diferencial — e ele foi feito para **não atrapalhar** a
> medição de tempo, porque o modelo gerado é leve e de custo fixo.

A ideia: em vez de uma regra fixa, o computador **aprende** a decidir entrada/saída
a partir de exemplos de trajetória. Na pasta `ml_training`:

```bash
pip install scikit-learn numpy
python3 gerar_dados.py        # cria exemplos de treino (trajetorias.csv)
python3 treinar_modelo.py     # treina e gera argus_modelo.h / argus_modelo.c
```

**O que você deve ver:** a acurácia do modelo (ex.: ~99%) e uma "matriz de
confusão". Ao final, ele cria `argus_modelo.h` e `argus_modelo.c`.

**Para usar no ESP32:** copie esses dois arquivos para a pasta `firmware/` e
troque, em `argus_fsm.c`, a chamada da função `classify(...)` por
`argus_ml_classify(...)`, passando as features da trajetória.

**Usando dados REAIS:** o `gerar_dados.py` cria exemplos "de mentira" só para o
treino rodar. Para valer, faça o firmware **registrar as trajetórias reais** (com
o rótulo certo que você anota) num arquivo com as mesmas colunas, e treine com
ele no lugar do CSV sintético.

---

## Dicionário rápido (sem enrolação)

- **WCET** = tempo de pior caso. O máximo que uma etapa pode demorar.
- **Nmax** = o maior número de objetos simultâneos que ainda cabe no prazo de 0,8 s.
- **ESBMC** = um programa que "prova" matematicamente se o seu código respeita o
  prazo, em vez de só testar na sorte.
- **Centroide** = o "ponto central" de um objeto que se move, que o sistema segue.
- **Firmware** = o programa que roda dentro da plaquinha (ESP32).

---

## Resumo do caminho mais curto até 14/jun

Passos **1 e 2** hoje (PC). Passo **3** quando o hardware chegar. Passo **4**
mede os tempos. Passo **5** dá o Nmax. Passo **6** é opcional. Anotou tudo? Está
pronto para escrever o relatório.

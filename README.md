# ARGUS — código base para validação (sprint até 14/jun)

Detector embarcado de eventos IN/OUT numa geladeira, com análise de WCET.
Este pacote te dá o **núcleo fiel à proposta** (visão computacional clássica,
verificável no ESBMC) pronto para você validar e medir. A parte de
*machine learning* está explicada na seção final — e propositalmente **fora**
do caminho crítico de tempo real, pelos motivos abaixo.

---

## Mapa dos arquivos

```
firmware/
  argus_fsm.h          núcleo da máquina de estados (independente de HW)
  argus_fsm.c          tracking + classificação IN/OUT/AMBIGUOUS
  argus_esp32cam.ino   firmware ESP32-CAM: câmera + buzzer + MEDIÇÃO de WCET
pc_prototype/
  argus_tracker.py     mesma lógica em Python puro (sem OpenCV)
  argus_pc.py          loop de captura com a webcam (OpenCV)
verification/
  argus_wcet.c         harness ESBMC: deriva o Nmax certificável
tests/
  test_fsm.c           testes unitários do núcleo C   (8 casos)
  test_tracker.py      testes da porta Python          (6 casos)
```

O `argus_fsm.*` é a **espinha**: o mesmo arquivo roda no ESP32, é exercitado
pelos testes e é verificado pelo ESBMC. A camada de visão (pixels → centroides)
fica fora dele de propósito — é isso que torna a lógica testável e formalmente
verificável.

---

## O que você valida AGORA (no PC, sem o ESP32)

```bash
# 1. núcleo em C — deve imprimir "8 passou, 0 falhou"
cd tests
gcc -Wall -I../firmware -o test_fsm test_fsm.c ../firmware/argus_fsm.c && ./test_fsm

# 2. porta Python — deve imprimir "6 passou, 0 falhou"
python3 test_tracker.py

# 3. prototipar a visão com a webcam (valida a regra IN/OUT na sua cena)
pip install opencv-python numpy
cd ../pc_prototype && python3 argus_pc.py

# 4. harness de verificação — compila como C normal p/ sanity check
cd ../verification && gcc -Wall -o /tmp/w argus_wcet.c && /tmp/w
```

(Os passos 1, 2 e 4 já foram rodados e passam. O passo 3 precisa da sua webcam.)

## O que roda no hardware (ESP32-CAM)

1. Copie `argus_fsm.h` e `argus_fsm.c` para a mesma pasta do `.ino`.
2. Arduino IDE → placa **AI Thinker ESP32-CAM**, PSRAM **Enabled**.
3. Buzzer no GPIO 13. Grave via FTDI com **GPIO0 → GND** no boot.
4. Abra o Serial Monitor (115200). A cada 5 s ele imprime os **WCET medidos** —
   são esses números (em µs) que você joga em `argus_wcet.c`.

## Derivar o Nmax (ESBMC)

Instale o ESBMC (binário em https://github.com/esbmc/esbmc/releases — não há
pacote pip). Depois:

```bash
esbmc argus_wcet.c --unwind 32 --no-unwinding-assertions
```

O ESBMC procura o menor `N` que **viola** `Ctotal(N) ≤ 800 ms`. Esse `N` menos 1
é o seu **Nmax certificado**. Com os números *placeholder* do arquivo o cálculo
analítico dá Nmax = 6; com os WCET reais do ESP32 ele muda — e essa diferença
**é o resultado da sua pesquisa**.

---

## Mapeamento para o cronograma de 7 dias

| Dia | Entrega | Arquivos |
|-----|---------|----------|
| 1-2 | lógica IN/OUT validada na webcam | `argus_pc.py`, `argus_tracker.py`, testes |
| 2   | máquina de estados em ANSI-C      | `argus_fsm.*`, `test_fsm.c` |
| 3   | porte para o ESP32-CAM            | `argus_esp32cam.ino` |
| 4   | medir Cdetect/Ctrack/Cclassify/Cfeedback | Serial Monitor do `.ino` |
| 5   | anotar e verificar               | `argus_wcet.c` + ESBMC |
| 6   | derivar e validar Nmax           | ESBMC vs. bip duplo real |
| 7   | relatório IEEE                   | — |

---

## Onde entra o machine learning (e como treinar)

Ponto central, para você não se perder: **a decisão IN/OUT do ARGUS é
geométrica, não é ML — e isso é de propósito.** É justamente esse pipeline de
custo previsível que a sua análise de WCET precisa medir. Se você trocar a regra
por uma rede neural, você muda o `Ctotal(N)` e portanto o `Nmax` — o que é
literalmente a *linha futura* de pesquisa citada na sua proposta, não o trabalho
de agora. Então: para a entrega de 14/jun, **não treine nada no caminho crítico**.
O ML entra em três lugares legítimos, do mais fácil ao mais avançado.

### Camada 1 — Classificador de produto na nuvem (o "treinar modelo" clássico)

É o que a proposta chama de "classificador por aprendizado de máquina" e marca
como **fora de escopo** (roda no backend, sob garantias de tempo diferentes).
Quando um evento IN dispara, o ESP recorta o frame e envia à nuvem, que responde
*"leite"*, *"ovos"* etc. Treinar isso é o fluxo padrão de visão:

1. **Dados**: colete fotos dos seus produtos (50-200 por classe), rotuladas por
   pasta (`dataset/leite/`, `dataset/ovos/`…).
2. **Transfer learning**: parta de uma rede pré-treinada (MobileNetV2/EfficientNet)
   e treine só a "cabeça" — poucos minutos de GPU, funciona com poucos dados.
3. **Avaliação**: separe ~20% para validação, olhe matriz de confusão.
4. **Deploy**: exporte e sirva no backend (não no ESP32).

Esqueleto (Keras):

```python
import tensorflow as tf
base = tf.keras.applications.MobileNetV2(include_top=False, weights="imagenet",
                                         input_shape=(96, 96, 3), pooling="avg")
base.trainable = False
model = tf.keras.Sequential([
    base,
    tf.keras.layers.Dropout(0.2),
    tf.keras.layers.Dense(n_classes, activation="softmax"),
])
model.compile("adam", "sparse_categorical_crossentropy", metrics=["accuracy"])
# train_ds/val_ds via tf.keras.utils.image_dataset_from_directory("dataset/")
model.fit(train_ds, validation_data=val_ds, epochs=10)
```

### Camada 2 — ML para a própria decisão IN/OUT (a ponte mais útil)

Em vez da regra geométrica, você pode **aprender** a decisão a partir de
*features da trajetória* que o seu firmware **já produz**: `start_x`, `last_x`,
deslocamento, número de frames, velocidade média, lado de entrada. Como são
poucos números, um modelo minúsculo (regressão logística, árvore, MLP de 1
camada) basta — e cabe no ESP32 sem TensorFlow. Fluxo:

1. **Logar**: faça o `.ino` imprimir, por trajetória finalizada, o vetor de
   features + o rótulo verdadeiro que você anota à mão (IN/OUT/AMBIGUOUS).
2. **Treinar no PC** com scikit-learn:

   ```python
   from sklearn.linear_model import LogisticRegression
   import numpy as np
   X = np.loadtxt("trajetorias.csv", delimiter=",")  # features
   y = np.loadtxt("rotulos.csv")                      # 0=OUT 1=IN 2=AMB
   clf = LogisticRegression(max_iter=1000).fit(X, y)
   print(clf.coef_, clf.intercept_)                   # pesos -> p/ o C
   ```

3. **Exportar os pesos** para um `inferencia.c` de poucas linhas (multiplicação
   + soma + argmax). O custo disso é determinístico e pequeno — então **continua
   compatível com a sua análise de WCET**, só com um `Cclassify` diferente.

Essa camada é a mais valiosa para o seu projeto porque conecta dados reais à
mesma pergunta de tempo real, sem quebrar a verificação.

### Camada 3 — Inferência neural embarcada (a sua pesquisa futura)

Rodar uma CNN no próprio ESP32 via **TensorFlow Lite Micro** ou **ESP-DL**.
Treina-se no PC (Camada 1), quantiza-se para int8 e roda-se no chip. O ponto de
pesquisa — e o motivo de isto ser *futuro* — é que a latência da inferência
neural vira um termo novo e pesado em `Ctotal(N)`, derrubando o `Nmax`. Medir e
**certificar** o WCET desse termo é exatamente a linha que a sua proposta
antecipa. Para 14/jun, deixe documentado como trabalho futuro; o arcabouço de
medição (o `.ino` instrumentado) e de verificação (`argus_wcet.c`) que você está
montando agora é precisamente a base experimental para atacá-la depois.

---

### Resumo da estratégia de ML

Para a entrega: **nenhum treino no caminho crítico** — use a regra geométrica
já pronta e meça o WCET dela. Se sobrar tempo e você quiser um diferencial,
a **Camada 2** (logar trajetórias → treinar um modelo minúsculo → exportar pesos
para C) é factível em um dia e não compromete a análise temporal. Camadas 1 e 3
ficam como evolução natural do produto e da pesquisa.

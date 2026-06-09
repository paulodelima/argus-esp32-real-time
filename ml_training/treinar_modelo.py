"""
treinar_modelo.py  --  Treina o classificador de trajetoria (Camada 2) e
exporta os pesos para C, pronto para rodar no ESP32.

O modelo e uma REGRESSAO LOGISTICA pequena: rapida, deterministica e de custo
previsivel -> nao quebra a sua analise de WCET. A "inferencia" no ESP32 e so
multiplicacao + soma + argmax (gerado em argus_modelo.c).

Entrada:  trajetorias.csv  (gere com gerar_dados.py, ou use dados reais do ESP32)
Saida:    argus_modelo.h, argus_modelo.c   (copie para a pasta do firmware)

  python3 treinar_modelo.py
"""
import csv
import numpy as np
from sklearn.linear_model import LogisticRegression
from sklearn.model_selection import train_test_split
from sklearn.metrics import accuracy_score, confusion_matrix

FEATURES = ["start_x", "last_x", "desloc", "travel", "idade", "vel_media"]
CLASSES = ["OUT", "IN", "AMBIGUOUS"]   # indice 0,1,2 (igual ao argus_event_t -1)


def carregar(path="trajetorias.csv"):
    X, y = [], []
    with open(path) as f:
        for row in csv.DictReader(f):
            X.append([float(row[c]) for c in FEATURES])
            y.append(int(row["rotulo"]))
    return np.array(X), np.array(y)


def exportar_c(clf):
    """Gera argus_modelo.h/.c com os pesos da regressao logistica."""
    W = clf.coef_            # shape (n_classes, n_features)
    b = clf.intercept_       # shape (n_classes,)
    nc, nf = W.shape

    # ---- header ----
    with open("argus_modelo.h", "w") as f:
        f.write(
"""/* argus_modelo.h -- classificador de trajetoria treinado (gerado por
 * treinar_modelo.py). Substitui a regra geometrica de classify() por um
 * modelo aprendido. Custo deterministico -> compativel com a analise de WCET.
 */
#ifndef ARGUS_MODELO_H
#define ARGUS_MODELO_H
/* features na MESMA ordem do treino:
 * [start_x/W, last_x/W, desloc/W, travel/W, idade, vel_media] */
""")
        f.write(f"#define ARGUS_ML_NFEAT {nf}\n")
        f.write(f"#define ARGUS_ML_NCLASS {nc}\n")
        f.write("/* retorna 0=OUT 1=IN 2=AMBIGUOUS */\n")
        f.write("int argus_ml_classify(const float feat[ARGUS_ML_NFEAT]);\n")
        f.write("#endif\n")

    # ---- source ----
    with open("argus_modelo.c", "w") as f:
        f.write('#include "argus_modelo.h"\n\n')
        f.write("static const float W[ARGUS_ML_NCLASS][ARGUS_ML_NFEAT] = {\n")
        for k in range(nc):
            vals = ", ".join(f"{w:.6f}f" for w in W[k])
            f.write(f"  {{ {vals} }},\n")
        f.write("};\n")
        b_vals = ", ".join(f"{x:.6f}f" for x in b)
        f.write(f"static const float B[ARGUS_ML_NCLASS] = {{ {b_vals} }};\n\n")
        f.write(
"""int argus_ml_classify(const float feat[ARGUS_ML_NFEAT]) {
    int best = 0; float best_score = -1e30f;
    for (int k = 0; k < ARGUS_ML_NCLASS; k++) {
        float s = B[k];
        for (int j = 0; j < ARGUS_ML_NFEAT; j++) s += W[k][j] * feat[j];
        if (s > best_score) { best_score = s; best = k; }
    }
    return best; /* argmax: nao precisa de softmax para decidir a classe */
}
""")
    print("Exportado: argus_modelo.h e argus_modelo.c")


def main():
    X, y = carregar()
    Xtr, Xte, ytr, yte = train_test_split(X, y, test_size=0.2,
                                          random_state=0, stratify=y)
    clf = LogisticRegression(max_iter=2000, C=1.0)
    clf.fit(Xtr, ytr)

    acc = accuracy_score(yte, clf.predict(Xte))
    print(f"Acuracia no teste: {acc*100:.1f}%")
    print("Matriz de confusao (linhas=verdadeiro, colunas=previsto):")
    print("        " + "  ".join(f"{c:>9}" for c in CLASSES))
    cm = confusion_matrix(yte, clf.predict(Xte), labels=[0, 1, 2])
    for i, c in enumerate(CLASSES):
        print(f"{c:>9} " + "  ".join(f"{v:>9}" for v in cm[i]))

    exportar_c(clf)
    print("\nPronto. Copie argus_modelo.h/.c para a pasta firmware/ e troque a\n"
          "chamada de classify() por argus_ml_classify() em argus_fsm.c.")


if __name__ == "__main__":
    main()

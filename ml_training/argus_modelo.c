#include "argus_modelo.h"

static const float W[ARGUS_ML_NCLASS][ARGUS_ML_NFEAT] = {
  { 1.047607f, -2.720645f, -3.768252f, 2.020553f, 0.027663f, 0.126596f },
  { -1.265822f, 2.465944f, 3.731766f, 1.457346f, 0.094981f, 0.190405f },
  { 0.218215f, 0.254701f, 0.036486f, -3.477899f, -0.122643f, -0.317000f },
};
static const float B[ARGUS_ML_NCLASS] = { -1.444173f, -4.332458f, 5.776631f };

int argus_ml_classify(const float feat[ARGUS_ML_NFEAT]) {
    int best = 0; float best_score = -1e30f;
    for (int k = 0; k < ARGUS_ML_NCLASS; k++) {
        float s = B[k];
        for (int j = 0; j < ARGUS_ML_NFEAT; j++) s += W[k][j] * feat[j];
        if (s > best_score) { best_score = s; best = k; }
    }
    return best; /* argmax: nao precisa de softmax para decidir a classe */
}

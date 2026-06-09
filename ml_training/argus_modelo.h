/* argus_modelo.h -- classificador de trajetoria treinado (gerado por
 * treinar_modelo.py). Substitui a regra geometrica de classify() por um
 * modelo aprendido. Custo deterministico -> compativel com a analise de WCET.
 */
#ifndef ARGUS_MODELO_H
#define ARGUS_MODELO_H
/* features na MESMA ordem do treino:
 * [start_x/W, last_x/W, desloc/W, travel/W, idade, vel_media] */
#define ARGUS_ML_NFEAT 6
#define ARGUS_ML_NCLASS 3
/* retorna 0=OUT 1=IN 2=AMBIGUOUS */
int argus_ml_classify(const float feat[ARGUS_ML_NFEAT]);
#endif

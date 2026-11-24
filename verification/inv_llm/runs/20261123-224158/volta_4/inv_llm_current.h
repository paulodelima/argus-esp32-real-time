static int inv_extra(const argus_ctx_t *c) {
    int i, j, same_age, same_missed, gap;

    /* coerencia estado <-> numero de tracks */
    if (c->state == ARGUS_IDLE && c->n_tracks != 0) return 0;
    if (c->state == ARGUS_TRACKING && c->n_tracks <= 0) return 0;

    for (i = 0; i < ARGUS_MAX_TRACKS; i++) {
        /* active e booleano: o firmware so escreve 0 (init/finalize) ou 1 (nascimento) */
        if (c->tracks[i].active != 0 && c->tracks[i].active != 1) return 0;
        if (!c->tracks[i].active) continue;

        /* missed so cresce junto com age e nasce 0 com age 1 */
        if (c->tracks[i].missed > c->tracks[i].age - 1) return 0;

        /* id foi tirado de next_id ha pouco: distancia ciclica (1..0x3FFFFFFF)
         * de id ate next_id esta em [1, MAX_DETS*age]; logo id != next_id */
        if (c->tracks[i].id < 1 || c->tracks[i].id > 0x3FFFFFFF) return 0;
        gap = c->next_id - c->tracks[i].id;
        if (gap <= 0) gap += 0x3FFFFFFF;
        if (gap < 1 || gap > MAX_DETS * c->tracks[i].age) return 0;

        /* no maximo MAX_DETS tracks nascem (ou casam) por frame */
        same_age = 0;
        same_missed = 0;
        for (j = 0; j < ARGUS_MAX_TRACKS; j++) {
            if (!c->tracks[j].active) continue;
            if (c->tracks[j].age == c->tracks[i].age) same_age++;
            if (c->tracks[j].missed == c->tracks[i].missed) same_missed++;
        }
        if (same_age > MAX_DETS) return 0;
        if (same_missed > MAX_DETS) return 0;
    }
    return 1;
}

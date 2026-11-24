static int inv_extra(const argus_ctx_t *c) {
    int i;
    for (i = 0; i < ARGUS_MAX_TRACKS; i++) {
        int diff;
        if (c->tracks[i].active != 0 && c->tracks[i].active != 1) return 0;
        if (!c->tracks[i].active) continue;
        if (c->tracks[i].id < 1 || c->tracks[i].id > 0x3FFFFFFF) return 0;
        /* distancia ciclica (no anel 1..0x3FFFFFFF) de id ate next_id */
        diff = c->next_id - c->tracks[i].id;
        if (diff <= 0) diff += 0x3FFFFFFF;
        if (diff < 1 || diff > c->tracks[i].age * MAX_DETS) return 0;
    }
    return 1;
}

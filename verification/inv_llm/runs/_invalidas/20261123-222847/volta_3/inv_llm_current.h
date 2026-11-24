static int inv_extra(const argus_ctx_t *c) {
    int i;
    for (i = 0; i < ARGUS_MAX_TRACKS; i++) {
        int dist;
        if (c->tracks[i].active != 0 && c->tracks[i].active != 1) return 0;
        if (!c->tracks[i].active) continue;
        if (c->tracks[i].missed >= c->tracks[i].age) return 0;
        if (c->tracks[i].id < 1 || c->tracks[i].id > 0x3FFFFFFF) return 0;
        /* ids emitidos depois deste track, contados no anel [1, 0x3FFFFFFF] */
        dist = c->next_id - c->tracks[i].id;
        if (dist <= 0) dist += 0x3FFFFFFF;
        if (dist < 1 || dist > c->tracks[i].age * MAX_DETS) return 0;
    }
    return 1;
}

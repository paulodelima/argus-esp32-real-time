static int inv_extra(const argus_ctx_t *c) {
    int i, j;
    for (i = 0; i < ARGUS_MAX_TRACKS; i++) {
        int a = c->tracks[i].active;
        int dd;
        if (a != 0 && a != 1) return 0;
        if (!a) continue;
        if (c->tracks[i].id < 1 || c->tracks[i].id > 0x3FFFFFFF) return 0;
        if (c->tracks[i].age < 1 || c->tracks[i].age > ARGUS_MAX_AGE) return 0;
        /* distancia ciclica (modulo 0x3FFFFFFF) de id ate next_id */
        dd = c->next_id - c->tracks[i].id;
        if (dd <= 0) dd += 0x3FFFFFFF;
        if (dd < 1 || dd > c->tracks[i].age * MAX_DETS) return 0;
        for (j = i + 1; j < ARGUS_MAX_TRACKS; j++)
            if (c->tracks[j].active && c->tracks[j].id == c->tracks[i].id)
                return 0;
    }
    return 1;
}

static int inv_extra(const argus_ctx_t *c) {
    int i, active = 0;
    for (i = 0; i < ARGUS_MAX_TRACKS; i++) {
        if (c->tracks[i].active) {
            active++;
            if (c->tracks[i].id < 1 || c->tracks[i].id > 0x3FFFFFFF) return 0;
            if (c->tracks[i].age < 1 || c->tracks[i].age > ARGUS_MAX_AGE) return 0;
            if (c->tracks[i].missed < 0 || c->tracks[i].missed > ARGUS_MAX_MISSED) return 0;
        }
    }
    if (active != c->n_tracks) return 0;
    return 1;
}

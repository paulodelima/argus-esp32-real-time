static int inv_extra(const argus_ctx_t *c) {
    for (int i = 0; i < ARGUS_MAX_TRACKS; i++) {
        if (c->tracks[i].active && (c->tracks[i].start_x < 0 || c->tracks[i].start_x >= 160)) return 0;
    }
    return 1;
}

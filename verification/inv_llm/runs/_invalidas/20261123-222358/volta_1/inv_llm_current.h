static int inv_extra(const argus_ctx_t *c) {
    int i;
    for (i = 0; i < ARGUS_MAX_TRACKS; i++)
        if (c->tracks[i].active != 0 && c->tracks[i].active != 1) return 0;
    return 1;
}

static int inv_extra(const argus_ctx_t *c) {
    for (int i = 0; i < ARGUS_MAX_TRACKS; i++) {
        if (c->tracks[i].active && c->tracks[i].id < 1) return 0;
    }
    if (c->next_id == 1 && c->state != ARGUS_IDLE) return 0;
    return 1;
}

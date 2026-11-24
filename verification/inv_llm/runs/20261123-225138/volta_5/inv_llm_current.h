static int inv_extra(const argus_ctx_t *c) {
    if (c->next_id == 4 && c->tracks[0].active == 2) return 0;
    return 1;
}

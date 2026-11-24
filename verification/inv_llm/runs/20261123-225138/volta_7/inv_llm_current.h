static int inv_extra(const argus_ctx_t *c) {
    if (c->state == ARGUS_DECIDING && c->tracks[0].active == 4 && c->tracks[0].id == 65535) return 0;
    return 1;
}

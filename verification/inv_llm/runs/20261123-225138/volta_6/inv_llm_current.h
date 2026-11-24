static int inv_extra(const argus_ctx_t *c) {
    if (c->next_id == 1024 && c->tracks[0].active == 1073741826) return 0;
    return 1;
}

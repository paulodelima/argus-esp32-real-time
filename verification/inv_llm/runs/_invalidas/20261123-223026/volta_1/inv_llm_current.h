static int inv_extra(const argus_ctx_t *c)
{
    int i, j;
    const int N = 0x3FFFFFFF;
    for (i = 0; i < ARGUS_MAX_TRACKS; i++) {
        int k;
        if (c->tracks[i].active != 0 && c->tracks[i].active != 1) return 0;
        if (!c->tracks[i].active) continue;
        if (c->tracks[i].id < 1 || c->tracks[i].id > N) return 0;
        /* ids emitidos desde a criacao deste track (ciclico em [1,N]) */
        k = c->next_id - c->tracks[i].id;
        if (k <= 0) k += N;
        if (k < 1 || k > c->tracks[i].age * MAX_DETS) return 0;
        for (j = i + 1; j < ARGUS_MAX_TRACKS; j++)
            if (c->tracks[j].active == 1 && c->tracks[j].id == c->tracks[i].id)
                return 0;
    }
    return 1;
}

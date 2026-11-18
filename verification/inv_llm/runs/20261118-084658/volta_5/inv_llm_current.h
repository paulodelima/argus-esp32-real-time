static int inv_extra(const argus_ctx_t *c) {
    int i, finalized = 0, polled = 0;
    for (i = 0; i < c->ev_count; i++) {
        if (c->ev_q[(c->ev_head + i) % ARGUS_MAX_EVENTS] != ARGUS_EVENT_NONE) {
            finalized++;
        }
    }
    while (c->ev_count > 0) {
        if (argus_poll_event((argus_ctx_t *)c) != ARGUS_EVENT_NONE) {
            polled++;
        }
    }
    return finalized == polled + c->ev_dropped;
}

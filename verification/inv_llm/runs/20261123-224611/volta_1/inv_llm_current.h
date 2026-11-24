static int inv_extra(const argus_ctx_t *c) {
    int i, finalized = 0, polled = 0;
    for (i = 0; i < c->ev_count; i++) {
        if (c->ev_q[(c->ev_head + i) % ARGUS_MAX_EVENTS] != ARGUS_EVENT_NONE) {
            finalized++;
        }
    }
    while (c->ev_head != c->ev_tail) {
        if (c->ev_q[c->ev_head] != ARGUS_EVENT_NONE) {
            polled++;
        }
        c->ev_head = (c->ev_head + 1) % ARGUS_MAX_EVENTS;
    }
    return (finalized == polled + c->ev_dropped);
}

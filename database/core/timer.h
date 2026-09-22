#include <time.h>

struct Timer {
    double start;
    size_t count;
};
// exmaple use
// String_Builder sb = {0};
// arena_reset(printArena, false);
// struct Timer t1 = {0};
// timer_start(&t1);
// visit_direct_children(printArena, &sb, &nodes, 1, visit_print, NULL);
// timer_mark(&t1);
// printf(
//     "callback: %.6f s total, %.6f us each\n",
//     timer_elapsed_sec(&t1),
//     1e6 * timer_elapsed_sec(&t1) / (double)t1.count);
// String_View sv = sb_to_sv(sb);
// printf(SV_Fmt "\n", SV_ARG(sv));

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void timer_start(struct Timer* t) {
    t->count = 0;
    t->start = now_sec();
}

static void timer_mark(struct Timer* t) {
    t->count++;
}

static double timer_elapsed_sec(struct Timer* t) {
    return now_sec() - t->start;
}

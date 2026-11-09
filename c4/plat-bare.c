// plat-bare.c -- curses with no operating system under it, or with C4DOS: the
// program draws on libjs's display itself. Under grin, C4DOS's graphical shell,
// the display is grin's, and grin publishes a table in C4DOS's slot 25 while it
// has it (c4's docs/grin-plan.md 3.6): the program then plays in a window of
// grin's. On a machine with no display at all (libjs/cli.js), pp_open says so
// and pdcc4.c makes it a scripted run.
//
// Built by c4lc without -conforming, as gui.c expects (c4/c4.mk).
#include <stdlib.h>
#include <unistd.h>
#include "gui.h"
#include "pdcplat.h"

struct grin_table {
    int magic, version, gen;
    void *env;
    void (*hello)(void);
    int  (*fwin_open)(char *title, int w, int h, int flags);
    void (*fwin_frame)(char *pixels, int w, int h, int pitch, int *pal);
    int  (*fwin_poll)(int *ev);
};
extern int *__c4dos_api;                // bare.c's: C4DOS's table, 0 on the bare machine

static struct grin_table *grin(void) {
    struct grin_table *t;
    if (!__c4dos_api || __c4dos_api[0] != 4404292 || __c4dos_api[5] < 4) return 0;
    t = (struct grin_table *)__c4dos_api[25];
    return t && t->magic == 1196575054 && t->version >= 2 ? t : 0;
}

enum { RING = 65536 };
static struct grin_table *g;            // grin's window, or 0: the display
static int *ring;
static int on;

int pp_open(const char *title) {
    if ((g = grin())) {
        if (g->fwin_open((char *)title, 640, 400, 0)) return 1;
        g = 0;
    }
    if (!gui_present()) return 0;
    if (!ring && !(ring = (int *)malloc(RING))) return 0;
    if (!gui_attach(ring, RING, 640, 400)) return 0;
    gui_events(GUI_M_KEYS | GUI_M_WINDOW);
    gui_cursor(GUI_PTR_NONE);
    gui_clear(0x000000);
    gui_show();
    on = 1;
    return 1;
}

void pp_close(void) {
    if (!on) return;
    gui_cursor(GUI_PTR_ARROW);
    gui_detach();
    on = 0;
}

int pp_ready(void) { return 1; }

void pp_frame(char *pixels, int *pal) {
    if (g) { g->fwin_frame(pixels, 640, 400, 640, pal); return; }
    gui_blit8(0, 0, 640, 400, pixels, 640, pal, 640, 400);
    gui_show();
}

int pp_poll(int *ev) {
    int e[4], k;
    if (g) {
        while ((k = g->fwin_poll(e)) > 0) {
            if (e[0] != GUI_EV_KEYDOWN) continue;
            ev[0] = e[1]; ev[1] = e[2]; ev[2] = e[3];
            return 1;
        }
        return k < 0 ? -1 : 0;
    }
    while (gui_poll(e)) {
        if (e[0] != GUI_EV_KEYDOWN) continue;
        ev[0] = e[1]; ev[1] = e[2]; ev[2] = e[3];
        return 1;
    }
    return 0;
}

void pp_sleep(int ms) { usleep(ms * 1000); }

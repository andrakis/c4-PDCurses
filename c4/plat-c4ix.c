// plat-c4ix.c -- curses in a window of C4IX's desktop. Run from a Terminal, the
// program's window becomes its canvas (window.h), because a Terminal's own
// input is edited a line at a time and a curses program wants each key. The
// frame goes as 8-bit pixels, and the window says when it has drawn them: the
// next frame is copied for it only then. That word comes in with the keys, and a
// game that draws and sleeps never asks for a key, so asking whether a frame can
// go takes in the events too, and keeps any keys for pp_poll.
//
// Built by c4lc without -conforming, as window.c expects (c4/c4.mk).
#include <stdlib.h>
#include <string.h>
#include "window.h"
#include "pdcplat.h"

enum { KEYS = 32 };

static char *shown;                     // what the window reads when it draws
static int *shown_pal;                  // from malloc: see pdcc4.c's pal
static int waiting;                     // a frame has gone and not been drawn
static int keys[KEYS * 3];              // keys that came in meanwhile
static int key_in, key_out;
static int gone;                        // stdin has ended: the window was closed

static void pump(void) {
    int ev[3], k;
    while (!gone) {
        k = win_poll(ev);
        if (k == WIN_FRAME) { waiting = 0; continue; }
        if (k == WIN_KEY) {
            if ((key_in + 1) % KEYS != key_out) {
                keys[key_in * 3] = ev[0];
                keys[key_in * 3 + 1] = ev[1];
                keys[key_in * 3 + 2] = ev[2];
                key_in = (key_in + 1) % KEYS;
            }
            continue;
        }
        if (k < 0) gone = 1;
        if (k <= 0) return;
    }
}

int pp_open(const char *title) {
    if (!shown) shown = (char *)malloc(640 * 400);
    if (!shown_pal) shown_pal = (int *)malloc(256 * sizeof(int));
    if (!shown || !shown_pal || !win_open((char *)title)) return 0;
    waiting = 0;
    return 1;
}

void pp_close(void) { win_close(); }

int pp_ready(void) {
    pump();
    return !waiting;
}

void pp_frame(char *pixels, int *pal) {
    memcpy(shown, pixels, 640 * 400);
    memcpy(shown_pal, pal, 256 * sizeof(int));
    win_fb(shown, 640, 400, 640, shown_pal);
    win_present();
    waiting = 1;
}

int pp_poll(int *ev) {
    pump();
    if (key_out != key_in) {
        ev[0] = keys[key_out * 3];
        ev[1] = keys[key_out * 3 + 1];
        ev[2] = keys[key_out * 3 + 2];
        key_out = (key_out + 1) % KEYS;
        return 1;
    }
    return gone ? -1 : 0;
}

void pp_sleep(int ms) { win_sleep(ms); }

/* plat-host.c -- gcc's build has no display: pdcc4.c runs it scripted. */
#include "pdcplat.h"
int  pp_open(const char *title) { return 0; }
void pp_close(void) { }
int  pp_ready(void) { return 1; }
void pp_frame(char *pixels, int *pal) { }
int  pp_poll(int *ev) { return 0; }
void pp_sleep(int ms) { }

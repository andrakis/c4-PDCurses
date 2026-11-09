/* pdcplat.h -- what pdcc4.c asks of the machine. One of plat-c4ix.c (a window
   in C4IX's desktop), plat-bare.c (libjs's display, or grin's window under
   C4DOS) or plat-host.c (nothing: gcc's build is always a scripted run). The
   frame is 640x400 8-bit pixels, a row every 640 bytes, through 256 colours. */
int  pp_open(const char *title);       /* 1: the display is ours. 0: there is none */
void pp_close(void);
int  pp_ready(void);                   /* may another frame go now? */
void pp_frame(char *pixels, int *pal);
int  pp_poll(int *ev);                 /* >0: a key went down, ev[] its keyCode,
                                          character and modifiers. 0: nothing yet.
                                          <0: the window has gone. */
void pp_sleep(int ms);

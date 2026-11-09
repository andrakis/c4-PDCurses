/* pdcc4.c -- PDCurses for C4's machines: the PDC_ functions docs/IMPLEMNT.md
   asks a platform for, in one file. Curses draws here into an 80x25 grid of
   8x16 cells, as 8-bit pixels in the font SDL's port uses, and pdcplat.h's
   functions put those pixels on whatever the machine has: a window in C4IX, or
   libjs's display on the bare machine and under C4DOS.

   A SCRIPTED RUN. Where there is no display, or with a file called pdcscrip in
   the current directory, there is nothing to look at and nobody at the keys.
   Keys are then read from stdin (the script: below) and each frame goes to
   stdout, as the rows it changed: their text and a checksum of their pixels.
   napms takes no time, and pdc4_time and pdc4_rand make the rest repeatable. A
   gcc build runs the same file, always scripted, so test-pdcurses and the BSD
   games' test can hold c4lc's build to gcc's, frame by frame.

   The script is the keys as typed, with a newline for Enter, and:
     `        one poll that finds no key, for a game that doesn't wait
     `N       N of them
     ~u ~d ~l ~r  the arrows         ~h ~e  Home and End
     ~p ~n    Page Up and Page Down  ~i ~x  Insert and Delete
     ~1 .. ~9 F1 to F9               ~^X ~!X  Ctrl+X and Alt+X
     ~~ ~`    those two characters   ~# ... the rest of the line is a comment
     ~.       the program ends here, as when the script runs out
     ~ and a newline                 nothing, to break a long line
   When it runs out the program ends, so a game that never quits can't hang. */

#include <curspriv.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "common/font437.h"
#include "common/acs437.h"
#include "pdcplat.h"

#define PC_COLS 80
#define PC_ROWS 25
#define PC_CW 8
#define PC_CH 16
#define PC_W (PC_COLS * PC_CW)
#define PC_H (PC_ROWS * PC_CH)

static unsigned char *fb;              /* the frame being drawn */
static unsigned char *glyphs;          /* 256 glyphs of 16 rows, a byte each */
static chtype *cells;                  /* what the frame shows */
static int *pal;                       /* 256 of 0x00RRGGBB: from malloc, because the display
                                          won't read one that isn't word-aligned, and a linked
                                          image's static array can land anywhere */
static int rowdirty[PC_ROWS];
static int dirty;                      /* drawn since the last frame went */
static int pal_dirty;
static int cur_y, cur_x;               /* the cursor's cell */
static int scripted;                   /* a scripted run: see above */
static int opened;                     /* the display is ours */
static int key_pending;                /* a key PDC_check_key has seen */
static int frames;

/* ---- the screen ------------------------------------------------------- */

/* The font is a 256x128 1-bit BMP, bottom row first: 32 glyphs across */
static void font_load(void) {
    unsigned char *bits;
    int c, y;
    bits = font437 + font437[10] + (font437[11] << 8);
    for (c = 0; c < 256; c++)
        for (y = 0; y < PC_CH; y++)
            glyphs[c * PC_CH + y] = bits[(127 - ((c / 32) * PC_CH + y)) * 32 + c % 32];
}

/* The sixteen in PDCurses's order (blue is 1, red is 4), then xterm's cube
   and greys for a game that asks for more */
static int vga[16] = { 0x000000, 0x0000aa, 0x00aa00, 0x00aaaa, 0xaa0000, 0xaa00aa, 0xaa5500, 0xc0c0c0,
                       0x555555, 0x5555ff, 0x55ff55, 0x55ffff, 0xff5555, 0xff55ff, 0xffff55, 0xffffff };

static int cube(int v) { return v ? 55 + 40 * v : 0; }

static void pal_load(void) {
    int i, v;
    for (i = 0; i < 16; i++) pal[i] = vga[i];
    for (; i < 232; i++)
        pal[i] = (cube((i - 16) / 36) << 16) | (cube(((i - 16) / 6) % 6) << 8) | cube((i - 16) % 6);
    for (; i < 256; i++) { v = 8 + 10 * (i - 232); pal[i] = (v << 16) | (v << 8) | v; }
}

static void cell_draw(int y, int x) {
    chtype ch;
    unsigned char *p, *g;
    unsigned int bits;
    int pair, fg, bg, i, row, under;

    ch = cells[y * PC_COLS + x];
    pair = PAIR_NUMBER(ch);
    fg = SP->atrtab[pair].f;
    bg = SP->atrtab[pair].b;
    if (fg < 0) fg = COLOR_WHITE;
    if (bg < 0) bg = COLOR_BLACK;
    if ((ch & A_BOLD) && fg < 8) fg = fg | 8;
    if ((ch & A_BLINK) && bg < 8) bg = bg | 8;
    if (ch & A_REVERSE) { i = fg; fg = bg; bg = i; }
    if ((ch & A_ALTCHARSET) && !(ch & 0xff80))
        ch = (ch & (A_ATTRIBUTES ^ A_ALTCHARSET)) | acs_map[ch & 0x7f];
    under = (ch & A_UNDERLINE) != 0;

    g = glyphs + (ch & 255) * PC_CH;
    p = fb + y * PC_CH * PC_W + x * PC_CW;
    for (row = 0; row < PC_CH; row++) {
        bits = g[row];
        if (under && row == 14) bits = 255;
        if ((ch & A_LEFT) != 0) bits = bits | 128;
        if ((ch & A_RIGHT) != 0) bits = bits | 1;
        /* the cursor: two rows at the foot of the cell, or all of it */
        if (y == cur_y && x == cur_x && SP->visibility) {
            if (SP->visibility == 2 || row == 13 || row == 14) bits = bits ^ 255;
        }
        for (i = 0; i < PC_CW; i++)
            p[i] = (unsigned char)((bits & (128 >> i)) ? fg : bg);
        p = p + PC_W;
    }
    rowdirty[y] = 1;
    dirty = 1;
}

/* A scripted frame: the rows it changed, as text and a checksum of pixels */
static void frame_script(void) {
    static char text[PC_COLS + 1];
    unsigned int h;
    unsigned char *p;
    int y, x, n, c;

    printf("frame %d cursor %d,%d,%d\n", frames, cur_y, cur_x, SP->visibility);
    for (y = 0; y < PC_ROWS; y++) {
        if (!rowdirty[y]) continue;
        rowdirty[y] = 0;
        n = 0;
        for (x = 0; x < PC_COLS; x++) {
            c = (int)(cells[y * PC_COLS + x] & 255);
            if (cells[y * PC_COLS + x] & A_ALTCHARSET) c = '+';
            text[x] = (char)(c >= 32 && c < 127 ? c : '.');
            if (c != ' ') n = x + 1;
        }
        text[n] = 0;
        /* FNV-1a over the row's pixels */
        h = 2166136261u;
        p = fb + y * PC_CH * PC_W;
        for (x = 0; x < PC_CH * PC_W; x++) h = (h ^ p[x]) * 16777619u;
        printf("%02d %08x %s\n", y, h, text);
    }
    fflush(stdout);
}

/* Send what has been drawn, if the display can take it. Scripted, always. */
static void frame_send(void) {
    if (!dirty && !pal_dirty) return;
    if (scripted) {
        frames++;
        frame_script();
    } else {
        if (!opened || !pp_ready()) return;
        frames++;
        pp_frame((char *)fb, pal);
    }
    dirty = 0;
    pal_dirty = 0;
}

void PDC_gotoyx(int y, int x) {
    int oy, ox;
    PDC_LOG(("PDC_gotoyx() - called: row %d col %d\n", y, x));
    oy = cur_y; ox = cur_x;
    cur_y = y; cur_x = x;
    if (oy != y || ox != x) cell_draw(oy, ox);
    cell_draw(y, x);
}

void PDC_transform_line(int lineno, int x, int len, const chtype *srcp) {
    int i;
    PDC_LOG(("PDC_transform_line() - called: line %d\n", lineno));
    for (i = 0; i < len; i++) {
        cells[lineno * PC_COLS + x + i] = srcp[i];
        cell_draw(lineno, x + i);
    }
}

void PDC_doupdate(void) { frame_send(); }

int PDC_get_columns(void) { return PC_COLS; }
int PDC_get_rows(void) { return PC_ROWS; }
int PDC_get_cursor_mode(void) { return 0; }

int PDC_curs_set(int visibility) {
    int ret;
    PDC_LOG(("PDC_curs_set() - called: visibility=%d\n", visibility));
    ret = SP->visibility;
    SP->visibility = visibility;
    cell_draw(cur_y, cur_x);
    return ret;
}

void PDC_set_title(const char *title) {
    if (scripted) { printf("title %s\n", title); fflush(stdout); }
}

int PDC_set_blink(bool blinkon) { return blinkon ? ERR : OK; }
int PDC_set_bold(bool boldon) { return boldon ? OK : ERR; }

/* ---- keys ------------------------------------------------------------- */

/* A key as the display reports it: a browser keyCode, the character, and the
   modifiers (2 is Ctrl). -1 for one curses doesn't want, such as Shift. */
static int key_of(int code, int ch, int mods) {
    if ((mods & 2) && code >= 65 && code <= 90) return code - 64;
    if ((mods & 4) && code >= 65 && code <= 90) return ALT_A + code - 65;
    switch (code) {
    case 8: case 9: case 13: case 27: return code;
    case 33: return KEY_PPAGE;
    case 34: return KEY_NPAGE;
    case 35: return KEY_END;
    case 36: return KEY_HOME;
    case 37: return KEY_LEFT;
    case 38: return KEY_UP;
    case 39: return KEY_RIGHT;
    case 40: return KEY_DOWN;
    case 45: return KEY_IC;
    case 46: return KEY_DC;
    }
    if (code >= 112 && code <= 123) return KEY_F(code - 111);
    if (ch > 0 && ch < 256) return ch;
    return -1;
}

static int script_ticks;

static void script_ended(void) {
    printf("the script ended\n");
    fflush(stdout);
    exit(0);
}

static int script_get(void) {
    int c;
    c = getchar();
    if (c == EOF) script_ended();
    return c;
}

/* The script's next key, or -1 for a poll that finds none */
static int script_key(void) {
    int c, n;
    if (script_ticks > 0) { script_ticks--; return -1; }
    for (;;) {
        c = script_get();
        if (c == '\n') return key_of(13, 13, 0);
        if (c == '`') {
            n = 0;
            while ((c = getchar()) >= '0' && c <= '9') n = n * 10 + c - '0';
            if (c != EOF) ungetc(c, stdin);
            script_ticks = n > 1 ? n - 1 : 0;
            return -1;
        }
        if (c != '~') return key_of(0, c, 0);
        c = script_get();
        switch (c) {
        case '\n': continue;
        case '#':
            while (script_get() != '\n') ;
            continue;
        case 'u': return KEY_UP;
        case 'd': return KEY_DOWN;
        case 'l': return KEY_LEFT;
        case 'r': return KEY_RIGHT;
        case 'h': return KEY_HOME;
        case 'e': return KEY_END;
        case 'p': return KEY_PPAGE;
        case 'n': return KEY_NPAGE;
        case 'i': return KEY_IC;
        case 'x': return KEY_DC;
        case '.': script_ended();
        case '^': return key_of(script_get() & ~32, 0, 2);
        case '!': return key_of(script_get() & ~32, 0, 4);
        case '~': case '`': return c;
        }
        if (c >= '1' && c <= '9') return KEY_F(c - '0');
    }
}

static int key_next = -1;

bool PDC_check_key(void) {
    int ev[3], r, k;
    if (key_pending) return TRUE;
    frame_send();
    if (scripted) {
        k = script_key();
        if (k < 0) return FALSE;
        key_next = k;
        key_pending = 1;
        return TRUE;
    }
    while ((r = pp_poll(ev)) > 0) {
        k = key_of(ev[0], ev[1], ev[2]);
        if (k < 0) continue;
        key_next = k;
        key_pending = 1;
        return TRUE;
    }
    /* the window was closed: the game ends, as when it is quit */
    if (r < 0) { PDC_scr_close(); exit(0); }
    return FALSE;
}

int PDC_get_key(void) {
    int k;
    if (!key_pending && !PDC_check_key()) return -1;
    k = key_next;
    key_pending = 0;
    SP->key_code = k >= KEY_MIN;
    return k;
}

void PDC_flushinp(void) {
    if (scripted) { key_pending = 0; return; }
    while (PDC_check_key()) key_pending = 0;
}

bool PDC_has_mouse(void) { return FALSE; }
int PDC_mouse_set(void) { return ERR; }
int PDC_modifiers_set(void) { return OK; }
void PDC_set_keyboard_binary(bool on) { }

/* ---- colours ---------------------------------------------------------- */

bool PDC_can_change_color(void) { return TRUE; }

int PDC_color_content(short color, short *red, short *green, short *blue) {
    int v;
    v = pal[color & 255];
    *red = (short)(((v >> 16) & 255) * 1000 / 255);
    *green = (short)(((v >> 8) & 255) * 1000 / 255);
    *blue = (short)((v & 255) * 1000 / 255);
    return OK;
}

int PDC_init_color(short color, short red, short green, short blue) {
    pal[color & 255] = ((red * 255 / 1000) << 16) | ((green * 255 / 1000) << 8) | (blue * 255 / 1000);
    pal_dirty = 1;
    return OK;
}

/* ---- the program's start and end -------------------------------------- */

static void display_open(void) {
    if (!scripted && !opened) opened = pp_open("curses");
    if (!opened) scripted = 1;
}

int PDC_scr_open(void) {
    FILE *f;
    PDC_LOG(("PDC_scr_open() - called\n"));
    SP = (SCREEN *)calloc(1, sizeof(SCREEN));
    fb = (unsigned char *)calloc(PC_W, PC_H);
    glyphs = (unsigned char *)malloc(256 * PC_CH);
    cells = (chtype *)calloc(PC_COLS * PC_ROWS, sizeof(chtype));
    pal = (int *)malloc(256 * sizeof(int));
    if (!SP || !fb || !glyphs || !cells || !pal) return ERR;
    f = fopen("pdcscrip", "r");
    if (f) { scripted = 1; fclose(f); }
    display_open();
    font_load();
    pal_load();
    SP->orig_attr = FALSE;
    SP->orig_fore = COLOR_WHITE;
    SP->orig_back = COLOR_BLACK;
    SP->mono = FALSE;
    SP->audible = FALSE;
    SP->lines = PC_ROWS;
    SP->cols = PC_COLS;
    SP->mouse_wait = PDC_CLICK_PERIOD;
    SP->termattrs = A_COLOR | A_REVERSE | A_UNDERLINE | A_LEFT | A_RIGHT;
    SP->visibility = 1;
    return OK;
}

void PDC_scr_close(void) {
    frame_send();
    if (opened) { pp_close(); opened = 0; }
}

void PDC_scr_free(void) {
    free(fb); free(glyphs); free(cells); free(pal);
    fb = 0; glyphs = 0; cells = 0; pal = 0;
    if (SP) free(SP);
    SP = 0;
}

int PDC_resize_screen(int nlines, int ncols) {
    return (nlines == PC_ROWS && ncols == PC_COLS) ? OK : ERR;
}

void PDC_reset_prog_mode(void) {
    int was;
    was = opened;
    display_open();
    if (!was && opened) { dirty = 1; frame_send(); }
}

void PDC_reset_shell_mode(void) { }
void PDC_restore_screen_mode(int i) { }
void PDC_save_screen_mode(int i) { }

/* ---- the rest --------------------------------------------------------- */

static int clock_ms;                   /* a scripted run's clock */

void PDC_beep(void) {
    if (scripted) { printf("beep\n"); fflush(stdout); }
}

/* Called as time passes, for a program with timers of its own (the BSD games) */
void (*pdc4_tick)(int ms);

void PDC_napms(int ms) {
    if (scripted) clock_ms = clock_ms + ms;
    else {
        frame_send();
        pp_sleep(ms);
    }
    if (pdc4_tick) pdc4_tick(ms);
}

const char *PDC_sysname(void) { return "C4"; }

/* For the demos, built with -Dtime=pdc4_time -Drand=pdc4_rand -Dsrand=pdc4_srand,
   and for a game's own clock in a scripted run */
int pdc4_scripted(void) { return scripted; }
int pdc4_ms(void) { return clock_ms; }

static unsigned int rstate = 2463534242u;

void pdc4_srand(unsigned int seed) {
    rstate = scripted ? 2463534242u : seed * 2654435761u + 2463534242u;
    if (!rstate) rstate = 2463534242u;
}

int pdc4_rand(void) {
    unsigned int x;
    x = rstate;
    x = x ^ (x << 13);
    x = x ^ (x >> 17);
    x = x ^ (x << 5);
    rstate = x;
    return (int)(x & 32767u);
}

time_t pdc4_time(time_t *t) {
    time_t v;
    if (!scripted) return time(t);
    v = (time_t)(1000000 + clock_ms / 1000);
    if (t) *t = v;
    return v;
}

/* ---- the clipboard: there is none to share, so one of its own --------- */

static char *clip;
static long cliplen;

int PDC_getclipboard(char **contents, long *length) {
    if (!clip) return PDC_CLIP_EMPTY;
    *contents = (char *)malloc(cliplen + 1);
    if (!*contents) return PDC_CLIP_MEMORY_ERROR;
    memcpy(*contents, clip, cliplen + 1);
    *length = cliplen;
    return PDC_CLIP_SUCCESS;
}

int PDC_setclipboard(const char *contents, long length) {
    PDC_clearclipboard();
    clip = (char *)malloc(length + 1);
    if (!clip) return PDC_CLIP_MEMORY_ERROR;
    memcpy(clip, contents, length);
    clip[length] = 0;
    cliplen = length;
    return PDC_CLIP_SUCCESS;
}

int PDC_freeclipboard(char *contents) {
    free(contents);
    return PDC_CLIP_SUCCESS;
}

int PDC_clearclipboard(void) {
    if (clip) free(clip);
    clip = 0;
    cliplen = 0;
    return PDC_CLIP_SUCCESS;
}

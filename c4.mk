# c4.mk -- PDCurses's part of c4's Makefile. c4 includes it from
# games/pdcurses/c4.mk, its submodule, and runs it from c4's root, with c4's
# tools and variables (games/README.md in c4). Without it, c4 builds everything
# else, the BSD games' curses games aren't built, and test-pdcurses says it was
# skipped.

# The library is upstream's units with c4/pdcc4.c, its platform for C4's
# machines: .pdcurses/lib/ has the same objects for C4IX, the bare machine and
# C4DOS, which a game links with --start-lib, so that it carries what it calls.
# plat-c4ix and plat-bare say where the pixels go. They include window.c and
# gui.c, which are built without -conforming everywhere.
PDC       := games/pdcurses
PDC_UNITS := addch addchstr addstr attr beep bkgd border clear color delch deleteln \
             getch getstr getyx inch inchstr initscr inopts insch insstr instr kernel \
             keyname mouse move outopts overlay pad panel printw refresh scanw \
             scr_dump scroll slk termattr touch util window
PDC_DEMOS := firework xmas testcurs ptest ozdemo tuidemo
PDC_HDRS  := $(wildcard $(PDC)/*.h $(PDC)/c4/*.h $(PDC)/common/*.h) \
             $(wildcard src/c4ix/libc/include/*.h src/c4ix/libc/include/sys/*.h)
PDC_CC    := ./c4sp32 -c 400000 src/c4sp/lisp/c4lc.lisp -O $(IMG_G) -mfuse -fno-builtin -c -conforming \
             -I $(PDC)/c4 -I $(PDC) -I src/c4ix/libc/include
PDC_PLATCC := ./c4sp32 -c 400000 src/c4sp/lisp/c4lc.lisp -O $(IMG_G) -mfuse -fno-builtin -c \
             -I $(PDC)/c4 -I src/c4ix/libc/include -I src/c4ix/include -I libjs/guest
# what makes the demos repeatable in a scripted run (pdcc4.c)
PDC_DEMODEFS := time=pdc4_time rand=pdc4_rand srand=pdc4_srand

pdcurses-objs: $(PDC_UNITS:%=.pdcurses/lib/%.c4o) .pdcurses/lib/pdcc4.c4o \
               .pdcurses/plat-c4ix.c4o .pdcurses/plat-bare.c4o \
               .pdcurses/demo/tui.c4o $(PDC_DEMOS:%=.pdcurses/demo/%.c4o)
.pdcurses/lib/%.c4o: $(PDC)/pdcurses/%.c $(PDC_HDRS) $(C4LC_LISP) .img_g | c4sp32
	@mkdir -p .pdcurses/lib
	$(PDC_CC) $< $@ > /dev/null
.pdcurses/lib/pdcc4.c4o: $(PDC)/c4/pdcc4.c $(PDC_HDRS) $(C4LC_LISP) .img_g | c4sp32
	@mkdir -p .pdcurses/lib
	$(PDC_CC) $< $@ > /dev/null
.pdcurses/plat-%.c4o: $(PDC)/c4/plat-%.c $(PDC_HDRS) $(C4LC_LISP) .img_g | c4sp32
	@mkdir -p .pdcurses
	$(PDC_PLATCC) $< $@ > /dev/null
.pdcurses/demo/%.c4o: $(PDC)/demos/%.c $(PDC_HDRS) $(C4LC_LISP) .img_g | c4sp32
	@mkdir -p .pdcurses/demo
	$(PDC_CC) -I $(PDC)/demos -I $(PDC)/c4/inc $(PDC_DEMODEFS:%=-D %) $< $@ > /dev/null
GAME_OBJS += pdcurses-objs
PHONY += pdcurses-objs

# ... and by gcc for the host, always a scripted run (c4/plat-host.c): the
# reference test-pdcurses holds them to. 32-bit, as c4 is.
PDC_HOST_CC := gcc -m32 -O1 -w -std=gnu99 -D_DEFAULT_SOURCE -I $(PDC)/c4 -I $(PDC)
.pdcurses/host/lib/%.o: $(PDC)/pdcurses/%.c $(PDC_HDRS)
	@mkdir -p .pdcurses/host/lib
	$(PDC_HOST_CC) -c -o $@ $<
.pdcurses/host/lib/%.o: $(PDC)/c4/%.c $(PDC_HDRS)
	@mkdir -p .pdcurses/host/lib
	$(PDC_HOST_CC) -c -o $@ $<
PDC_HOST_OBJS := $(PDC_UNITS:%=.pdcurses/host/lib/%.o) .pdcurses/host/lib/pdcc4.o .pdcurses/host/lib/plat-host.o
.pdcurses/host/%: $(PDC)/demos/%.c $(PDC_HOST_OBJS) $(PDC_HDRS)
	$(PDC_HOST_CC) -I $(PDC)/demos $(PDC_DEMODEFS:%=-D%) -o $@ $< $(if $(filter tuidemo,$*),$(PDC)/demos/tui.c) $(PDC_HOST_OBJS)
pdcurses-host: $(PDC_DEMOS:%=.pdcurses/host/%)
PHONY += pdcurses-host

# test-pdcurses: the demos with scripted keys, through c4/tests/run.sh: each
# frame's rows, as text and a checksum of their pixels, on gcc's build, the bare
# machine, C4DOS and C4IX. Then c4/tests/test-window.mjs: on a display, with no
# script, in C4IX's desktop and on the bare machine, each screen as gcc drew it.
test-pdcurses: pdcurses-host c4bb-images
	sh $(PDC)/c4/tests/run.sh $(PDC)/c4/tests .pdcurses/host .pdc_t $(PDC_DEMOS)
	node $(PDC)/c4/tests/test-window.mjs
GAME_TESTS += test-pdcurses
PHONY += test-pdcurses

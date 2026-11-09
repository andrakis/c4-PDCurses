# c4-images.sh c4ix|bare -- PDCurses's part of c4's build-images.sh, which sources
# it from c4's root (games/README.md in c4). The library goes into each program
# that uses it; what comes to the disks of its own is upstream's demos.
PDC_DEMO_LIST="firework xmas testcurs ptest ozdemo tuidemo"
pdc_objs () { echo .pdcurses/demo/$1.c4o; if [ $1 = tuidemo ]; then echo .pdcurses/demo/tui.c4o; fi; }
case "$1" in
c4ix)
    if [ -f .pdcurses/plat-c4ix.c4o ] && [ -f $PB/lib/libc.c4s ]; then
        say "linking PDCurses's demos for C4IX"
        for d in $PDC_DEMO_LIST; do
            ./c4rlink32 $(pdc_objs $d) .pdcurses/plat-c4ix.c4o --start-lib .pdcurses/lib/*.c4o -l $PB/lib/libc.c4s -o $DISK/c4ix-pdcurses-$d.c4r > /dev/null
        done
    fi ;;
bare)
    if [ -f .pdcurses/plat-bare.c4o ]; then
        for d in $PDC_DEMO_LIST; do
            item $d
            ./c4rlink32 $(pdc_objs $d) .pdcurses/plat-bare.c4o --start-lib .pdcurses/lib/*.c4o .libc-bare/libc-*.c4o -o $GAMESDISK/$d.c4r > /dev/null
        done
        GAMES_ON_DISK="$GAMES_ON_DISK FIREWORK XMAS TESTCURS"
    fi ;;
esac

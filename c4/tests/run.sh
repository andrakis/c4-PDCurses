#!/bin/sh
# run.sh TESTS HOSTDIR TMP NAME... -- hold each program, built by c4lc, to its
# gcc build, in HOSTDIR. From c4's root. TESTS/NAME.in is what it reads, and
# TESTS/NAME.args its arguments if there are any; TESTS/data.sh DIR puts what
# they need in DIR, the directory each one runs in. Every run has the files
# bsdseed and pdcscrip there, which make it repeatable and, for a curses
# program, scripted (c4/c4bsd.h, and pdcc4.c in games/pdcurses).
#
# What gcc's build prints must be what each of these prints, line for line:
# the bare machine (each program from its own boot), C4DOS (the same) and C4IX
# (all of them in one boot, from the shell: a program that only ends at the end of
# its input has TESTS/NAME.c4ix, the shell's lines to run it). C4's consoles don't echo, and these
# programs don't look at isatty, so nothing but the system's own words differs,
# and same.py finds where the program's start. A program on C4DOS's disk has a
# name of 8 characters at most.
TESTS=$1; HOSTDIR=$2; T=$3; shift 3
HERE=$(dirname "$0")
ROOT=$(pwd)
CLI="node $ROOT/libjs/cli.js -m 64"
IMG=$ROOT/src/c4bb/images
rm -rf "$T"
mkdir -p "$T/host" "$T/bare" "$T/dos" "$T/out"

# TESTS/scores: files a game opens to keep scores. The bare machine's and C4DOS's
# disks here are read-only, and a game does different things when it can't write
# one, so nobody can: each is a directory for gcc's build and in C4IX.
scorefiles() { if [ -f "$TESTS/scores" ]; then cat "$TESTS/scores"; fi; }
marks() {
    echo 1000000000 > "$1/bsdseed"
    : > "$1/pdcscrip"
    for f in $(scorefiles); do mkdir "$1/$f"; done
}
args() { if [ -f "$TESTS/$1.args" ]; then cat "$TESTS/$1.args"; fi; }
short() { echo "$1" | cut -c1-8; }
same() {  # same NAME WHERE GOT
    if ! python3 -I "$HERE/same.py" "$T/out/$1.gcc" "$3"; then
        echo "$1 on $2 differs from gcc's (full output: $3)"
        exit 1
    fi
}

# gcc's
marks "$T/host"
sh "$TESTS/data.sh" "$T/host"
for g in "$@"; do
    ( cd "$T/host" && TZ=UTC timeout 120 "$ROOT/$HOSTDIR/$g" $(cd "$ROOT" && args "$g") ) \
        < "$TESTS/$g.in" > "$T/out/$g.gcc" 2> /dev/null
    r=$?
    if [ $r -ge 124 ]; then echo "$g: gcc's build timed out or crashed ($r)"; exit 1; fi
    echo "$g: gcc's build prints $(wc -l < "$T/out/$g.gcc") lines"
done

# the bare machine
marks "$T/bare"
sh "$TESTS/data.sh" "$T/bare"
for g in "$@"; do
    s=$(short "$g")
    cp "$IMG/games/$s.c4r" "$T/bare/"
    ( cd "$T/bare" && timeout 600 $CLI -d . "$s.c4r" $(cd "$ROOT" && args "$g") ) \
        < "$TESTS/$g.in" > "$T/out/$g.bare" 2> /dev/null
    same "$g" "the bare machine" "$T/out/$g.bare"
    echo "$g: the bare machine's is the same"
done

# C4DOS, on the games disk with the two files added
cp -rs "$IMG/games/." "$T/dos/"
marks "$T/dos"
rm -f "$T/dos/c4dos.dir"
( cd "$T/dos" && ls -p | grep -v '/$' | grep -v '^manifest.json$' > c4dos.dir )
for g in "$@"; do
    s=$(short "$g")
    { echo "$s $(args "$g")"; cat "$TESTS/$g.in"; echo; echo ver; } \
        | ( cd "$T/dos" && timeout 900 $CLI -d . "$IMG/c4dos32.c4r" ) > "$T/out/$g.dos" 2> /dev/null
    same "$g" "C4DOS" "$T/out/$g.dos"
    echo "$g: C4DOS's is the same"
done

# C4IX: one boot, a marker before each
{
    echo "cd /tmp"
    echo "echo 1000000000 > bsdseed"
    echo "echo x > pdcscrip"
    for f in $(scorefiles); do echo "mkdir $f"; done
    for g in "$@"; do
        echo "echo ==BEGIN $g=="
        if [ -f "$TESTS/$g.c4ix" ]; then
            cat "$TESTS/$g.c4ix"
        else
            echo "$g $(args "$g")"
            cat "$TESTS/$g.in"
            echo
        fi
    done
    echo "echo ==END=="
    echo exit
} | timeout 1800 $CLI -d "$IMG/disk" "$IMG/c4ix32.c4r" > "$T/out/all.c4ix" 2> /dev/null
for g in "$@"; do
    tr -d '\r' < "$T/out/all.c4ix" | awk -v m="==BEGIN $g==" 'p && /==BEGIN |==END==/ { exit } p; index($0, m) { p = 1 }' > "$T/out/$g.c4ix"
    same "$g" "C4IX" "$T/out/$g.c4ix"
    echo "$g: C4IX's is the same"
done
rm -rf "$T"

#!/usr/bin/env python3
# same.py WANT GOT -- is GOT's text WANT's, wherever it starts? GOT has the
# system's own words around it: a prompt can share the first line with the
# program's first words, and the last line, if it has no newline, with what
# comes after. Line for line otherwise. Says where they part, and exits 1.
import sys

def lines(path):
    return open(path, 'rb').read().replace(b'\r', b'').decode('latin-1').split('\n')

want = lines(sys.argv[1])
got = lines(sys.argv[2])
while want and not want[-1]:
    want.pop()
while want and not want[0]:
    want.pop(0)
if not want:
    print('nothing to compare: the reference printed nothing')
    sys.exit(1)
start = next((i for i, l in enumerate(got) if l.endswith(want[0])), None)
if start is None:
    print('first line never appears: %r' % want[0][:100])
    print('it starts: %r' % got[:6])
    sys.exit(1)
for i, w in enumerate(want):
    g = got[start + i] if start + i < len(got) else None
    ok = g is not None and (g.endswith(w) if i == 0 else g.startswith(w) if i == len(want) - 1 else g == w)
    if not ok:
        print('line %d of %d' % (i + 1, len(want)))
        print('  want %r' % w[:160])
        print('  got  %r' % (g[:160] if g is not None else None))
        sys.exit(1)

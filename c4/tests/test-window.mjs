// test-window.mjs - curses on a real display, headless: testcurs's menu in a window
// of C4IX's desktop, and on the bare machine's display. run.sh's runs are scripted,
// so they never reach c4/plat-c4ix.c or c4/plat-bare.c; this does, with key events
// from the device as the browser sends them. Each screen's pixels must be the ones
// gcc's scripted build drew, row by row. Then Q ends it, and in C4IX the window is
// a terminal again.
//
//   node games/pdcurses/c4/tests/test-window.mjs          (from c4's root)

import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { execFileSync } from 'node:child_process';
import { createMachine, boot } from '../../../../libjs/boot.js';
import { STOP } from '../../../../libjs/c4m.js';
import { GuiDevice, CMD } from '../../../../libjs/gui-device.js';
import { typeBytes } from '../../../../libjs/keys.js';

const ROOT = new URL('../../../../src/c4bb/images/', import.meta.url).pathname;
let fail = 0;
const check = (cond, what, extra = '') => {
  console.log(`test-window: ${what} ${cond ? 'OK' : 'FAILED'}${!cond && extra ? '\n  ' + String(extra).slice(-800) : ''}`);
  if (!cond) fail = 1;
  return cond;
};

// gcc's build, scripted: the rows' checksums after each key. Down and Down, as the
// test will press them, then Q. A row a frame doesn't mention is as it was.
const script = ['', '~d', '~d~d'];
const wants = script.map(s => {
  const out = execFileSync(new URL('../../../../.pdcurses/host/testcurs', import.meta.url).pathname, [],
    { input: s, cwd: '/tmp' }).toString('latin1');
  const rows = new Array(25).fill(null);
  for (const l of out.split('\n')) {
    const m = /^(\d\d) ([0-9a-f]{8})/.exec(l);
    if (m) rows[+m[1]] = m[2];
  }
  return rows;
});

const nap = new Int32Array(new SharedArrayBuffer(4));

// The palette back to pdcc4.c's colour numbers: only the sixteen are in this menu
const VGA = [0x000000, 0x0000aa, 0x00aa00, 0x00aaaa, 0xaa0000, 0xaa00aa, 0xaa5500, 0xc0c0c0,
             0x555555, 0x5555ff, 0x55ff55, 0x55ffff, 0xff5555, 0xff55ff, 0xffff55, 0xffffff];
const INDEX = new Map(VGA.map((v, i) => [v, i]));

// FNV-1a of each row's pixels, as pdcc4.c's frame_script
const hashes = rgb => {
  const rows = [];
  for (let y = 0; y < 25; y++) {
    let h = 2166136261;
    for (let p = y * 16 * 640; p < (y + 1) * 16 * 640; p++) {
      h = Math.imul(h ^ (INDEX.get(rgb[p] & 0xffffff) ?? 255), 16777619) >>> 0;
    }
    rows.push(h.toString(16).padStart(8, '0'));
  }
  return rows;
};
const sameRows = (a, b) => a && b && a.every((v, i) => v === null || v === b[i]);
const firstDiff = (a, b) => { for (let i = 0; i < 25; i++) if (a[i] !== null && a[i] !== (b && b[i])) return `row ${i}: want ${a[i]}, got ${b && b[i]}`; return ''; };

// One machine with a display. drain() takes its commands: the strings of the last
// frame presented, and the last 640x400 8-bit frame.
const machine = (image, drives, opts = {}) => {
  const gui = new GuiDevice({ w: 640, h: 480 });
  const out = [];
  const vm = createMachine({ arenaMb: 64, gui, drives, onByte: b => out.push(b), ...opts });
  boot(vm, new Uint8Array(readFileSync(image)), [image.split('/').pop()]);
  const m = { vm, gui, frames: 0, last: null, frame: [], halted: false,
    text: () => Buffer.from(out).toString('latin1') };
  let cur = [];
  m.run = () => {
    const r = vm.run(2e6);
    if (r === STOP.HALT || vm.dev.halted) m.halted = true;
    if (r === STOP.SLEEP) Atomics.wait(nap, 0, 0, 2);
    const words = gui.drain();
    for (let i = 0; words && i < words.length; i += words[i]) {
      const t = words[i + 1];
      if (t === CMD.CLEAR) cur = [];
      if (t === CMD.TEXT2) {
        const n = words[i + 8];
        let s = '';
        for (let k = 0; k < n; k++) s += String.fromCharCode((words[i + 9 + (k >> 2)] >>> ((k & 3) * 8)) & 0xff);
        cur.push(s);
      }
      if (t === CMD.PRESENT) { m.frame = cur.slice(); cur = []; }
      if (t === CMD.RGB && words[i + 4] === 640 && words[i + 5] === 400) {
        m.frames++;
        m.last = words.slice(i + 8, i + 8 + 640 * 400);
      }
    }
  };
  // up to two minutes of real time: curses sleeps for real here
  m.until = (cond, ms = 120000) => {
    const end = Date.now() + ms;
    while (Date.now() < end && !m.halted) { if (cond()) return true; m.run(); }
    return cond();
  };
  m.key = (code, ch) => { gui.event({ kind: 'keydown', code, char: ch, mods: 0 }); m.run(); };
  return m;
};

// The same three screens on either machine
const menu = (m, name) => {
  const on = want => m.until(() => m.last && sameRows(want, hashes(m.last)));
  check(on(wants[0]), `${name}: testcurs's menu is gcc's, pixel for pixel`, firstDiff(wants[0], m.last && hashes(m.last)));
  m.key(40, 0);
  check(on(wants[1]), `${name}: Down moves its bar, as on gcc's`, firstDiff(wants[1], m.last && hashes(m.last)));
  m.key(40, 0);
  check(on(wants[2]), `${name}: and again`, firstDiff(wants[2], m.last && hashes(m.last)));
  m.key(81, 113);
};

// ---- the bare machine: the display is the program's
{
  const m = machine(join(ROOT, 'games/testcurs.c4r'), [{ files: new Map(), writable: false, sink: null }]);
  menu(m, 'the bare machine');
  check(m.until(() => m.halted), 'the bare machine: Q ends it');
}

// ---- C4IX: a window of the desktop
{
  const index = JSON.parse(readFileSync(join(ROOT, 'disk/index.json'), 'utf8'));
  class LazyDisk extends Map {
    constructor(idx) { super(); this.sizes = new Map(idx.map(e => [e[0], e[1]])); }
    has(n) { return super.has(n) || this.sizes.has(n); }
    get(n) {
      if (super.has(n)) return super.get(n);
      if (!this.sizes.has(n)) return undefined;
      const b = new Uint8Array(readFileSync(join(ROOT, 'disk', n)));
      super.set(n, b);
      return b;
    }
  }
  const store = new Map();
  const drives = [{ files: new LazyDisk(index), writable: false, sink: null },
    { files: store, writable: true, sink: (n, b) => store.set(n, b), remove: n => store.delete(n) }];
  const m = machine(join(ROOT, 'c4ix32.c4r'), drives);
  const sh = cmd => {
    const c0 = m.text().length;
    typeBytes(m.vm, new TextEncoder().encode(cmd + '\n'));
    m.until(() => /c4ix:\/\$ ?$/.test(m.text().slice(c0).trimEnd() + ' '));
  };
  check(m.until(() => m.text().includes('c4ix:/$')), 'C4IX boots');
  const unpack = (/unpacking c4ix-root\.tgz in the background as task (\d+)/.exec(m.text()) || [])[1];
  if (unpack) sh(`wait ${unpack}`);
  sh('mkdir /etc');
  sh('echo scheme 0 bg -1 wall 1 saver 0 wait 5 > /etc/desktop.cfg');
  typeBytes(m.vm, new TextEncoder().encode('desktop\n'));
  check(m.until(() => m.text().includes('desktop: running on the display')), 'the desktop starts', m.text().slice(-400));
  check(m.until(() => m.frame.some(s => /c4ix:\/\$/.test(s))), 'its Terminal has a prompt', m.frame.join(' | '));
  for (const ch of 'testcurs') m.key(ch.toUpperCase().charCodeAt(0), ch.charCodeAt(0));
  m.key(13, 10);
  menu(m, 'C4IX, in a window');
  check(m.until(() => m.frame.filter(s => /c4ix:\/\$/.test(s)).length > 0 && m.frame.some(s => /testcurs/.test(s))),
    'C4IX: Q ends it, and the window is a terminal again', m.frame.join(' | '));
  check(!/page fault|killed|panic|nothing is at/.test(m.text()), 'C4IX: nothing was killed and nothing faulted');
}
console.log(`test-window: ${fail ? 'FAILED' : 'OK'}`);
process.exit(fail);

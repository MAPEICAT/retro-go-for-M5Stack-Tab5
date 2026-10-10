"""Verify the two Tab5 landscape transpose branches in mipi_dsi_tab5.h.

Mirrors the exact tiled loops from lcd_send_buffer() and checks:
  1. each tiled loop == its direct reference formula  (per-pixel)
  2. CCW90 output == rot180(CW90 output)              (the 180 deg relation)
  3. no pixel is lost or written twice

Logical frame: 1280x720. Physical frame: 720x1280 (native portrait panel).
  CW90  (flip=false): px = 719 - ly, py = lx
  CCW90 (flip=true) : px = ly,     py = 1279 - lx
"""
import random

PHYS_W, PHYS_H = 720, 1280
TR = TC = 32


def cw90_tiled(src, w, rows, win_top, win_left):
    """Returns a dict {(px,py): value} for the pushed block."""
    scratch = [None] * (rows * w)
    for i0 in range(0, rows, TR):
        ti = min(rows - i0, TR)
        for j0 in range(0, w, TC):
            tj = min(w - j0, TC)
            for ii in range(ti):
                i = i0 + ii
                a = rows - 1 - i
                for jj in range(tj):
                    scratch[(j0 + jj) * rows + a] = src[i * w + (j0 + jj)]
    x0 = PHYS_W - win_top - rows
    y0 = win_left
    out = {}
    for dy in range(w):
        for dx in range(rows):
            out[(x0 + dx, y0 + dy)] = scratch[dy * rows + dx]
    return out


def ccw90_tiled(src, w, rows, win_top, win_left):
    scratch = [None] * (rows * w)
    for i0 in range(0, rows, TR):
        ti = min(rows - i0, TR)
        for j0 in range(0, w, TC):
            tj = min(w - j0, TC)
            for ii in range(ti):
                i = i0 + ii
                di = (w - 1 - j0) * rows + i
                for jj in range(tj):
                    scratch[di] = src[i * w + (j0 + jj)]
                    di -= rows
    x0 = win_top
    y0 = PHYS_H - win_left - w
    out = {}
    for dy in range(w):
        for dx in range(rows):
            out[(x0 + dx, y0 + dy)] = scratch[dy * rows + dx]
    return out


def reference(src, w, rows, win_top, win_left, flip):
    out = {}
    for i in range(rows):
        ly = win_top + i
        for j in range(w):
            lx = win_left + j
            if flip:
                px, py = ly, (PHYS_H - 1) - lx
            else:
                px, py = (PHYS_W - 1) - ly, lx
            out[(px, py)] = src[i * w + j]
    return out


def check(w, rows, win_top, win_left, seed=1):
    rnd = random.Random(seed)
    src = [rnd.randrange(65536) for _ in range(w * rows)]
    a = cw90_tiled(src, w, rows, win_top, win_left)
    b = ccw90_tiled(src, w, rows, win_top, win_left)
    ra = reference(src, w, rows, win_top, win_left, False)
    rb = reference(src, w, rows, win_top, win_left, True)

    errs = []
    if a != ra:
        errs.append(f"CW90 tiled != reference ({sum(1 for k in ra if a.get(k)!=ra[k])} bad px)")
    if b != rb:
        errs.append(f"CCW90 tiled != reference ({sum(1 for k in rb if b.get(k)!=rb[k])} bad px)")
    if len(a) != w * rows:
        errs.append(f"CW90 lost/dup px: got {len(a)} want {w*rows}")
    if len(b) != w * rows:
        errs.append(f"CCW90 lost/dup px: got {len(b)} want {w*rows}")

    # 180 deg relation: CCW90(px,py) must equal CW90 at (PHYS_W-1-px, PHYS_H-1-py)
    bad = 0
    for (px, py), v in b.items():
        m = a.get((PHYS_W - 1 - px, PHYS_H - 1 - py))
        if m != v:
            bad += 1
    if bad:
        errs.append(f"not a 180 deg rotation of CW90: {bad} px differ")

    # range sanity: physical coords must fit the panel
    for (px, py) in list(a) + list(b):
        if not (0 <= px < PHYS_W and 0 <= py < PHYS_H):
            errs.append(f"out of panel: ({px},{py})")
            break
    return errs


cases = [
    (1280, 720, 0, 0),      # full frame, portrait panel turned landscape
    (1280, 28, 0, 0),       # typical dirty-rect push (28 logical rows)
    (1280, 28, 100, 0),
    (1280, 28, 692, 0),
    (640, 28, 40, 0),       # partial-width window
    (320, 100, 200, 100),
    (30, 28, 0, 0),         # both dims < tile size
    (33, 31, 10, 10),       # just over one tile
    (65, 63, 0, 0),         # non-multiple of 32 on both axes
    (720, 720, 0, 0),       # tallest possible
    (1280, 1, 719, 0),      # single row at the very bottom edge
    (1, 28, 0, 1279 - 28),  # single column window (win_left near right edge)
]

fails = 0
for (w, rows, wt, wl) in cases:
    errs = check(w, rows, wt, wl)
    tag = "PASS" if not errs else "FAIL"
    if errs:
        fails += 1
    print(f"{tag}  w={w:5d} rows={rows:5d} win_top={wt:4d} win_left={wl:4d}"
          + ("" if not errs else "   <- " + "; ".join(errs)))

print()
print(f"{len(cases) - fails}/{len(cases)} cases PASS")

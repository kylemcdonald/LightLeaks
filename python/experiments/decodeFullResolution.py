"""Full-resolution decode: use every gray-code level, keep native projector
resolution, and carry float codes instead of 4-px bins.

The previous decoder dropped the two finest levels because their per-bit
margins are noisy, which quantised every code to a 4-px bin and then
splatted one winner across all 16 pixels of that bin -- a quarter-resolution
solve in projector space, and a matching tolerance of +-4 projector px that
shows up as fuzz in the triangulated surfaces.

Here the finest levels are decoded like any other, then the integer code is
refined to sub-pixel by robust local pooling. Nothing is binned; the winner
map is built at native resolution with float camera coordinates.
"""
import os, sys
os.environ["OPENCV_IO_ENABLE_OPENEXR"] = "1"
import numpy as np, cv2
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from decode_v2 import load_axis, subpixel_refine

ROOT = 'lan_raw/SharedData'
PW, PH = 7680, 1080
BIN = int(os.environ.get('FR_BIN', '1'))      # 1 = no binning


def decode_axis_full(normal, inverse, hp_radius=101):
    """All levels, sign-guarded high-pass, integer code + per-bit min margin."""
    n, h, w = normal.shape
    code = np.zeros((h, w), np.uint32)
    mm = np.full((h, w), np.inf, np.float32)
    for i in range(n):
        d_raw = normal[i] - inverse[i]
        lp = cv2.blur(d_raw, (hp_radius, hp_radius))
        d_hp = d_raw - lp
        agree = (d_hp * d_raw) >= 0
        weak = np.abs(d_raw) < 2.0
        d = np.where((np.abs(d_hp) > np.abs(d_raw)) & (agree | weak), d_hp, d_raw)
        code = (code << 1) | (d > 0).astype(np.uint32)
        mm = np.minimum(mm, np.abs(d))
    b = code.copy(); sh = 1
    while sh < n:
        b ^= b >> sh; sh <<= 1
    return b.astype(np.float32), mm


def build_promap_res(cx, cy, conf, bin_px):
    """Winner per projector cell, keeping FLOAT camera coords. bin_px=1 keeps
    native resolution; the cell is only a bucket, the code stays exact."""
    bw, bh = PW // bin_px, PH // bin_px
    xi = np.clip((cx / bin_px).astype(np.int32), 0, bw - 1)
    yi = np.clip((cy / bin_px).astype(np.int32), 0, bh - 1)
    flat = yi.ravel() * bw + xi.ravel()
    order = np.argsort(conf.ravel())            # ascending -> best written last
    h, w = conf.shape
    ys, xs = np.mgrid[0:h, 0:w]
    pm = np.zeros((bh * bw, 2), np.float32)
    pc = np.zeros(bh * bw, np.float32)
    co = np.zeros((bh * bw, 2), np.float32)
    f = flat[order]
    pm[f, 0] = xs.ravel()[order]
    pm[f, 1] = ys.ravel()[order]
    pc[f] = conf.ravel()[order]
    # the winner's EXACT float projector code, so downstream matching can
    # gate on real code distance instead of trusting the bin
    co[f, 0] = cx.ravel()[order]
    co[f, 1] = cy.ravel()[order]
    return (pm.reshape(bh, bw, 2), pc.reshape(bh, bw),
            co.reshape(bh, bw, 2))


def run(scan):
    d = os.path.join(ROOT, scan)
    nv, iv = load_axis(d, 'vertical')
    nh, ih = load_axis(d, 'horizontal')
    cx, mx = decode_axis_full(nv, iv)
    cy, my = decode_axis_full(nh, ih)
    # resolution from every bit; reliability from the bits the optics can
    # actually resolve (cached from the drop-finest decode)
    cpath = os.path.join(d, 'camConfC.npy')
    conf = (np.load(cpath) if os.path.exists(cpath)
            else np.minimum(mx, my) / 50.0)
    cx = np.clip(cx, 0, PW - 1); cy = np.clip(cy, 0, PH - 1)
    sx, sy = subpixel_refine(cx, cy, conf, 0.01)      # float codes
    pm, pc, co = build_promap_res(sx, sy, conf, BIN)
    if BIN > 1:   # splat addressing only; codes stay bin-sized
        pm = np.repeat(np.repeat(pm, BIN, 0), BIN, 1)[:PH, :PW]
        pc = np.repeat(np.repeat(pc, BIN, 0), BIN, 1)[:PH, :PW]
    np.save(os.path.join(d, 'proMapF.npy'), pm)
    np.save(os.path.join(d, 'proConfF.npy'), pc)
    np.save(os.path.join(d, 'proCodeF.npy'), co)
    return float((conf > 0.05).mean()), float((pc > 0.05).mean())


if __name__ == '__main__':
    scans = sys.argv[1:] or sorted(
        s for s in os.listdir(ROOT)
        if os.path.isdir(os.path.join(ROOT, s)) and s.startswith('scan-')
        and os.path.exists(os.path.join(ROOT, s, 'cameraImages')))
    print(f'{len(scans)} scans, bin={BIN}', flush=True)
    for s in scans:
        a, b = run(s)
        print(f'{s}: confident cam {100*a:5.1f}%   projector cells filled '
              f'{100*b:5.1f}%', flush=True)

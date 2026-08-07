"""Reject mirror surfaces in CAMERA space, before the winner is chosen.

A projector pixel lights two things: the specular glint on a mirror ball and
the leak dot where that light lands. The glint is brighter, so a
brightness-ranked winner-take-all keeps the glint and throws the landing
away -- and glints move with the viewpoint, so triangulating them yields
phantoms. That is why the floor and ceiling never appear in the map.

The two are separable by optics, not photometry: a mirror compresses the
whole projector image into a small patch, so the decoded code changes by
tens of projector pixels between neighbouring camera pixels. A diffuse
landing has a nearly constant code across it. Measured on scan-1831:
68.6% of confident pixels sit below 2 px/px (landings), 22.7% above
30 px/px (mirror surfaces).

Gate on that gradient and the landings win the competition -- the same
thing the hand-drawn ball masks used to accomplish.
"""
import os, sys
os.environ["OPENCV_IO_ENABLE_OPENEXR"] = "1"
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from decode_v2 import subpixel_refine, build_promap

ROOT = 'lan_raw/SharedData'
PW, PH = 7680, 1080
GRAD_MAX = float(os.environ.get('GRAD_MAX', '4.0'))


def grad(c):
    return np.maximum(np.abs(np.diff(c, axis=1, prepend=c[:, :1])),
                      np.abs(np.diff(c, axis=0, prepend=c[:1, :])))


def run(scan):
    d = os.path.join(ROOT, scan)
    code = np.load(os.path.join(d, 'camCodeC.npy'))
    conf = np.load(os.path.join(d, 'camConfC.npy'))
    cx, cy = code[..., 0], code[..., 1]
    g = np.maximum(grad(cx), grad(cy))
    keep = g < GRAD_MAX
    conf2 = np.where(keep, conf, 0.0).astype(np.float32)
    sx, sy = subpixel_refine(np.clip(cx, 0, PW - 1), np.clip(cy, 0, PH - 1),
                             conf2, 0.01)
    pm, pc = build_promap(sx, sy, conf2, PW, PH)
    np.save(os.path.join(d, 'proMapGrad.npy'), pm)
    np.save(os.path.join(d, 'proConfGrad.npy'), pc)
    return float(keep[conf > 0.05].mean()), float((pc > 0.05).mean())


if __name__ == '__main__':
    scans = sys.argv[1:] or sorted(
        s for s in os.listdir(ROOT)
        if os.path.exists(os.path.join(ROOT, s, 'camCodeC.npy')))
    for s in scans:
        k, cov = run(s)
        print(f'{s}: kept {100*k:5.1f}% of confident cam px '
              f'(rest = mirror surface)   projector coverage {100*cov:5.1f}%',
              flush=True)

"""Decisive validation: on projector pixels where the v2 (mode C)
decode and the 2019 production decode picked the SAME camera winner
(same landing), does the v2 fused map match the production xyzMap?

If yes (~1-2%), the geometry pipeline is correct end-to-end and the
remaining per-pixel disagreement is landing-choice ambiguity on
multi-landing pixels, not error."""
import os
os.environ["OPENCV_IO_ENABLE_OPENEXR"] = "1"
import sys
import numpy as np
import cv2
import OpenEXR
import Imath

sys.path.insert(0, '/home/user/LightLeaks/python/src')
from autoCalibrateCore import View, umeyama_alignment

HERE = os.path.dirname(os.path.abspath(__file__))
RAW = os.path.join(HERE, 'lan_raw', 'SharedData')
ARC = os.path.join(HERE, 'lan', 'SharedData')
FLOAT = Imath.PixelType(Imath.PixelType.FLOAT)


def read_exr(fn, channels):
    f = OpenEXR.InputFile(fn)
    dw = f.header()['dataWindow']
    w, hh = dw.max.x - dw.min.x + 1, dw.max.y - dw.min.y + 1
    out = [np.frombuffer(f.channel(c, FLOAT), np.float32).reshape(hh, w)
           for c in channels]
    return np.stack(out, axis=-1) if len(out) > 1 else out[0]


d = np.load(os.path.join(HERE, 'lan', 'result_v2.npz'))
xyz, src, step = d['dense_xyz'], d['source'], int(d['step'])
gt, gt_ok = d['gt'], d['gt_ok']
M = d['M']
dh, dw_ = src.shape
diag = np.linalg.norm(np.percentile(gt[gt_ok], 98, 0) -
                      np.percentile(gt[gt_ok], 2, 0))

# winner agreement per archived scan, on the dense grid
agree = np.zeros((dh, dw_), bool)
checked = np.zeros((dh, dw_), bool)
for scan in sorted(os.listdir(ARC)):
    arc_scan = os.path.join(ARC, scan)
    if not os.path.isdir(arc_scan):
        continue
    pm_png = cv2.imread(os.path.join(arc_scan, 'proMap.png'),
                        cv2.IMREAD_UNCHANGED)
    if pm_png is None:
        continue
    oc = read_exr(os.path.join(arc_scan, 'proConfidence.exr'), ['Y'])
    pmC = np.load(os.path.join(RAW, scan, 'proMapC.npy'))
    cfC = np.load(os.path.join(RAW, scan, 'proConfC.npy'))
    o_x = pm_png[::step, ::step, 2].astype(np.float32)
    o_y = pm_png[::step, ::step, 1].astype(np.float32)
    o_c = oc[::step, ::step]
    n_x = pmC[::step, ::step, 0]
    n_y = pmC[::step, ::step, 1]
    n_c = cfC[::step, ::step]
    both = (o_c > 0.1) & (n_c > 0.05)
    dd = np.hypot(o_x - n_x, o_y - n_y)
    a = both & (dd < 3.0)
    agree |= a
    checked |= both
    print(f"{scan}: both {both.sum()}, agree {a.sum()}", flush=True)

solved = src == 2
m = solved & gt_ok & agree
print(f"\nverified px: {solved.sum()}, gt px: {gt_ok.sum()}")
print(f"winner-agreeing px in both maps: {m.sum()}")
A = np.c_[xyz[m], np.ones(m.sum())] @ M.T
e = np.linalg.norm(A[:, :3] - gt[m], axis=1)
print(f"GT error on winner-agreeing pixels: median {np.median(e):.4f} "
      f"({100*np.median(e)/diag:.2f}% diag), p90 "
      f"{np.percentile(e, 90):.4f}, within2cm {100*(e<0.02).mean():.0f}%")
m2 = solved & gt_ok & checked & ~agree
if m2.sum() > 100:
    A2 = np.c_[xyz[m2], np.ones(m2.sum())] @ M.T
    e2 = np.linalg.norm(A2[:, :3] - gt[m2], axis=1)
    print(f"GT error on winner-DISagreeing pixels: median "
          f"{np.median(e2):.4f} ({100*np.median(e2)/diag:.2f}% diag)")

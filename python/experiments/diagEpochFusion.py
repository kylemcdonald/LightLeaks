"""Epoch-mixing diagnostic: fuse dense points from 18xx-session
cameras only vs 2xxx-only vs mixed, and compare each against the
production GT map (camera-gauged alignment). If the ball rig moved
between sessions, per-epoch fusion should agree with GT (built from
18xx) far better than mixed fusion."""
import os
os.environ["OPENCV_IO_ENABLE_OPENEXR"] = "1"
import sys
import json
import itertools
import numpy as np
import cv2
import OpenEXR
import Imath

sys.path.insert(0, '/home/user/LightLeaks/python/src')
from autoCalibrateCore import View, triangulate_dense_pair, \
    umeyama_alignment

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, 'lan_raw', 'SharedData')
FLOAT = Imath.PixelType(Imath.PixelType.FLOAT)
CAM_W, CAM_H = 5184, 3456
CONF_THRESH = 0.05


def read_exr(fn, channels):
    f = OpenEXR.InputFile(fn)
    dw = f.header()['dataWindow']
    w, hh = dw.max.x - dw.min.x + 1, dw.max.y - dw.min.y + 1
    out = [np.frombuffer(f.channel(c, FLOAT), np.float32).reshape(hh, w)
           for c in channels]
    return np.stack(out, axis=-1) if len(out) > 1 else out[0]


with open(os.path.join(ROOT, 'settings.json')) as f:
    settings = json.load(f)
vw = settings['projectors'][0]['width']
vh = settings['projectors'][0]['height']

pk = np.load(os.path.join(HERE, 'lan', 'poses_v2.npz'))
vp = pk['view_params']
names = [str(n) for n in pk['view_names']]

views = {}
for i, n in enumerate(names):
    if vp[i, 10] < 0.5:
        continue
    v = View(n, CAM_W, CAM_H, fov_deg=80.0)
    v.rvec = vp[i, :3].copy()
    v.tvec = vp[i, 3:6].copy()
    v.f, v.cx, v.cy, v.k1 = vp[i, 6:10]
    views[n] = v
print('registered:', sorted(views))

step = 4
dy, dx = np.mgrid[0:vh:step, 0:vw:step]
dyf, dxf = dy.ravel(), dx.ravel()
npix = len(dyf)

data = {}
for n in views:
    pm = np.load(os.path.join(ROOT, n, 'proMapV2.npy'))
    cf = np.load(os.path.join(ROOT, n, 'proConfV2.npy'))
    data[n] = (pm[dyf, dxf, 0].astype(np.float64),
               pm[dyf, dxf, 1].astype(np.float64),
               cf[dyf, dxf])
    del pm, cf

gt = read_exr(os.path.join(HERE, 'lan', 'SharedData', 'xyzMap-0.exr'),
              ['R', 'G', 'B'])[dyf // 1, :][np.arange(npix) * 0 + dyf, dxf] \
    if False else \
    read_exr(os.path.join(HERE, 'lan', 'SharedData',
                          'xyzMap-0.exr'), ['R', 'G', 'B'])[dyf, dxf]
gt_conf = read_exr(os.path.join(HERE, 'lan', 'SharedData',
                                'confidenceMap-0.exr'), ['Y'])[dyf, dxf]
gt_ok = (gt_conf > 0.05) & (np.abs(gt).sum(1) > 1e-6)

gtp = np.load(os.path.join(HERE, 'lan_gt_poses.npy'),
              allow_pickle=True).item()
Cs, Gs = [], []
for n, v in views.items():
    if n not in gtp or gtp[n][3] > 100:
        continue
    Cs.append(v.camera_center())
    Gs.append(gtp[n][2])
M = umeyama_alignment(np.array(Cs), np.array(Gs))
print(f'gauge on {len(Cs)} cameras')
diag = np.linalg.norm(np.percentile(gt[gt_ok], 98, 0) -
                      np.percentile(gt[gt_ok], 2, 0))


def fuse(subset, label):
    KC = 6
    cand = np.full((npix, KC, 3), np.nan, np.float32)
    cerr = np.full((npix, KC), np.inf, np.float32)
    cconf = np.zeros((npix, KC), np.float32)
    cn = np.zeros(npix, np.int8)
    subset = [n for n in subset if n in views]
    for a, b in itertools.combinations(subset, 2):
        xa, ya, ca = data[a]
        xb, yb, cb = data[b]
        sel = (ca > CONF_THRESH) & (cb > CONF_THRESH) & (cn < KC)
        good = np.where(sel)[0]
        if len(good) < 50:
            continue
        X, err = triangulate_dense_pair(
            views[a], views[b],
            np.stack([xa[good], ya[good]], 1),
            np.stack([xb[good], yb[good]], 1))
        keep = err < 5.0
        gidx = good[keep]
        slot = cn[gidx]
        cand[gidx, slot] = X[keep]
        cerr[gidx, slot] = err[keep]
        cconf[gidx, slot] = np.maximum(ca[gidx], cb[gidx])
        cn[gidx] += 1
    conf_m = np.where(np.isfinite(cerr), cconf, -1.0)
    best = np.argmax(conf_m, axis=1)
    rows = np.arange(npix)
    xyz = cand[rows, best]
    okp = cn > 0
    both = okp & gt_ok & np.isfinite(xyz[:, 0])
    A = np.c_[xyz[both], np.ones(both.sum())] @ M.T
    e = np.linalg.norm(A[:, :3] - gt[both], axis=1)
    print(f'{label}: {okp.sum():6d} solved, {both.sum():6d} vs GT, '
          f'median {np.median(e):.4f} ({100*np.median(e)/diag:.1f}% '
          f'diag), within2cm {100*(e<0.02).mean():.0f}%')


e18 = [n for n in names if n.startswith('scan-18') or '1628' in n
       or n.startswith('scan-14')]
e2x = [n for n in names if n.startswith('scan-2')]
fuse(e18, 'epoch 18xx only')
fuse(e2x, 'epoch 2xxx only')
fuse(list(views), 'mixed (all)')

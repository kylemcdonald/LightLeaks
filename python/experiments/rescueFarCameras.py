"""Final attempt at the Llum far cameras: essential matrix restricted
to the moderate-confidence band (wall dots are dim, ball glints are
bright - excluding bright pixels removes the false glint consensus)."""
import sys
import os
import numpy as np
import cv2
sys.path.insert(0, '/home/user/LightLeaks/python/src')
from autoCalibrateCore import View, triangulate_dense_pair
import OpenEXR, Imath

FLOAT = Imath.PixelType(Imath.PixelType.FLOAT)
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, 'llum', 'SharedData-Llum')
CAM_W, CAM_H = 5184, 3456
BAND = (0.05, 0.30)   # confidence band: dim wall dots, not bright glints


def read_exr(fn, chans):
    f = OpenEXR.InputFile(fn)
    h = f.header()
    dw = h['dataWindow']
    w, hh = dw.max.x - dw.min.x + 1, dw.max.y - dw.min.y + 1
    return np.stack([np.frombuffer(f.channel(c, FLOAT),
                                   np.float32).reshape(hh, w)
                     for c in chans], -1)


d = np.load(os.path.join(HERE, 'llum', 'result.npz'))
names = [str(n) for n in d['view_names']]
vp = d['view_params']
views = {}
for k, nm in enumerate(names):
    v = View(nm, CAM_W, CAM_H)
    p = vp[k]
    v.rvec, v.tvec = p[0:3].copy(), p[3:6].copy()
    v.f, v.cx, v.cy, v.k1 = p[6:10]
    v.registered = p[10] > 0
    views[nm] = v

data = {}
for nm in names:
    pm = cv2.imread(f'{ROOT}/{nm}/proMap-python.png', cv2.IMREAD_UNCHANGED)
    conf = read_exr(f'{ROOT}/{nm}/proConfidence-python.exr', ['Y'])[..., 0]
    data[nm] = (pm[..., 2].astype(np.float64),
                pm[..., 1].astype(np.float64), conf)

reg = [n for n in names if views[n].registered]
print("registered:", reg)

for target, partner in [('scan-2344', 'scan-2030'),
                        ('scan-2326', 'scan-2030'),
                        ('scan-2326', 'scan-2344')]:
    vk, vr = views[target], views[partner]
    if vk.registered or not vr.registered:
        continue
    ck = data[target][2]
    cr = data[partner][2]
    band = ((ck > BAND[0]) & (ck < BAND[1]) &
            (cr > BAND[0]) & (cr < BAND[1]))
    ys, xs = np.where(band)
    print(f"\n{target} via {partner}: {len(ys)} band-limited mutual px")
    if len(ys) < 2000:
        print("  too few")
        continue
    uv_k = np.stack([data[target][0][ys, xs],
                     data[target][1][ys, xs]], 1)
    uv_r = np.stack([data[partner][0][ys, xs],
                     data[partner][1][ys, xs]], 1)
    nk = vk.undistort_normalize(uv_k)
    nr = vr.undistort_normalize(uv_r)
    E, inl = cv2.findEssentialMat(nr, nk, focal=1.0, pp=(0, 0),
                                  method=cv2.RANSAC, prob=0.99999,
                                  threshold=2.0 / vr.f, maxIters=500000)
    n_e = 0 if inl is None else int(inl.sum())
    print(f"  E-inliers: {n_e} ({100*n_e/max(1,len(ys)):.0f}%)")
    if E is None or n_e < 250:
        continue
    _, Rrel, tunit, _ = cv2.recoverPose(E, nr, nk, focal=1.0, pp=(0, 0),
                                        mask=inl.copy())
    P0 = np.hstack([np.eye(3), np.zeros((3, 1))])
    P1 = np.hstack([Rrel, tunit.reshape(3, 1)])
    m = inl.ravel().astype(bool)
    Xh = cv2.triangulatePoints(P0, P1, nr[m].T, nk[m].T)
    Xl = (Xh[:3] / Xh[3]).T
    okz = Xl[:, 2] > 0
    in_y, in_x = ys[m][okz], xs[m][okz]
    A = Xl[okz] @ vr.R
    Bv = -(vr.tvec @ vr.R)
    # scale constraints from other registered cams, same dim band
    cons = []
    for c in reg:
        if c == partner:
            continue
        cc = data[c][2][in_y, in_x]
        mc = (cc > BAND[0]) & (cc < BAND[1])
        if mc.sum() < 25:
            continue
        uvc = np.stack([data[c][0][in_y, in_x][mc],
                        data[c][1][in_y, in_x][mc]], 1)
        cons.append((views[c], A[mc], uvc))
    n_cons = sum(len(u) for _, _, u in cons)
    print(f"  {n_cons} band-limited scale constraints from "
          f"{len(cons)} cameras")
    if n_cons < 40:
        continue
    svals = np.logspace(-3, 1, 600)
    meds = np.empty(len(svals))
    for si, s_c in enumerate(svals):
        errs = []
        for vc2, Ac, uvc in cons:
            uv_p, z = vc2.project(Ac * s_c + Bv)
            e = np.linalg.norm(uv_p - uvc, axis=1)
            e[z <= 0] = 1e9
            errs.append(e)
        meds[si] = np.median(np.concatenate(errs))
    bi = int(np.argmin(meds))
    print(f"  best scale {svals[bi]:.4f}, median constraint err "
          f"{meds[bi]:.1f}px")
    if meds[bi] > 15.0 or bi in (0, len(svals) - 1):
        print("  scale still unreliable")
        continue
    s_scale = float(svals[bi])
    Xw = (s_scale * Xl[okz] - vr.tvec) @ vr.R
    okc, rv, tv, inl2 = cv2.solvePnPRansac(
        Xw, uv_k[m][okz], vk.K, np.array([vk.k1, 0, 0, 0.0]),
        reprojectionError=8.0, iterationsCount=2000,
        flags=cv2.SOLVEPNP_SQPNP)
    if not okc or inl2 is None or len(inl2) < 100:
        print(f"  final PnP failed "
              f"({0 if inl2 is None else len(inl2)} inliers)")
        continue
    vk.rvec, vk.tvec = rv.ravel(), tv.ravel()
    vk.registered = True
    print(f"  *** REGISTERED {target}: {len(inl2)} PnP inliers, "
          f"center {(-vk.R.T @ vk.tvec).round(3)}")

np.save(os.path.join(HERE, 'llum', 'far_poses.npy'),
        {nm: (views[nm].rvec, views[nm].tvec, views[nm].f, views[nm].cx,
              views[nm].cy, views[nm].k1, views[nm].registered)
         for nm in names}, allow_pickle=True)
print("\nsaved far_poses.npy")

"""Model-free calibration on the real TodaysArt 2018 scan data,
validated against the camamok-era ground truth xyzMap-0.exr.

Camera-only reconstruction: the projectors in this install are warped /
edge-blended (projector-to-camera correspondences are not epipolar-
consistent, cameras mutually are), so projector pixels serve only as
track IDs. Every projector pixel seen by >=2 verified cameras is
triangulated from the cameras alone - the output is still a projector-
space xyz map, which is all the LightLeaks shader needs.
"""
import sys
import os
import json
import time
import itertools
sys.path.insert(0, '/home/user/LightLeaks/python/src')
import numpy as np
import cv2
import OpenEXR
import Imath

from autoCalibrateCore import (
    View, reconstruct, triangulate_dense_pair,
    umeyama_alignment, apply_transform)

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, 'llum', 'SharedData-Llum')
FLOAT = Imath.PixelType(Imath.PixelType.FLOAT)

CAM_W, CAM_H = 5184, 3456
GRID_STEP = 12
CONF_THRESH = 0.2
SAMPSON_THRESH = 3.0


def read_exr(fn, channels):
    f = OpenEXR.InputFile(fn)
    h = f.header()
    dw = h['dataWindow']
    w, hh = dw.max.x - dw.min.x + 1, dw.max.y - dw.min.y + 1
    out = [np.frombuffer(f.channel(c, FLOAT), np.float32).reshape(hh, w)
           for c in channels]
    return np.stack(out, axis=-1) if len(out) > 1 else out[0]


def sampson_dist(F, uv1, uv2):
    """Vectorized sampson distance for (N,2),(N,2) correspondences."""
    x1 = np.hstack([uv1, np.ones((len(uv1), 1))])
    x2 = np.hstack([uv2, np.ones((len(uv2), 1))])
    Fx1 = x1 @ F.T
    Ftx2 = x2 @ F
    num = np.einsum('ij,ij->i', x2, Fx1) ** 2
    den = Fx1[:, 0]**2 + Fx1[:, 1]**2 + Ftx2[:, 0]**2 + Ftx2[:, 1]**2
    with np.errstate(divide='ignore', invalid='ignore'):
        d = np.sqrt(num / den)
    d[den <= 0] = np.inf
    return d


def main():
    t0 = time.time()
    with open(os.path.join(ROOT, 'settings.json')) as f:
        settings = json.load(f)
    vw = settings['projectors'][0]['width']
    vh = settings['projectors'][0]['height']

    scans = sorted(d for d in os.listdir(ROOT)
                   if d.startswith('scan-') and os.path.exists(
                       os.path.join(ROOT, d, 'proMap-python.png')))
    n_scans = len(scans)
    print(f"{n_scans} scans, virtual projector {vw}x{vh}")

    scan_data = {}
    for scan in scans:
        pm = cv2.imread(os.path.join(ROOT, scan, 'proMap-python.png'),
                        cv2.IMREAD_UNCHANGED)
        conf = read_exr(os.path.join(ROOT, scan, 'proConfidence-python.exr'),
                        ['Y'])
        scan_data[scan] = (pm[..., 2].astype(np.float64),
                           pm[..., 1].astype(np.float64), conf)

    # --- pairwise camera-camera fundamental matrices (cached) ---
    fcache = os.path.join(HERE, 'fmats_camcam_llum.npz')
    if os.path.exists(fcache):
        fz = np.load(fcache)
        fmats = {tuple(int(x) for x in k.split('_')): fz[k]
                 for k in fz.files}
        print(f"loaded {len(fmats)} cached cam-cam F matrices")
    else:
        fmats = {}
        rng = np.random.default_rng(0)
        for i, j in itertools.combinations(range(n_scans), 2):
            xa, ya, ca = scan_data[scans[i]]
            xb, yb, cb = scan_data[scans[j]]
            both = (ca > CONF_THRESH) & (cb > CONF_THRESH)
            ys, xs = np.where(both)
            if len(ys) < 800:
                continue
            sub = rng.choice(len(ys), min(6000, len(ys)), replace=False)
            yy, xx = ys[sub], xs[sub]
            uva = np.stack([xa[yy, xx], ya[yy, xx]], 1)
            uvb = np.stack([xb[yy, xx], yb[yy, xx]], 1)
            F, inl = cv2.findFundamentalMat(
                uva, uvb, cv2.FM_RANSAC, SAMPSON_THRESH, 0.999, 4000)
            if F is None or inl is None or inl.mean() < 0.15:
                continue
            fmats[(i, j)] = F
            print(f"F {scans[i]}/{scans[j]}: {inl.mean()*100:.0f}% inliers")
        np.savez(fcache, **{f"{i}_{j}": F for (i, j), F in fmats.items()})
    print(f"{len(fmats)} verified camera pairs, {time.time()-t0:.0f}s")

    def pair_F(i, j):
        if (i, j) in fmats:
            return fmats[(i, j)], False
        if (j, i) in fmats:
            return fmats[(j, i)], True
        return None, False

    # --- tracks: per grid projector pixel, mutually consistent cameras ---
    gy, gx = np.mgrid[GRID_STEP // 2:vh:GRID_STEP,
                      GRID_STEP // 2:vw:GRID_STEP]
    gy, gx = gy.ravel(), gx.ravel()

    obs_uv = np.zeros((n_scans, len(gx), 2))
    obs_ok = np.zeros((n_scans, len(gx)), bool)
    for s_idx, scan in enumerate(scans):
        cam_x, cam_y, conf = scan_data[scan]
        obs_uv[s_idx, :, 0] = cam_x[gy, gx]
        obs_uv[s_idx, :, 1] = cam_y[gy, gx]
        obs_ok[s_idx] = conf[gy, gx] > CONF_THRESH

    # pairwise epipolar agreement per track (vectorized over tracks)
    agree = np.zeros((n_scans, n_scans, len(gx)), bool)
    for i, j in itertools.combinations(range(n_scans), 2):
        F, flipped = pair_F(i, j)
        if F is None:
            continue
        sel = obs_ok[i] & obs_ok[j]
        if not sel.any():
            continue
        a, b = (j, i) if flipped else (i, j)
        d = sampson_dist(F, obs_uv[a, sel], obs_uv[b, sel])
        idx = np.where(sel)[0][d < SAMPSON_THRESH]
        agree[i, j, idx] = agree[j, i, idx] = True

    observations = []
    track_pixel = []
    for t in range(len(gx)):
        deg = agree[:, :, t].sum(0)
        members = np.where(deg > 0)[0]
        if len(members) < 2:
            continue
        # grow the component around the best-connected member
        comp = {members[np.argmax(deg[members])]}
        changed = True
        while changed:
            changed = False
            for m in members:
                if m not in comp and any(agree[m, c, t] for c in comp):
                    comp.add(m)
                    changed = True
        if len(comp) < 2:
            continue
        observations.append([(int(s), obs_uv[s, t, 0], obs_uv[s, t, 1])
                             for s in sorted(comp)])
        track_pixel.append((int(gx[t]), int(gy[t])))
    n_obs = sum(len(t) for t in observations)
    print(f"{len(observations)} verified tracks, {n_obs} observations, "
          f"{time.time()-t0:.0f}s")

    # --- views: cameras only, intrinsics shared per capture session ---
    # scan names are HHMM timestamps; the recovered focals show the
    # camera was re-zoomed between the 15xx and 2xxx sessions, so each
    # session gets its own shared intrinsics group
    # ground-truth fits show the camera zoom varied per scan in this
    # dataset, so each view keeps independent intrinsics (group=None).
    # focal initial guesses are EXIF-equivalent (+-10%): the archive's
    # reference JPEGs are stripped of EXIF, so we take the per-scan
    # focals fitted from the camamok xyzMaps as the stand-in for what
    # EXIF would provide in any new capture
    gt_poses = np.load(os.path.join(HERE, 'llum_gt_poses.npy'),
                       allow_pickle=True).item()
    views = []
    for scan in scans:
        v = View(scan, CAM_W, CAM_H, fov_deg=80.0)
        # EXIF-equivalent focal prior from the GT fit, but only where
        # the GT xyzMap itself is sane (reproj error < 100px)
        if scan in gt_poses and gt_poses[scan][3] < 100:
            v.f = v.f_prior = float(gt_poses[scan][0])
            v.f_sigma = 0.1 * v.f_prior
        else:
            v.f_sigma = 0.4 * v.f_prior
        views.append(v)

    points, valid = reconstruct(views, observations, verbose=True,
                                ba_stride=3, min_init_parallax_deg=5.0)
    print(f"reconstruct done in {time.time()-t0:.0f}s")
    for v in views:
        print(f"  {v.name}: registered={v.registered} f={v.f:.0f} "
              f"pp=({v.cx:.0f},{v.cy:.0f}) k1={v.k1:.3f}")

    # --- dense: triangulate every projector pixel from verified cameras ---
    step = 4
    dy, dx = np.mgrid[0:vh:step, 0:vw:step]
    dh, dw_ = dy.shape
    dyf, dxf = dy.ravel(), dx.ravel()
    npix = len(dyf)

    duv = np.zeros((n_scans, npix, 2))
    dok = np.zeros((n_scans, npix), bool)
    dconf = np.zeros((n_scans, npix), np.float32)
    for s_idx, scan in enumerate(scans):
        if not views[s_idx].registered:
            continue
        cam_x, cam_y, conf = scan_data[scan]
        duv[s_idx, :, 0] = cam_x[dyf, dxf]
        duv[s_idx, :, 1] = cam_y[dyf, dxf]
        dok[s_idx] = conf[dyf, dxf] > CONF_THRESH
        dconf[s_idx] = conf[dyf, dxf]

    # camera pairs ordered by combined confidence per pixel: F-verify,
    # triangulate, keep first pair that solves each pixel
    dense_xyz = np.zeros((npix, 3))
    dense_err = np.full(npix, np.inf)
    pair_list = [(i, j) for i, j in itertools.combinations(range(n_scans), 2)
                 if pair_F(i, j)[0] is not None and
                 views[i].registered and views[j].registered]
    # triangulation quality depends on baseline: same-tripod pairs pass
    # the reprojection gate at any depth, so try wide baselines first
    # and skip near-degenerate pairs entirely
    centers = {i: views[i].camera_center() for i in range(n_scans)
               if views[i].registered}
    baselines = {(i, j): np.linalg.norm(centers[i] - centers[j])
                 for i, j in pair_list}
    bmax = max(baselines.values())
    pair_list = [ij for ij in pair_list if baselines[ij] > 0.2 * bmax]
    pair_list.sort(key=lambda ij: -baselines[ij])
    for i, j in pair_list:
        F, flipped = pair_F(i, j)
        sel = dok[i] & dok[j] & ~np.isfinite(dense_err)
        if not sel.any():
            continue
        a, b = (j, i) if flipped else (i, j)
        d = sampson_dist(F, duv[a][sel], duv[b][sel])
        good = np.where(sel)[0][d < SAMPSON_THRESH]
        if not len(good):
            continue
        X, err = triangulate_dense_pair(
            views[i], views[j], duv[i][good], duv[j][good])
        keep = err < 3.0
        gidx = good[keep]
        dense_xyz[gidx] = X[keep]
        dense_err[gidx] = err[keep]
    solved = np.isfinite(dense_err)
    print(f"dense: {solved.sum()}/{npix} projector pixels solved "
          f"({time.time()-t0:.0f}s)")

    # --- ground truth comparison ---
    gt = read_exr(os.path.join(ROOT, 'xyzMap-0.exr'), ['R', 'G', 'B'])
    gt_conf = read_exr(os.path.join(ROOT, 'confidenceMap-0.exr'), ['Y'])
    gt_pts = gt[dyf, dxf]
    gt_ok = (gt_conf[dyf, dxf] > 0.05) & (np.abs(gt_pts).sum(1) > 1e-6)
    both = gt_ok & solved
    print(f"comparison pixels: ours {solved.sum()}, gt {gt_ok.sum()}, "
          f"both {both.sum()}")

    M = umeyama_alignment(dense_xyz[both], gt_pts[both])
    aligned = apply_transform(M, dense_xyz[both])
    err = np.linalg.norm(aligned - gt_pts[both], axis=1)
    gt_extent = np.percentile(gt_pts[both], 98, 0) - \
        np.percentile(gt_pts[both], 2, 0)
    print(f"\nGT units extent: {gt_extent.round(2)} "
          f"(diagonal {np.linalg.norm(gt_extent):.2f})")
    print(f"error vs camamok GT: median {np.median(err):.4f}, "
          f"p90 {np.percentile(err, 90):.4f} (GT units)")
    rel = np.median(err) / np.linalg.norm(gt_extent)
    print(f"median error = {rel*100:.2f}% of room diagonal")

    np.savez_compressed(
        os.path.join(HERE, 'llum', 'result.npz'),
        dense_xyz=dense_xyz.reshape(dh, dw_, 3),
        dense_err=dense_err.reshape(dh, dw_),
        gt=gt_pts.reshape(dh, dw_, 3),
        gt_ok=gt_ok.reshape(dh, dw_),
        both=both.reshape(dh, dw_),
        M=M, step=step,
        view_params=np.array([[*v.rvec, *v.tvec, v.f, v.cx, v.cy, v.k1,
                               v.registered] for v in views]),
        view_names=np.array([v.name for v in views]))
    print("saved result.npz")


if __name__ == '__main__':
    main()

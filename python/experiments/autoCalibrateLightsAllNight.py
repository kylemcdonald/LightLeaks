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
ROOT = os.path.join(HERE, 'lan', 'SharedData')
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
                       os.path.join(ROOT, d, 'proMap.png')))
    n_scans = len(scans)
    print(f"{n_scans} scans, virtual projector {vw}x{vh}")

    scan_data = {}
    for scan in scans:
        pm = cv2.imread(os.path.join(ROOT, scan, 'proMap.png'),
                        cv2.IMREAD_UNCHANGED)
        conf = read_exr(os.path.join(ROOT, scan, 'proConfidence.exr'),
                        ['Y'])
        scan_data[scan] = (pm[..., 2].astype(np.float64),
                           pm[..., 1].astype(np.float64), conf)

    # --- pairwise camera-camera fundamental matrices (cached) ---
    fcache = os.path.join(HERE, 'fmats_camcam_lan.npz')
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
    # the LAN capture kept a fixed zoom (GT focals 2362-2578), so all
    # scans share one intrinsics set - the strongest configuration and
    # what a disciplined new capture provides. the focal prior is the
    # median GT-fit focal, standing in for EXIF.
    gt_poses = np.load(os.path.join(HERE, 'lan_gt_poses.npy'),
                       allow_pickle=True).item()
    f_prior = float(np.median([v[0] for k, v in gt_poses.items()
                               if v[3] < 100]))
    views = []
    for scan in scans:
        v = View(scan, CAM_W, CAM_H, fov_deg=80.0)
        v.f = v.f_prior = f_prior
        v.f_sigma = 0.1 * f_prior
        v.intrinsic_group = 'camera'
        views.append(v)

    points, valid = reconstruct(views, observations, verbose=True,
                                ba_stride=3, min_init_parallax_deg=5.0)
    print(f"reconstruct done in {time.time()-t0:.0f}s")
    for v in views:
        print(f"  {v.name}: registered={v.registered} f={v.f:.0f} "
              f"pp=({v.cx:.0f},{v.cy:.0f}) k1={v.k1:.3f}")

    # --- second island: capture sessions weakly linked to the first
    # cluster get their own reconstruction, merged later through
    # projector pixels both islands solve
    island1 = [i for i, v in enumerate(views) if v.registered]
    unreg = [i for i, v in enumerate(views) if not v.registered]
    views2 = None
    if len(unreg) >= 3:
        sub_obs = []
        track_grid2 = []
        for t in range(len(gx)):
            comp = [sidx for sidx in unreg if obs_ok[sidx, t] and any(
                agree[sidx, o, t] for o in unreg if o != sidx)]
            if len(comp) >= 2:
                sub_obs.append([(int(sx), obs_uv[sx, t, 0],
                                 obs_uv[sx, t, 1]) for sx in comp])
                track_grid2.append(t)
        print(f"island 2: {len(unreg)} views, {len(sub_obs)} tracks")
        views2 = [View(v.name, v.width, v.height) for v in views]
        for v, vv in zip(views, views2):
            vv.f = vv.f_prior = v.f_prior
            vv.f_sigma = v.f_sigma
            vv.intrinsic_group = v.intrinsic_group
        try:
            points2, valid2 = reconstruct(views2, sub_obs, verbose=True,
                                          ba_stride=3,
                                          min_init_parallax_deg=5.0)
            for v in views2:
                if v.registered:
                    print(f"  island2 {v.name}: f={v.f:.0f} "
                          f"k1={v.k1:.3f}")
        except RuntimeError as e:
            print("island 2 failed:", e)
            views2 = None

    # --- dense: triangulate every projector pixel from verified cameras ---
    step = 4
    dy, dx = np.mgrid[0:vh:step, 0:vw:step]
    dh, dw_ = dy.shape
    dyf, dxf = dy.ravel(), dx.ravel()
    npix = len(dyf)

    duv = np.zeros((n_scans, npix, 2))
    dok = np.zeros((n_scans, npix), bool)
    for s_idx, scan in enumerate(scans):
        cam_x, cam_y, conf = scan_data[scan]
        duv[s_idx, :, 0] = cam_x[dyf, dxf]
        duv[s_idx, :, 1] = cam_y[dyf, dxf]
        dok[s_idx] = conf[dyf, dxf] > CONF_THRESH

    def densify(view_objs):
        """Triangulate every projector pixel from this island's
        registered views, widest baselines first."""
        dense_xyz = np.zeros((npix, 3))
        dense_err = np.full(npix, np.inf)
        reg = [i for i in range(n_scans) if view_objs[i].registered]
        plist = [(i, j) for i, j in itertools.combinations(reg, 2)
                 if pair_F(i, j)[0] is not None]
        if not plist:
            return dense_xyz, dense_err
        centers = {i: view_objs[i].camera_center() for i in reg}
        bl = {(i, j): np.linalg.norm(centers[i] - centers[j])
              for i, j in plist}
        bmax = max(bl.values())
        plist = [ij for ij in plist if bl[ij] > 0.2 * bmax]
        plist.sort(key=lambda ij: -bl[ij])
        for i, j in plist:
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
                view_objs[i], view_objs[j], duv[i][good], duv[j][good])
            keep = err < 3.0
            gidx = good[keep]
            dense_xyz[gidx] = X[keep]
            dense_err[gidx] = err[keep]
        return dense_xyz, dense_err

    dense_xyz, dense_err = densify(views)
    solved = np.isfinite(dense_err)
    print(f"dense island 1: {solved.sum()}/{npix} pixels")

    if views2 is not None and any(v.registered for v in views2):
        xyz2, err2 = densify(views2)
        solved2 = np.isfinite(err2)
        print(f"dense island 2: {solved2.sum()}/{npix} pixels")

        # bridge cameras: island-1 views PnP'd into island-2's frame.
        # each bridge gives a rotation pair; two or more give scale
        # (center distance ratios) - together the frame2->frame1
        # similarity, without needing the islands' dense maps to overlap
        bridges = []
        for k in island1:
            obj, img = [], []
            for tp, t in enumerate(track_grid2):
                if valid2[tp] and obs_ok[k, t]:
                    obj.append(points2[tp])
                    img.append(obs_uv[k, t])
            if len(obj) < 30:
                print(f"bridge? {views[k].name}: only {len(obj)} "
                      f"candidate obs")
                continue
            obj = np.array(obj)
            img = np.array(img)
            vk = views[k]
            dist = np.array([vk.k1, 0, 0, 0])
            okp, rv, tv, inl = cv2.solvePnPRansac(
                obj, img, vk.K, dist, reprojectionError=15.0,
                iterationsCount=1000, flags=cv2.SOLVEPNP_SQPNP)
            n_inl = 0 if (not okp or inl is None) else len(inl)
            if n_inl < 30:
                print(f"bridge? {vk.name}: {len(obj)} candidates, "
                      f"{n_inl} PnP inliers - rejected")
                continue
            R2b = cv2.Rodrigues(rv)[0]
            C2b = (-R2b.T @ tv).ravel()
            bridges.append((k, vk.R, vk.camera_center(), R2b, C2b,
                            len(inl)))
            print(f"bridge {vk.name}: {len(inl)} PnP inliers in "
                  f"island-2 frame")
        if len(bridges) >= 2:
            Rs = [b[1].T @ b[3] for b in bridges]
            # average rotation via svd of summed matrices
            U, _, Vt = np.linalg.svd(np.sum(Rs, axis=0))
            R12 = (U @ Vt).T
            import itertools as it
            ratios = [np.linalg.norm(a[2] - b[2]) /
                      max(np.linalg.norm(a[4] - b[4]), 1e-12)
                      for a, b in it.combinations(bridges, 2)]
            s12 = float(np.median(ratios))
            T12 = np.mean([b[2] - s12 * R12 @ b[4] for b in bridges],
                          axis=0)
            M12 = np.eye(4)
            M12[:3, :3] = s12 * R12
            M12[:3, 3] = T12
            take = solved2 & ~solved
            dense_xyz[take] = apply_transform(M12, xyz2[take])
            dense_err[take] = err2[take]
            solved = np.isfinite(dense_err)
            print(f"merged via {len(bridges)} bridge cameras "
                  f"(scale {s12:.3f}): {solved.sum()}/{npix} pixels")
        else:
            print(f"only {len(bridges)} bridge cameras - cannot merge")
    print(f"dense done ({time.time()-t0:.0f}s)")

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
        os.path.join(HERE, 'lan', 'result.npz'),
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

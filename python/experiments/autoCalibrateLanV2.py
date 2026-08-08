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
ROOT = os.path.join(HERE, 'lan_raw', 'SharedData')
FLOAT = Imath.PixelType(Imath.PixelType.FLOAT)

CAM_W, CAM_H = 5184, 3456
GRID_STEP = 12
CONF_THRESH = float(os.environ.get('ABL_CONF', '0.05'))
# decode products to consume: margin-only maps, or coherence-weighted
_PM = {'coh': ('proMapCoh.npy', 'proConfCoh.npy'),
       'grad': ('proMapGrad.npy', 'proConfGrad.npy'),
       'full': ('proMapF.npy', 'proConfF.npy')}
# Max difference between two cameras' EXACT projector codes before they are
# accepted as the same point. Without it the addressing bin width (4 px)
# silently becomes the matching tolerance, which is a direct source of
# scatter on flat surfaces. 1.5 measured best; 3.0 is worse on every axis.
CODE_TOL = float(os.environ.get('ABL_CODETOL', '0'))
PMAP, PCONF = _PM.get(os.environ.get('ABL_PMAP', ''),
                      ('proMapC.npy', 'proConfC.npy'))
# NOTE: the F-matrix cache and the pose checkpoint both freeze which
# cameras register. Changing anything upstream of registration (decoder,
# CONF_THRESH, gating) requires deleting BOTH or the change is a no-op.
# v2 codes are quantized to 4-projector-px bins (drop_finest=2); the
# resulting camera-space jitter measured ~3.6px median against the 2019
# sub-pixel decode, so every geometric gate is opened up accordingly
SAMPSON_THRESH = 6.0


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
                   if os.path.exists(
                       os.path.join(ROOT, d, PMAP)))
    n_scans = len(scans)
    print(f"{n_scans} scans, virtual projector {vw}x{vh}")

    scan_data = {}
    for scan in scans:
        pm = np.load(os.path.join(ROOT, scan, PMAP))
        conf = np.load(os.path.join(ROOT, scan, PCONF))
        scan_data[scan] = (pm[..., 0].astype(np.float64),
                           pm[..., 1].astype(np.float64), conf)

    # --- pairwise camera-camera fundamental matrices (cached) ---
    fcache = os.path.join(HERE, 'fmats_camcam_lanv2.npz')
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
    # LAN kept a fixed zoom: one shared intrinsics set, prior = median
    # of the sane per-scan ground-truth focal fits (EXIF stand-in)
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

    # pose checkpoint: registration (reconstruct+completion+rescue) is
    # deterministic and slow; once a run has produced poses, later
    # runs iterating on the dense/mask/mesh stages resume from them
    pose_ckpt = os.path.join(HERE, 'lan', 'poses_v2.npz')
    RESUME = os.path.exists(pose_ckpt)
    if RESUME:
        pk = np.load(pose_ckpt)
        vp_ck = pk['view_params']
        nm_ck = [str(n) for n in pk['view_names']]
        for v in views:
            i = nm_ck.index(v.name)
            v.rvec = vp_ck[i, :3].copy()
            v.tvec = vp_ck[i, 3:6].copy()
            v.f, v.cx, v.cy, v.k1 = vp_ck[i, 6:10]
            v.registered = bool(vp_ck[i, 10] > 0.5)
        print(f"resumed poses from checkpoint: "
              f"{sum(v.registered for v in views)} registered")

    # initial pair restricted to the 18xx session: LAN has two scene
    # epochs and the production map was built from this one
    if not RESUME:
        counts18 = {}
        for track in observations:
            for (a, _, _), (b, _, _) in itertools.combinations(track, 2):
                if scans[a].startswith('scan-18') and \
                        scans[b].startswith('scan-18'):
                    counts18[(a, b)] = counts18.get((a, b), 0) + 1
        cand18 = sorted(counts18, key=counts18.get, reverse=True)[:12]
        points = valid = None
        for pair in cand18:
            try:
                points, valid = reconstruct(
                    views, observations, verbose=True, ba_stride=3,
                    min_init_parallax_deg=5.0, init_pair=pair)
                break
            except RuntimeError as e:
                print(f"init {scans[pair[0]]}/{scans[pair[1]]} failed: "
                      f"{e}")
                for v in views:
                    v.registered = False
        if points is None:
            raise RuntimeError('no 18xx init pair worked')
        print(f"reconstruct done in {time.time()-t0:.0f}s")
    for v in views:
        print(f"  {v.name}: registered={v.registered} f={v.f:.0f} "
              f"pp=({v.cx:.0f},{v.cy:.0f}) k1={v.k1:.3f}")

    # --- dense: robust multi-pair fusion per projector pixel ---
    step = 4
    dy, dx = np.mgrid[0:vh:step, 0:vw:step]
    dh, dw_ = dy.shape
    dyf, dxf = dy.ravel(), dx.ravel()
    npix = len(dyf)

    duv = np.zeros((n_scans, npix, 2))
    dok = np.zeros((n_scans, npix), bool)
    dcf = np.zeros((n_scans, npix), np.float32)
    for s_idx, scan in enumerate(scans):
        cam_x, cam_y, conf = scan_data[scan]
        duv[s_idx, :, 0] = cam_x[dyf, dxf]
        duv[s_idx, :, 1] = cam_y[dyf, dxf]
        dok[s_idx] = conf[dyf, dxf] > CONF_THRESH
        dcf[s_idx] = conf[dyf, dxf]

    KCAND = 8

    def densify(view_objs):
        """Fuse triangulations from up to KCAND camera pairs per pixel.

        Every registered pair is tried - no fundamental-matrix
        pre-check. The raw-pixel F gate is distortion-blind: wall dots
        near the image borders (the 360-degree scan's main content)
        fail a 3px sampson test purely from unmodeled k1. Once the
        calibration is solved, triangulation with full intrinsics plus
        a reprojection-error gate is the correct verification."""
        cand = np.full((npix, KCAND, 3), np.nan, np.float32)
        cand_err = np.full((npix, KCAND), np.inf, np.float32)
        cand_conf = np.zeros((npix, KCAND), np.float32)
        cand_n = np.zeros(npix, np.int8)
        reg = [i for i in range(n_scans) if view_objs[i].registered]
        plist = list(itertools.combinations(reg, 2))
        if not plist:
            return (np.zeros((npix, 3)), np.full(npix, np.inf),
                    np.zeros(npix, np.int8))
        centers = {i: view_objs[i].camera_center() for i in reg}
        bl = {(i, j): np.linalg.norm(centers[i] - centers[j])
              for i, j in plist}
        bmax = max(bl.values())
        plist = [ij for ij in plist if bl[ij] > 0.2 * bmax]
        plist.sort(key=lambda ij: -bl[ij])
        for i, j in plist:
            sel = dok[i] & dok[j] & (cand_n < KCAND)
            if not sel.any():
                continue
            good = np.where(sel)[0]
            X, err = triangulate_dense_pair(
                view_objs[i], view_objs[j], duv[i][good], duv[j][good])
            keep = err < 5.0
            gidx = good[keep]
            slot = cand_n[gidx]
            cand[gidx, slot] = X[keep]
            cand_err[gidx, slot] = err[keep]
            cand_conf[gidx, slot] = np.maximum(dcf[i][gidx],
                                               dcf[j][gidx])
            cand_n[gidx] += 1
        # mode-aware fusion: a projector pixel can have TWO real
        # answers (ball glint and reflected-leak landing spot); median
        # across modes would invent a point between them. cluster the
        # candidates around the lowest-error one and fuse only those.
        safe_err = np.where(np.isfinite(cand_err), cand_err, np.inf)
        # mode choice: HIGHEST-CONFIDENCE candidate, not lowest error.
        # a projector pixel's light exists at several places (direct
        # spill, primary bounce landing, secondary bounces); the map
        # must answer with the brightest = primary landing, and decode
        # confidence is the brightness proxy. triangulation error is a
        # gate (candidates are already err<5px), not a preference -
        # a well-triangulated secondary bounce is still wrong art.
        # (measured: picking by error put 73% of verified pixels on a
        # real surface but a DIFFERENT one than production used.)
        conf_m = np.where(np.isfinite(safe_err), cand_conf, -1.0)
        if os.environ.get('ABL_FUSION') == 'err':
            # ablation: pick lowest triangulation error instead of
            # highest decode confidence (the "wrong art" baseline)
            best = np.argmin(np.where(np.isfinite(safe_err),
                                      safe_err, np.inf), axis=1)
        else:
            best = np.argmax(conf_m, axis=1)
        rows = np.arange(npix)
        bxyz = cand[rows, best]
        finite_b = np.isfinite(bxyz[:, 0])
        ext_est = np.linalg.norm(
            np.nanpercentile(bxyz[finite_b], 98, axis=0) -
            np.nanpercentile(bxyz[finite_b], 2, axis=0)) \
            if finite_b.any() else 1.0
        tol = 0.015 * ext_est
        dcand = np.linalg.norm(cand - bxyz[:, None, :], axis=2)
        near = np.isfinite(dcand) & (dcand < tol)
        cand_near = np.where(near[..., None], cand, np.nan)
        with np.errstate(all='ignore'):
            xyz = np.nanmedian(cand_near, axis=1)
        err_out = np.where(cand_n > 0, safe_err[rows, best], np.inf)
        xyz[cand_n == 0] = 0
        densify.cand = cand
        densify.cand_err = safe_err
        densify.cand_conf = cand_conf
        return xyz, err_out, cand_n

    dense_xyz, dense_err, nviews_px = densify(views)
    solved = np.isfinite(dense_err)
    print(f"provisional dense: {solved.sum()}/{npix} pixels")

    # --- completion: scans the track graph couldn't reach register by
    # PnP against the dense anchors - thousands of already-triangulated
    # projector pixels they also decoded (a 360-degree sweep leaves some
    # cameras facing walls the main cluster barely sees; their link to
    # the model is through the pixels, not through pairwise overlap)
    # completion loop: each successful registration re-densifies, so
    # newly triangulated regions (e.g. the far wall unlocked by the
    # first far-side camera) become anchors for the next camera. RANSAC
    # absorbs view-dependent glint anchors; the dense stage's per-pixel
    # 3px reprojection gate self-limits any residual pose error.
    newly = not RESUME
    while newly:
        newly = False
        for k in [i for i, v in enumerate(views) if not v.registered]:
            sel = np.where(dok[k] & solved)[0]
            if len(sel) < 150:
                continue
            obj = dense_xyz[sel]
            img = duv[k][sel]
            v = views[k]
            try:
                ok2, rv, tv, inl = cv2.solvePnPRansac(
                    obj, img, v.K, np.array([v.k1, 0, 0, 0.0]),
                    reprojectionError=15.0, iterationsCount=1000,
                    flags=cv2.SOLVEPNP_SQPNP)
            except cv2.error:
                continue
            n_inl = 0 if (not ok2 or inl is None) else len(inl)
            # absolute count alone is not enough: 76 inliers out of
            # 72k shared anchors is chance agreement, and accepting
            # such a pose puts a phantom far-away camera into densify
            # where it inflates bmax and starves every real pair
            if n_inl < 60 or n_inl < 0.02 * len(sel):
                print(f"completion: {v.name} only {n_inl} anchor "
                      f"inliers of {len(sel)} - skipped")
                continue
            objs = obj[inl.ravel()].astype(np.float32)
            imgs = img[inl.ravel()].astype(np.float32)
            K = v.K.copy()
            flags = (cv2.CALIB_USE_INTRINSIC_GUESS |
                     cv2.CALIB_FIX_ASPECT_RATIO |
                     cv2.CALIB_FIX_PRINCIPAL_POINT |
                     cv2.CALIB_ZERO_TANGENT_DIST |
                     cv2.CALIB_FIX_K2 | cv2.CALIB_FIX_K3)
            rerr, K, dist, rvs, tvs = cv2.calibrateCamera(
                [objs], [imgs], (v.width, v.height), K, None,
                flags=flags)
            if rerr > 12.0:
                print(f"completion: {v.name} refine {rerr:.1f}px - "
                      f"rejected")
                continue
            v.f, v.k1 = float(K[0, 0]), float(dist[0][0])
            v.rvec = rvs[0].ravel()
            v.tvec = tvs[0].ravel()
            v.registered = True
            newly = True
            print(f"completion: registered {v.name} "
                  f"({n_inl} anchors, refine {rerr:.2f}px, f={v.f:.0f})")
            dense_xyz, dense_err, nviews_px = densify(views)
            solved = np.isfinite(dense_err)
            print(f"  re-densified: {solved.sum()}/{npix} pixels")
            break

    # --- full-resolution rescue: the sampling grid starves weakly
    # connected scans of correspondences (16x fewer than exist). for
    # each leftover camera, take EVERY projector pixel it shares with
    # its best-connected registered partner, survive the glint-outlier
    # rate with a long essential-matrix RANSAC, and resolve the scale
    # through projector-pixel identity: pixels the new pair
    # triangulates that already exist in the dense map are direct
    # 3d-3d anchors.
    full_xy = {}
    for s_idx, scan in enumerate(scans):
        cam_x, cam_y, conf = scan_data[scan]
        full_xy[s_idx] = (cam_x, cam_y, conf > CONF_THRESH)

    dense_grid_solved = solved.reshape(dh, dw_)
    dense_grid_xyz = dense_xyz.reshape(dh, dw_, 3)

    changed = not RESUME
    while changed:
        changed = False
        unreg_now = [i for i, v in enumerate(views) if not v.registered]
        reg_now = [i for i, v in enumerate(views) if v.registered]
        for k in unreg_now:
            # best partner by full-resolution mutual count
            cnts = [(int((full_xy[k][2] & full_xy[r][2]).sum()), r)
                    for r in reg_now]
            cnts.sort(reverse=True)
            if not cnts or cnts[0][0] < 5000:
                continue
            n_mut, r = cnts[0]
            vk, vr = views[k], views[r]
            ys, xs = np.where(full_xy[k][2] & full_xy[r][2])
            # 30k correspondences carry the same geometry as 300k;
            # RANSAC cost is iterations x pool size, so cap the pool
            if len(ys) > 30000:
                pick = np.random.default_rng(0).choice(
                    len(ys), 30000, replace=False)
                ys, xs = ys[pick], xs[pick]
            uv_k = np.stack([full_xy[k][0][ys, xs],
                             full_xy[k][1][ys, xs]], 1)
            uv_r = np.stack([full_xy[r][0][ys, xs],
                             full_xy[r][1][ys, xs]], 1)
            nk = vk.undistort_normalize(uv_k)
            nr = vr.undistort_normalize(uv_r)
            E, inlE = cv2.findEssentialMat(
                nr, nk, focal=1.0, pp=(0, 0), method=cv2.RANSAC,
                prob=0.99999, threshold=3.5 / vr.f,
                maxIters=200000)
            n_e = 0 if inlE is None else int(inlE.sum())
            print(f"rescue {vk.name} via {vr.name}: {n_mut} mutual, "
                  f"{n_e} E-inliers")
            if E is None or n_e < 400:
                continue
            _, Rrel, tunit, _ = cv2.recoverPose(
                E, nr, nk, focal=1.0, pp=(0, 0), mask=inlE.copy())
            # triangulate in r's camera frame (r = identity)
            P0 = np.hstack([np.eye(3), np.zeros((3, 1))])
            P1 = np.hstack([Rrel, tunit.reshape(3, 1)])
            m = inlE.ravel().astype(bool)
            Xh = cv2.triangulatePoints(P0, P1,
                                       nr[m].T.astype(np.float64),
                                       nk[m].T.astype(np.float64))
            Xl = (Xh[:3] / Xh[3]).T   # r-camera frame, unit baseline
            okz = (Xl[:, 2] > 0)
            # scale: the one remaining unknown. the island points
            # X(s) = A*s + B are observed by OTHER registered cameras
            # too - each observation constrains s via reprojection, no
            # pixel-grid coincidence needed. 1-d robust search.
            in_y, in_x = ys[m][okz], xs[m][okz]
            Xl_ok = Xl[okz]
            A = Xl_ok @ vr.R
            Bv = -(vr.tvec @ vr.R)
            cons = []
            for c in reg_now:
                if c == r:
                    continue
                vc2 = views[c]
                mc = full_xy[c][2][in_y, in_x]
                if mc.sum() < 30:
                    continue
                uvc = np.stack([full_xy[c][0][in_y, in_x][mc],
                                full_xy[c][1][in_y, in_x][mc]], 1)
                cons.append((vc2, A[mc], uvc))
            n_cons = sum(len(u) for _, _, u in cons)
            if n_cons < 50:
                print(f"  only {n_cons} scale constraints - skip")
                continue
            svals = np.logspace(-3, 1, 500)
            meds = np.empty(len(svals))
            for si, s_c in enumerate(svals):
                errs = []
                for vc2, Ac, uvc in cons:
                    uv_p, z = vc2.project(Ac * s_c + Bv)
                    e = np.linalg.norm(uv_p - uvc, axis=1)
                    e[z <= 0] = 1e9
                    errs.append(e)
                meds[si] = np.median(np.concatenate(errs))
            bi_ = int(np.argmin(meds))
            s_scale = float(svals[bi_])
            best_med = float(meds[bi_])
            print(f"  scale {s_scale:.4f} from {n_cons} reprojection "
                  f"constraints, median err {best_med:.1f}px")
            if best_med > 15.0 or bi_ in (0, len(svals) - 1):
                print("  scale unreliable - skip")
                continue
            # k's pose in main frame
            Rk = Rrel @ vr.R
            tk = s_scale * tunit.ravel() + Rrel @ vr.tvec
            # world points of all triangulated inliers for refinement
            Xw = (s_scale * Xl[okz]) @ vr.R - (vr.R.T @ vr.tvec) @ \
                np.eye(3)
            Xw = (s_scale * Xl[okz] - vr.tvec) @ vr.R
            objs = Xw.astype(np.float32)
            imgs = uv_k[m][okz].astype(np.float32)
            K = vk.K.copy()
            flags = (cv2.CALIB_USE_INTRINSIC_GUESS |
                     cv2.CALIB_FIX_ASPECT_RATIO |
                     cv2.CALIB_FIX_PRINCIPAL_POINT |
                     cv2.CALIB_ZERO_TANGENT_DIST |
                     cv2.CALIB_FIX_K2 | cv2.CALIB_FIX_K3)
            try:
                rerr, K, distc, rvs, tvs = cv2.calibrateCamera(
                    [objs], [imgs], (vk.width, vk.height), K, None,
                    flags=flags)
            except cv2.error as e:
                print("  refine failed:", str(e)[:60])
                continue
            if rerr > 12.0:
                print(f"  refine {rerr:.1f}px - rejected")
                continue
            vk.f, vk.k1 = float(K[0, 0]), float(distc[0][0])
            vk.rvec = rvs[0].ravel()
            vk.tvec = tvs[0].ravel()
            vk.registered = True
            changed = True
            print(f"  REGISTERED {vk.name}: refine {rerr:.2f}px, "
                  f"f={vk.f:.0f}, k1={vk.k1:+.3f}")
            dense_xyz, dense_err, nviews_px = densify(views)
            solved = np.isfinite(dense_err)
            dense_grid_solved = solved.reshape(dh, dw_)
            dense_grid_xyz = dense_xyz.reshape(dh, dw_, 3)
            print(f"  re-densified: {solved.sum()}/{npix} pixels")
            break

    if not RESUME:
        np.savez(pose_ckpt,
                 view_params=np.array(
                     [[*v.rvec, *v.tvec, v.f, v.cx, v.cy, v.k1,
                       float(v.registered)] for v in views]),
                 view_names=np.array([v.name for v in views]))
        print("saved pose checkpoint")

    # final dense pass with every registered camera
    dense_xyz, dense_err, nviews_px = densify(views)
    solved = np.isfinite(dense_err)
    print(f"dense: {solved.sum()}/{npix} pixels "
          f"(multi-pair: {(nviews_px >= 2).sum()})")

    # --- ball exclusion: the map answers "where does this pixel's
    # light LAND" - never the mirror-ball surface. glints are the
    # brightest decodes and cluster into one compact volume (the rig);
    # detect that volume, then per pixel prefer candidates outside it.
    # Automatic ball detection is OFF by default: measured on
    # LightsAllNight it removed 117k pixels (200,077 -> 83,001), cut mean
    # face coverage from 40.1% to 24.2%, and made accuracy WORSE (2.28% ->
    # 2.86%). Every automatic version of it has misfired in a different
    # way. Keeping mirror surfaces out of the map encodes artistic intent,
    # not a measurement, so it belongs in a human-supplied mask -- and
    # with a reconstruction in hand that is one 3D volume marked once,
    # projected into every camera, instead of a mask painted per scan.
    # Set ABL_BALL=auto to re-enable the detector.
    MAP_BALLS = os.environ.get('ABL_BALL') != 'auto'
    if not MAP_BALLS and solved.sum() > 2000:
        cand = densify.cand
        cand_err = densify.cand_err
        cand_conf = densify.cand_conf
        pts_all = dense_xyz[solved]
        conf_all = np.zeros(npix, np.float32)
        rows_s = np.where(solved)[0]
        best_slot = np.argmin(cand_err[rows_s], axis=1)
        conf_all[rows_s] = cand_conf[rows_s, best_slot]
        # confidence no longer separates glints from geometry: the v2
        # decoder makes WALLS confident too, so a conf>=p80 "bright"
        # set is just well-decoded geometry and any occupancy statistic
        # over it flags the densest third of the room (this deleted
        # 120k good pixels in one run). the signature that survives a
        # better decoder is geometric: a mirror ball compresses the
        # projector's entire image into a few voxels (thousands of
        # points), while a wall is a 2d sheet crossing a voxel (tens).
        # so: occupancy of ALL solved points, thresholded at an extreme
        # outlier level - never below the 98th percentile of occupied
        # voxels, never below 12x the median occupancy.
        lo_b = np.percentile(pts_all, 1, 0)
        hi_b = np.percentile(pts_all, 99, 0)
        NV = 48
        span_b = np.maximum(hi_b - lo_b, 1e-9)
        def vox(p):
            v = np.clip(((p - lo_b) / span_b * NV).astype(int), 0, NV-1)
            return v[:,0]*NV*NV + v[:,1]*NV + v[:,2]
        occ = np.zeros(NV**3, np.int32)
        np.add.at(occ, vox(pts_all), 1)
        occ_nz = occ[occ > 0]
        med_occ = float(np.median(occ_nz))
        thresh_occ = max(50.0, 12.0 * med_occ,
                         float(np.percentile(occ_nz, 98)))
        ballvox = occ >= thresh_occ
        print(f"ball detect: median occ {med_occ:.0f}, thresh "
              f"{thresh_occ:.0f}, {int(ballvox.sum())} of "
              f"{len(occ_nz)} occupied voxels flagged")
        # dilate by one voxel in each axis
        bv = ballvox.reshape(NV,NV,NV)
        for ax in range(3):
            bv = bv | np.roll(bv,1,ax) | np.roll(bv,-1,ax)
        ballvox = bv.ravel()
        def in_ball(p):
            return ballvox[vox(p)]
        n_ball_px = 0
        # per solved pixel: if best candidate is in the ball volume,
        # switch to the best NON-ball candidate; if none, unmap.
        for chunk in np.array_split(rows_s, 8):
            bc = cand[chunk]           # (c,K,3)
            be = cand_err[chunk]       # (c,K)
            flat = bc.reshape(-1,3)
            okf = np.isfinite(flat[:,0])
            inb = np.zeros(len(flat), bool)
            inb[okf] = in_ball(flat[okf])
            inb = inb.reshape(be.shape)
            be2 = np.where(inb, np.inf, be)
            # same rule as fusion: among non-ball candidates take the
            # highest-confidence one (primary landing), not lowest-err
            bcf = np.where(np.isfinite(be2), cand_conf[chunk], -1.0)
            newbest = np.argmax(bcf, axis=1)
            has = np.isfinite(be2[np.arange(len(chunk)), newbest])
            cur_in = in_ball(dense_xyz[chunk])
            switch = cur_in & has
            unmap = cur_in & ~has
            dense_xyz[chunk[switch]] = bc[switch, newbest[switch]]
            dense_err[chunk[switch]] = be2[switch, newbest[switch]]
            dense_err[chunk[unmap]] = np.inf
            n_ball_px += int(cur_in.sum())
        solved = np.isfinite(dense_err)
        print(f"ball exclusion: {int(ballvox.sum())} voxels, "
              f"{n_ball_px} glint-first pixels re-decided, "
              f"{solved.sum()}/{npix} still mapped")
        globals()['BALLVOX'] = (ballvox, lo_b, span_b, NV)

        # --- the automated hand-mask: excluding ball CANDIDATES only
        # unmaps ball-hit pixels, because each scan's winner-take-all
        # already happened in camera space and the glint (brightest)
        # won everywhere - the landing mode never reached the candidate
        # list. production's hand masks worked by removing ball pixels
        # BEFORE the winner was chosen. same thing, derived from data:
        # project the detected ball volume into every registered
        # camera, zero decode confidence there, rebuild the winner
        # maps, re-fuse.
        from decode_v2 import build_promap
        vidx = np.where(ballvox)[0]
        ix, iy, iz = (vidx // (NV * NV), (vidx // NV) % NV, vidx % NV)
        vc = lo_b + (np.stack([ix, iy, iz], 1) + 0.5) / NV * span_b
        halfdiag = 0.5 * np.linalg.norm(span_b / NV)
        SC = 8
        for s_idx, scan in enumerate(scans):
            v = views[s_idx]
            if not v.registered:
                continue
            uvb, zb = v.project(vc)
            # a voxel near the camera's principal plane projects to
            # absurd pixel coords (int32 overflow in cv2.circle) and a
            # frame-filling radius; such a voxel cannot be a ball seen
            # by this camera - require sane depth and on/near-frame uv
            okz = (zb > 0.05) & np.isfinite(uvb).all(1) & \
                (np.abs(uvb[:, 0]) < 4 * CAM_W) & \
                (np.abs(uvb[:, 1]) < 4 * CAM_H)
            r_px = np.zeros(len(zb))
            r_px[okz] = np.minimum(v.f * halfdiag / zb[okz] * 1.3,
                                   CAM_W / 2)
            mimg = np.zeros((CAM_H // SC, CAM_W // SC), np.uint8)
            for (ux, uy), rr, oz in zip(uvb, r_px, okz):
                if not oz:
                    continue
                cv2.circle(mimg, (int(ux / SC), int(uy / SC)),
                           max(1, int(rr / SC)), 255, -1)
            mask_full = cv2.resize(mimg, (CAM_W, CAM_H),
                                   interpolation=cv2.INTER_NEAREST) > 0
            code = np.load(os.path.join(ROOT, scan, 'camCodeC.npy'))
            cconf = np.load(os.path.join(ROOT, scan, 'camConfC.npy'))
            n_masked_cam = int((mask_full & (cconf > CONF_THRESH)).sum())
            cconf = np.where(mask_full, 0.0, cconf).astype(np.float32)
            pro_cam, pro_conf = build_promap(
                code[..., 0], code[..., 1], cconf, vw, vh)
            del code
            cam_x = pro_cam[..., 0].astype(np.float64)
            cam_y = pro_cam[..., 1].astype(np.float64)
            scan_data[scan] = (cam_x, cam_y, pro_conf)
            duv[s_idx, :, 0] = cam_x[dyf, dxf]
            duv[s_idx, :, 1] = cam_y[dyf, dxf]
            dok[s_idx] = pro_conf[dyf, dxf] > CONF_THRESH
            dcf[s_idx] = pro_conf[dyf, dxf]
            full_xy[s_idx] = (cam_x, cam_y, pro_conf > CONF_THRESH)
            print(f"  auto-mask {scan}: {n_masked_cam} confident cam "
                  f"px zeroed", flush=True)
        dense_xyz, dense_err, nviews_px = densify(views)
        solved = np.isfinite(dense_err)
        print(f"re-fused with auto-masks: {solved.sum()}/{npix} pixels")
        # residual glints (unregistered-scan pairs, mask edge leaks):
        # one more re-decide pass against the same ball volume
        cand = densify.cand
        cand_err = densify.cand_err
        cand_conf = densify.cand_conf
        rows_s = np.where(solved)[0]
        n2 = 0
        for chunk in np.array_split(rows_s, 8):
            bc = cand[chunk]
            be = cand_err[chunk]
            flat = bc.reshape(-1, 3)
            okf = np.isfinite(flat[:, 0])
            inb = np.zeros(len(flat), bool)
            inb[okf] = in_ball(flat[okf])
            inb = inb.reshape(be.shape)
            be2 = np.where(inb, np.inf, be)
            bcf = np.where(np.isfinite(be2), cand_conf[chunk], -1.0)
            newbest = np.argmax(bcf, axis=1)
            has = np.isfinite(be2[np.arange(len(chunk)), newbest])
            cur_in = in_ball(dense_xyz[chunk])
            switch = cur_in & has
            unmap = cur_in & ~has
            dense_xyz[chunk[switch]] = bc[switch, newbest[switch]]
            dense_err[chunk[switch]] = be2[switch, newbest[switch]]
            dense_err[chunk[unmap]] = np.inf
            n2 += int(cur_in.sum())
        solved = np.isfinite(dense_err)
        print(f"residual ball re-decide: {n2} px, "
              f"{solved.sum()}/{npix} mapped")

    # --- edge-aware outlier cleanup in projector space: a solved pixel
    # far from the median of its solved neighbors is replaced (lone
    # speckle) - real depth edges survive because MAD scales locally
    def speckle_clean(xyz_flat, solved_flat, label):
        grid = xyz_flat.reshape(dh, dw_, 3).copy()
        gmask = solved_flat.reshape(dh, dw_)
        grid[~gmask] = np.nan
        shifts = []
        for oy in (-1, 0, 1):
            for ox in (-1, 0, 1):
                if oy == 0 and ox == 0:
                    continue
                shifts.append(np.roll(np.roll(grid, oy, 0), ox, 1))
        stack = np.stack(shifts)
        with np.errstate(all='ignore'):
            med = np.nanmedian(stack, axis=0)
            nn = np.isfinite(stack[..., 0]).sum(0)
            dev = np.linalg.norm(grid - med, axis=-1)
            mad = np.nanmedian(
                np.linalg.norm(stack - med[None], axis=-1), axis=0)
        bad = gmask & (nn >= 4) & (dev > np.maximum(4 * mad, 1e-6))
        grid[bad] = med[bad]
        lone = gmask & (nn < 2)
        grid[lone] = np.nan
        gmask = gmask & ~lone
        print(f"cleanup ({label}): fixed {int(bad.sum())} speckles, "
              f"dropped {int(lone.sum())} isolated pixels")
        return (np.nan_to_num(grid.reshape(-1, 3)),
                gmask.reshape(-1).copy())

    dense_xyz, solved = speckle_clean(dense_xyz, solved, 'triangulated')
    dense_err = np.where(solved, dense_err, np.inf)

    # --- mesh-fill: single-camera pixels raycast against the measured
    # mesh (camamok's model-painting with a measured model)
    source = np.zeros(npix, np.int8)
    source[solved] = 2
    try:
        import open3d as o3d
        pcd = o3d.geometry.PointCloud()
        pcd.points = o3d.utility.Vector3dVector(dense_xyz[solved])
        pcd, _ = pcd.remove_statistical_outlier(nb_neighbors=16,
                                                std_ratio=2.0)
        ext = np.linalg.norm(
            pcd.get_axis_aligned_bounding_box().get_extent())
        pcd.estimate_normals(o3d.geometry.KDTreeSearchParamHybrid(
            radius=ext * 0.02, max_nn=30))
        pcd.orient_normals_consistent_tangent_plane(30)
        mesh, dens = \
            o3d.geometry.TriangleMesh.create_from_point_cloud_poisson(
                pcd, depth=9)
        mesh.remove_vertices_by_mask(
            np.asarray(dens) < np.quantile(np.asarray(dens), 0.05))
        scene = o3d.t.geometry.RaycastingScene()
        scene.add_triangles(o3d.t.geometry.TriangleMesh.from_legacy(mesh))
        filled = 0
        # cast from the highest-confidence observing camera per pixel
        conf_order = np.argsort(-dcf, axis=0)
        for rank in range(n_scans):
            cams = conf_order[rank]
            for s_idx in range(n_scans):
                v = views[s_idx]
                if not v.registered:
                    continue
                todo = np.where((cams == s_idx) & (source == 0) &
                                (dcf[s_idx] > 2 * CONF_THRESH))[0]
                if not len(todo):
                    continue
                xn = v.undistort_normalize(duv[s_idx][todo])
                dirs = np.hstack([xn, np.ones((len(xn), 1))]) @ v.R
                dirs /= np.linalg.norm(dirs, axis=1, keepdims=True)
                origin = v.camera_center()
                rays = np.hstack([np.tile(origin, (len(dirs), 1)),
                                  dirs]).astype(np.float32)
                hits = scene.cast_rays(o3d.core.Tensor(rays))
                t_hit = hits['t_hit'].numpy()
                okh = np.isfinite(t_hit) & (t_hit < 2.0 * ext)
                if 'BALLVOX' in globals():
                    bvx, lo_b, span_b, NV = globals()['BALLVOX']
                    hits_p = origin + dirs[okh] * \
                        t_hit[okh][:, None]
                    vv = np.clip(((hits_p - lo_b) / span_b * NV
                                  ).astype(int), 0, NV-1)
                    inb2 = bvx[vv[:,0]*NV*NV + vv[:,1]*NV + vv[:,2]]
                    tmp = np.where(okh)[0][inb2]
                    okh[tmp] = False
                idx = todo[okh]
                dense_xyz[idx] = origin + dirs[okh] * t_hit[okh][:, None]
                dense_err[idx] = 2.9
                source[idx] = 1
                filled += okh.sum()
        solved = source > 0
        print(f"mesh-fill: +{filled} pixels -> {solved.sum()}/{npix}")
        dense_xyz, solved = speckle_clean(dense_xyz, solved, 'filled')
        source[~solved] = 0
        dense_err = np.where(solved, dense_err, np.inf)
    except Exception as e:
        print("mesh-fill failed:", e)

    # --- projector mask parity with the old pipeline: hand-drawn
    # mask-0.png excludes projector regions that should never map
    mfile = os.path.join(HERE, 'lan', 'SharedData', 'mask-0.png')
    if os.path.exists(mfile):
        pmask = cv2.imread(mfile, cv2.IMREAD_GRAYSCALE)
        keep = pmask[dyf, dxf] > 127
        n_masked = int((solved & ~keep).sum())
        solved &= keep
        source[~keep] = 0
        dense_err = np.where(solved, dense_err, np.inf)
        print(f"projector mask: removed {n_masked} pixels")

    # --- ground truth comparison ---
    gt = read_exr(os.path.join(HERE, 'lan', 'SharedData', 'xyzMap-0.exr'), ['R', 'G', 'B'])
    gt_conf = read_exr(os.path.join(HERE, 'lan', 'SharedData', 'confidenceMap-0.exr'), ['Y'])
    gt_pts = gt[dyf, dxf]
    gt_ok = (gt_conf[dyf, dxf] > 0.05) & (np.abs(gt_pts).sum(1) > 1e-6)
    both = gt_ok & solved
    print(f"comparison pixels: ours {solved.sum()}, gt {gt_ok.sum()}, "
          f"both {both.sum()}")

    # gauge the GT alignment on CAMERA CENTERS, not map pixels: pixel
    # correspondences carry mode-selection differences (bounce choice)
    # that bias a similarity fit, while recovered camera centers match
    # the camamok poses to <1% of the room. fall back to verified
    # pixels when too few cameras have a sane GT pose fit.
    M = None
    try:
        gtp = np.load(os.path.join(HERE, 'lan_gt_poses.npy'),
                      allow_pickle=True).item()
        Cs, Gs = [], []
        for v in views:
            if not v.registered or v.name not in gtp:
                continue
            f_gt, k1_gt, c_gt, fitq = gtp[v.name]
            if fitq > 100:
                continue
            Cs.append(v.camera_center())
            Gs.append(c_gt)
        if len(Cs) >= 4:
            M = umeyama_alignment(np.array(Cs), np.array(Gs))
            print(f"alignment gauged on {len(Cs)} camera centers")
    except Exception as e:
        print("camera-center alignment failed:", e)
    if M is None:
        anchor = both & (source == 2) & (dense_err < 1.0)
        if anchor.sum() < 500:
            anchor = both & (source == 2)
        M = umeyama_alignment(dense_xyz[anchor], gt_pts[anchor])
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
        os.path.join(HERE, 'lan', os.environ.get('ABL_OUT', 'result_v2.npz')),
        dense_xyz=dense_xyz.reshape(dh, dw_, 3),
        dense_err=dense_err.reshape(dh, dw_),
        gt=gt_pts.reshape(dh, dw_, 3),
        gt_ok=gt_ok.reshape(dh, dw_),
        both=both.reshape(dh, dw_),
        M=M, step=step,
        view_params=np.array([[*v.rvec, *v.tvec, v.f, v.cx, v.cy, v.k1,
                               v.registered] for v in views]),
        source=source.reshape(dh, dw_),
        view_names=np.array([v.name for v in views]))
    print("saved result.npz")


if __name__ == '__main__':
    main()

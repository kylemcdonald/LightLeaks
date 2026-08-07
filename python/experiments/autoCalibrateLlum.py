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
CONF_THRESH = 0.05
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

    # --- island 2: reconstruct scans the main cluster couldn't reach
    island1 = [i for i, v in enumerate(views) if v.registered]
    unreg = [i for i, v in enumerate(views) if not v.registered]
    views2 = None
    if len(unreg) >= 2:
        sub_obs, track_grid2 = [], []
        for t in range(len(gx)):
            comp = [sx for sx in unreg if obs_ok[sx, t] and any(
                agree[sx, o, t] for o in unreg if o != sx)]
            if len(comp) >= 2:
                sub_obs.append([(int(sx), obs_uv[sx, t, 0],
                                 obs_uv[sx, t, 1]) for sx in comp])
                track_grid2.append(t)
        print(f"island 2: {len(unreg)} views, {len(sub_obs)} tracks")
        if len(sub_obs) >= 100:
            views2 = [View(v.name, v.width, v.height) for v in views]
            for v, vv in zip(views, views2):
                vv.f = vv.f_prior = v.f_prior
                vv.f_sigma = v.f_sigma
            try:
                points2, valid2 = reconstruct(
                    views2, sub_obs, verbose=True, ba_stride=3,
                    min_init_parallax_deg=5.0)
            except RuntimeError as e:
                print("island 2 failed:", e)
                views2 = None

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

    KCAND = 6

    def densify(view_objs):
        """Fuse triangulations from up to KCAND camera pairs per pixel:
        component-wise median beats best-single-pair on noise and kills
        lone bad pairs."""
        cand = np.full((npix, KCAND, 3), np.nan, np.float32)
        cand_err = np.full((npix, KCAND), np.inf, np.float32)
        cand_n = np.zeros(npix, np.int8)
        reg = [i for i in range(n_scans) if view_objs[i].registered]
        plist = [(i, j) for i, j in itertools.combinations(reg, 2)
                 if pair_F(i, j)[0] is not None]
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
            F, flipped = pair_F(i, j)
            sel = dok[i] & dok[j] & (cand_n < KCAND)
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
            slot = cand_n[gidx]
            cand[gidx, slot] = X[keep]
            cand_err[gidx, slot] = err[keep]
            cand_n[gidx] += 1
        xyz = np.nanmedian(cand, axis=1)
        err_out = np.where(cand_n > 0, np.nanmin(
            np.where(np.isfinite(cand_err), cand_err, np.nan), axis=1),
            np.inf)
        xyz[cand_n == 0] = 0
        return xyz, err_out, cand_n

    dense_xyz, dense_err, nviews_px = densify(views)
    solved = np.isfinite(dense_err)
    print(f"dense island 1: {solved.sum()}/{npix} pixels "
          f"(multi-pair: {(nviews_px >= 2).sum()})")

    if views2 is not None and any(v.registered for v in views2):
        xyz2, err2, nv2 = densify(views2)
        solved2 = np.isfinite(err2)
        print(f"dense island 2: {solved2.sum()}/{npix} pixels")
        # bridge cameras: island-1 views PnP'd into island-2's frame
        bridges = []
        for k in island1:
            obj, img = [], []
            for tp, t in enumerate(track_grid2):
                if valid2[tp] and obs_ok[k, t]:
                    obj.append(points2[tp])
                    img.append(obs_uv[k, t])
            if len(obj) < 30:
                continue
            obj, img = np.array(obj), np.array(img)
            vk = views[k]
            dist = np.array([vk.k1, 0, 0, 0])
            okp, rv, tv, inl = cv2.solvePnPRansac(
                obj, img, vk.K, dist, reprojectionError=15.0,
                iterationsCount=1000, flags=cv2.SOLVEPNP_SQPNP)
            n_inl = 0 if (not okp or inl is None) else len(inl)
            if n_inl < 30:
                continue
            R2b = cv2.Rodrigues(rv)[0]
            bridges.append((vk.R, vk.camera_center(), R2b,
                            (-R2b.T @ tv).ravel()))
            print(f"bridge {vk.name}: {n_inl} inliers")
        if len(bridges) >= 2:
            Rs = [b[0].T @ b[2] for b in bridges]
            U, _, Vt = np.linalg.svd(np.sum(Rs, axis=0))
            R12 = (U @ Vt).T
            ratios = [np.linalg.norm(a[1] - b[1]) /
                      max(np.linalg.norm(a[3] - b[3]), 1e-12)
                      for a, b in itertools.combinations(bridges, 2)]
            s12 = float(np.median(ratios))
            T12 = np.mean([b[1] - s12 * R12 @ b[3] for b in bridges], 0)
            take = solved2 & ~solved
            dense_xyz[take] = xyz2[take] @ (s12 * R12).T + T12
            dense_err[take] = err2[take]
            solved = np.isfinite(dense_err)
            print(f"merged islands via {len(bridges)} bridges: "
                  f"{solved.sum()}/{npix}")
        else:
            print(f"only {len(bridges)} bridges - islands kept separate")

    # --- edge-aware outlier cleanup in projector space: a solved pixel
    # far from the median of its solved neighbors is replaced (lone
    # speckle) - real depth edges survive because MAD scales locally
    grid = dense_xyz.reshape(dh, dw_, 3).copy()
    gmask = solved.reshape(dh, dw_)
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
        mad = np.nanmedian(np.linalg.norm(stack - med[None], axis=-1),
                           axis=0)
    bad = gmask & (nn >= 4) & (dev > np.maximum(4 * mad, 1e-6))
    grid[bad] = med[bad]
    n_fixed = int(bad.sum())
    lone = gmask & (nn < 2)
    grid[lone] = np.nan
    gmask = gmask & ~lone
    dense_xyz = np.nan_to_num(grid.reshape(-1, 3))
    solved = gmask.reshape(-1)
    dense_err = np.where(solved, dense_err, np.inf)
    print(f"cleanup: fixed {n_fixed} speckles, dropped {lone.sum()} "
          f"isolated pixels")

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
                todo = np.where((cams == s_idx) & dok[s_idx] &
                                (source == 0))[0]
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
                idx = todo[okh]
                dense_xyz[idx] = origin + dirs[okh] * t_hit[okh][:, None]
                dense_err[idx] = 2.9
                source[idx] = 1
                filled += okh.sum()
        solved = source > 0
        print(f"mesh-fill: +{filled} pixels -> {solved.sum()}/{npix}")
    except Exception as e:
        print("mesh-fill failed:", e)

    # --- ground truth comparison ---
    gt = read_exr(os.path.join(ROOT, 'xyzMap-0.exr'), ['R', 'G', 'B'])
    gt_conf = read_exr(os.path.join(ROOT, 'confidenceMap-0.exr'), ['Y'])
    gt_pts = gt[dyf, dxf]
    gt_ok = (gt_conf[dyf, dxf] > 0.05) & (np.abs(gt_pts).sum(1) > 1e-6)
    both = gt_ok & solved
    print(f"comparison pixels: ours {solved.sum()}, gt {gt_ok.sum()}, "
          f"both {both.sum()}")

    # anchor the GT alignment on verified triangulations only -
    # mesh-filled and loose pixels would bias the similarity fit
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
        os.path.join(HERE, 'llum', 'result.npz'),
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

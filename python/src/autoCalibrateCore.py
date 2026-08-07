"""Model-free procam calibration from structured light correspondences.

Treats each projector (and each camera/scan position) as a pinhole view.
Gray-code decoded correspondences link them: a projector pixel seen by
multiple scans is a multi-view track. From these tracks we recover all
poses and intrinsics with an incremental SfM + bundle adjustment, then
triangulate a dense per-projector-pixel XYZ map — no 3d model needed.
"""
import numpy as np
import cv2
from scipy.optimize import least_squares
from scipy.sparse import lil_matrix

# Per-view parameter vector in bundle adjustment:
# [rvec(3), tvec(3), f, cx, cy, k1]
N_VIEW_PARAMS = 10


class View:
    def __init__(self, name, width, height, fov_deg=60.0, is_projector=False,
                 principal_point=None):
        self.name = name
        self.width = width
        self.height = height
        f = (width / 2) / np.tan(np.radians(fov_deg) / 2)
        self.f = f
        if principal_point is None:
            principal_point = (width / 2, height / 2)
        self.cx, self.cy = principal_point
        self.k1 = 0.0
        # priors for bundle adjustment regularization: intrinsics are
        # pulled weakly toward these (initial guess = EXIF focal or
        # projector throw ratio); sigma is the tolerated deviation
        self.f_prior, self.f_sigma = f, 0.25 * f
        self.cx_prior, self.cx_sigma = self.cx, 0.05 * width
        if is_projector:
            # projector lens shift: principal point can sit far outside
            # the image vertically, so constrain it only loosely
            self.cy_prior, self.cy_sigma = self.cy, 0.75 * height
        else:
            self.cy_prior, self.cy_sigma = self.cy, 0.05 * height
        self.k1_prior, self.k1_sigma = 0.0, 0.1
        self.rvec = np.zeros(3)
        self.tvec = np.zeros(3)
        self.is_projector = is_projector
        # views sharing an intrinsic_group string are constrained to
        # identical intrinsics in bundle adjustment (e.g. all scan
        # positions shot with the same physical camera)
        self.intrinsic_group = None
        self.registered = False

    @property
    def K(self):
        return np.array([[self.f, 0, self.cx],
                         [0, self.f, self.cy],
                         [0, 0, 1.0]])

    @property
    def R(self):
        return cv2.Rodrigues(self.rvec)[0]

    def camera_center(self):
        return -self.R.T @ self.tvec

    def projection_matrix(self):
        return self.K @ np.hstack([self.R, self.tvec.reshape(3, 1)])

    def project(self, points):
        """points: (N,3) world -> (N,2) pixels, plus (N,) depth"""
        pc = points @ self.R.T + self.tvec
        z = pc[:, 2]
        xy = pc[:, :2] / z[:, None]
        r2 = (xy ** 2).sum(1)
        xy = xy * (1 + self.k1 * r2)[:, None]
        uv = xy * self.f + [self.cx, self.cy]
        return uv, z

    def undistort_normalize(self, uv):
        """pixels (N,2) -> normalized image coords, undistorting k1"""
        pts = uv.reshape(-1, 1, 2).astype(np.float64)
        dist = np.array([self.k1, 0, 0, 0], np.float64)
        out = cv2.undistortPoints(pts, self.K, dist)
        return out.reshape(-1, 2)


def project_params(points, rvec, tvec, f, cx, cy, k1):
    """Vectorized projection used inside bundle adjustment residuals."""
    R = cv2.Rodrigues(rvec)[0]
    pc = points @ R.T + tvec
    xy = pc[:, :2] / pc[:, 2:3]
    r2 = (xy ** 2).sum(1)
    xy = xy * (1 + k1 * r2)[:, None]
    return xy * f + [cx, cy]


def triangulate_tracks(views, observations, reproj_thresh=20.0):
    """Multi-view linear (DLT) triangulation.

    observations: list of tracks, each a list of (view_index, u, v).
    Returns (N,3) points and (N,) bool validity (enough registered views,
    positive depth and residual below reproj_thresh in all of them).
    """
    n = len(observations)
    points = np.zeros((n, 3))
    valid = np.zeros(n, dtype=bool)
    pmats = [v.projection_matrix() if v.registered else None for v in views]
    for i, track in enumerate(observations):
        rows = []
        for view_idx, u, v in track:
            if pmats[view_idx] is None:
                continue
            # undistort the observation into ideal pinhole pixels
            view = views[view_idx]
            xn = view.undistort_normalize(np.array([[u, v]]))[0]
            uu = xn[0] * view.f + view.cx
            vv = xn[1] * view.f + view.cy
            P = pmats[view_idx]
            rows.append(uu * P[2] - P[0])
            rows.append(vv * P[2] - P[1])
        if len(rows) < 4:
            continue
        A = np.array(rows)
        _, _, vt = np.linalg.svd(A)
        X = vt[-1]
        if abs(X[3]) < 1e-12:
            continue
        X = X[:3] / X[3]
        ok = True
        for view_idx, u, v in track:
            if pmats[view_idx] is None:
                continue
            uv, z = views[view_idx].project(X[None])
            if z[0] <= 0 or \
                    np.linalg.norm(uv[0] - [u, v]) > reproj_thresh:
                ok = False
                break
        points[i] = X
        valid[i] = ok
    return points, valid


def prune_observations(views, observations, points, valid, thresh=3.0):
    """Drop observations whose residual exceeds thresh (in-place).

    Tracks left with fewer than 2 observations are no longer
    triangulable and will drop out at the next triangulate_tracks call.
    Returns the number of observations removed.
    """
    removed = 0
    for t_idx, track in enumerate(observations):
        if not valid[t_idx]:
            continue
        kept = []
        for view_idx, u, v in track:
            view = views[view_idx]
            if view.registered:
                uv, z = view.project(points[t_idx][None])
                if z[0] <= 0 or \
                        np.linalg.norm(uv[0] - [u, v]) > thresh:
                    removed += 1
                    continue
            kept.append((view_idx, u, v))
        track[:] = kept
    return removed


def initialize_pair(views, observations, i, j):
    """Relative pose of views i,j from their shared tracks (essential matrix)."""
    pi, pj = [], []
    for track in observations:
        d = dict((vi, (u, v)) for vi, u, v in track)
        if i in d and j in d:
            pi.append(d[i])
            pj.append(d[j])
    pi = np.array(pi, np.float64)
    pj = np.array(pj, np.float64)
    ni = views[i].undistort_normalize(pi)
    nj = views[j].undistort_normalize(pj)
    E, inliers = cv2.findEssentialMat(
        ni, nj, focal=1.0, pp=(0, 0), method=cv2.RANSAC,
        prob=0.9999, threshold=1.5 / views[i].f)
    _, R, t, _ = cv2.recoverPose(E, ni, nj, focal=1.0, pp=(0, 0),
                                 mask=inliers.copy())
    views[i].rvec = np.zeros(3)
    views[i].tvec = np.zeros(3)
    views[i].registered = True
    views[j].rvec = cv2.Rodrigues(R)[0].ravel()
    views[j].tvec = t.ravel()
    views[j].registered = True
    return int(inliers.sum())


def register_view_pnp(view, points, valid, observations, view_idx):
    """Register one view against already-triangulated tracks with PnP."""
    obj, img = [], []
    for t_idx, track in enumerate(observations):
        if not valid[t_idx]:
            continue
        for vi, u, v in track:
            if vi == view_idx:
                obj.append(points[t_idx])
                img.append((u, v))
    if len(obj) < 6:
        return 0
    obj = np.array(obj, np.float64)
    img = np.array(img, np.float64)
    dist = np.array([view.k1, 0, 0, 0], np.float64)
    # generous threshold first: intrinsics are still a rough guess and a
    # wrong focal shows up as large-but-systematic reprojection error
    thresh = max(8.0, 0.02 * view.width)
    tight = max(6.0, 0.002 * view.width)

    # try several solvers: near-planar point sets (a camera that mostly
    # sees the floor) have a two-fold pose ambiguity that trips up
    # individual PnP methods; keep whichever pose verifies best.
    # every attempt starts from (and on failure restores) the entry state:
    # refine_single_view mutates the view, and a corrupted focal from a
    # failed attempt must not leak into later attempts or later calls.
    entry = (view.rvec.copy(), view.tvec.copy(), view.f, view.cx, view.cy)
    best_good, best_state = None, None
    for flags in (cv2.SOLVEPNP_SQPNP, cv2.SOLVEPNP_ITERATIVE,
                  cv2.SOLVEPNP_EPNP):
        view.rvec, view.tvec = entry[0].copy(), entry[1].copy()
        view.f, view.cx, view.cy = entry[2:5]
        try:
            ok, rvec, tvec, inl = cv2.solvePnPRansac(
                obj, img, view.K, dist, reprojectionError=thresh,
                iterationsCount=500, flags=flags)
        except cv2.error:
            continue
        if not ok or inl is None or len(inl) < 6:
            continue
        view.rvec = rvec.ravel()
        view.tvec = tvec.ravel()
        refine_single_view(view, obj[inl.ravel()], img[inl.ravel()])
        # verify: a correct pose over an imprecise cloud has residuals on
        # the order of the local cloud error, so scale the acceptance
        # threshold to the refined inliers' median residual — but cap it,
        # so a catastrophically wrong pose (badly-conditioned PnP that
        # passed loose RANSAC) can never verify itself
        uv, z = view.project(obj)
        err = np.linalg.norm(uv - img, axis=1)
        med = np.median(err[inl.ravel()])
        scale = np.clip(3.0 * med, tight, 0.01 * view.width)
        good = (err < scale) & (z > 0)
        if good.sum() < max(12, 0.6 * len(inl)):
            continue
        if best_good is None or good.sum() > best_good.sum():
            best_good = good
            best_state = (view.rvec.copy(), view.tvec.copy(),
                          view.f, view.cx, view.cy)
    if best_good is None or best_good.sum() < 12:
        view.rvec, view.tvec = entry[0], entry[1]
        view.f, view.cx, view.cy = entry[2:5]
        return 0
    view.rvec, view.tvec = best_state[0], best_state[1]
    view.f, view.cx, view.cy = best_state[2:5]
    refine_single_view(view, obj[best_good], img[best_good])
    view.registered = True
    return int(best_good.sum())


def refine_single_view(view, obj, img):
    """Refine one view's pose and focal length against known 3d points.

    For cameras, principal point and distortion stay fixed here: freeing
    them against a still-converging cloud lets the view "explain away"
    structure errors. Projectors additionally free their principal point —
    lens shift puts it far from the image center (often near or beyond
    the image edge) and no pose can compensate the residual nonlinearity.
    """
    free_pp = view.is_projector

    def residuals(x):
        cx, cy = (x[7], x[8]) if free_pp else (view.cx, view.cy)
        uv = project_params(obj, x[0:3], x[3:6], x[6], cx, cy, view.k1)
        return (uv - img).ravel()

    x0 = np.concatenate([view.rvec, view.tvec, [view.f]] +
                        ([[view.cx], [view.cy]] if free_pp else []))
    result = least_squares(residuals, x0, method='trf', loss='soft_l1',
                           f_scale=2.0, x_scale='jac', max_nfev=100)
    x = result.x
    view.rvec, view.tvec = x[0:3].copy(), x[3:6].copy()
    view.f = x[6]
    if free_pp:
        view.cx, view.cy = x[7], x[8]


def bundle_adjust(views, observations, points, valid,
                  fix_first_pose=True, intrinsics='none',
                  loss='soft_l1', f_scale=2.0, verbose=0, max_nfev=200):
    """Joint refinement of all registered view params + track points.

    Views' rvec/tvec/f/cx/cy/k1 and the 3d points are updated in place.
    Returns final RMS reprojection error over inlier observations (pixels).

    Views sharing a non-None view.intrinsic_group are optimized with a
    single shared set of intrinsics - the strongest constraint available
    when one physical camera shot several scan positions.

    intrinsics: which intrinsics to optimize alongside poses+points.
      'none' - poses/points only (use while few views are registered:
               full self-calibration from 2 views is under-constrained)
      'f'    - also focal lengths
      'fk'   - focal lengths + radial distortion (principal point fixed)
      'full' - also principal points (only with many views)
    """
    reg = [i for i, v in enumerate(views) if v.registered]
    reg_pos = {vi: p for p, vi in enumerate(reg)}
    track_ids = np.where(valid)[0]
    track_pos = {t: p for p, t in enumerate(track_ids)}

    # intrinsic groups over registered views
    groups = []           # group -> list of view positions
    group_of_view = []    # view position -> group index
    key_to_group = {}
    for p, vi in enumerate(reg):
        key = views[vi].intrinsic_group
        if key is None:
            key = f"__solo_{vi}"
        if key not in key_to_group:
            key_to_group[key] = len(groups)
            groups.append([])
        g = key_to_group[key]
        groups[g].append(p)
        group_of_view.append(g)

    obs = []  # (view_pos, track_pos, u, v)
    for t_idx in track_ids:
        for vi, u, v in observations[t_idx]:
            if vi in reg_pos:
                obs.append((reg_pos[vi], track_pos[t_idx], u, v))
    obs_view = np.array([o[0] for o in obs])
    obs_track = np.array([o[1] for o in obs])
    obs_uv = np.array([[o[2], o[3]] for o in obs])

    n_views = len(reg)
    n_groups = len(groups)
    pose_size = n_views * 6
    intr_offset = pose_size
    pt_offset = intr_offset + n_groups * 4

    # initial values; group intrinsics = mean over members (they can
    # drift apart slightly during per-view PnP refinement)
    x0_pose = np.concatenate(
        [np.concatenate([views[vi].rvec, views[vi].tvec]) for vi in reg])
    x0_intr = []
    for members in groups:
        vs = [views[reg[p]] for p in members]
        x0_intr += [np.mean([v.f for v in vs]),
                    np.mean([v.cx for v in vs]),
                    np.mean([v.cy for v in vs]),
                    np.mean([v.k1 for v in vs])]
    x0 = np.concatenate([x0_pose, np.array(x0_intr),
                         points[track_ids].ravel()])

    # mask of parameters we actually optimize
    free = np.ones_like(x0, dtype=bool)
    if fix_first_pose:
        free[0:6] = False
        # fix gauge scale: freeze one translation component of second view
        if n_views > 1:
            free[6 + 5] = False
    for g in range(n_groups):
        base = intr_offset + g * 4
        if intrinsics == 'none':
            free[base: base + 4] = False
        elif intrinsics == 'f':
            free[base + 1: base + 4] = False
        elif intrinsics == 'fk':
            free[base + 1: base + 3] = False
    free_idx = np.where(free)[0]

    # sanity bounds keep intrinsics physical (prevents focal blow-ups in
    # weakly-constrained configurations)
    lo = np.full_like(x0, -np.inf)
    hi = np.full_like(x0, np.inf)
    for g, members in enumerate(groups):
        v = views[reg[members[0]]]
        base = intr_offset + g * 4
        lo[base + 0], hi[base + 0] = 0.2 * v.width, 20.0 * v.width
        lo[base + 1], hi[base + 1] = 0.0, v.width
        lo[base + 2], hi[base + 2] = -v.height, 2.0 * v.height
        lo[base + 3], hi[base + 3] = -0.5, 0.5
    x0 = np.clip(x0, lo + 1e-9, hi - 1e-9)

    def unpack(xfree):
        x = x0.copy()
        x[free_idx] = xfree
        return x

    # intrinsic priors: without them, thousands of tiny per-observation
    # gains can drag f/pp/k1 to physically absurd values that happen to
    # fit the current (noisy, partially converged) structure. the weight
    # grows sublinearly with the group's observation count: strong enough
    # to anchor a weakly-observed view, weak enough that dense data can
    # pull an intrinsic several sigma from a bad initial guess.
    prior_rows = []  # (param_index, prior, sigma, weight)
    for g, members in enumerate(groups):
        v = views[reg[members[0]]]
        base = intr_offset + g * 4
        n_obs_g = sum((obs_view == p).sum() for p in members)
        w = max(1.0, n_obs_g) ** 0.25
        for off, prior, sigma in (
                (0, v.f_prior, v.f_sigma), (1, v.cx_prior, v.cx_sigma),
                (2, v.cy_prior, v.cy_sigma), (3, v.k1_prior, v.k1_sigma)):
            if free[base + off]:
                prior_rows.append((base + off, prior, sigma, w))

    def residuals(xfree):
        x = unpack(xfree)
        res = np.empty((len(obs), 2))
        pts = x[pt_offset:].reshape(-1, 3)
        for p in range(n_views):
            sel = obs_view == p
            if not sel.any():
                continue
            pb = p * 6
            ib = intr_offset + group_of_view[p] * 4
            uv = project_params(
                pts[obs_track[sel]],
                x[pb:pb + 3], x[pb + 3:pb + 6],
                x[ib], x[ib + 1], x[ib + 2], x[ib + 3])
            res[sel] = uv - obs_uv[sel]
        prior = np.array([w * (x[idx] - pr) / sg
                          for idx, pr, sg, w in prior_rows])
        return np.concatenate([res.ravel(), prior])

    # sparsity pattern
    S = lil_matrix((len(obs) * 2 + len(prior_rows), len(free_idx)),
                   dtype=int)
    col_of = -np.ones(len(x0), dtype=int)
    col_of[free_idx] = np.arange(len(free_idx))
    for o_idx in range(len(obs)):
        pb = obs_view[o_idx] * 6
        ib = intr_offset + group_of_view[obs_view[o_idx]] * 4
        ptb = pt_offset + obs_track[o_idx] * 3
        for k in list(range(pb, pb + 6)) + list(range(ib, ib + 4)) + \
                 list(range(ptb, ptb + 3)):
            c = col_of[k]
            if c >= 0:
                S[o_idx * 2, c] = 1
                S[o_idx * 2 + 1, c] = 1
    for r, (idx, _, _, _) in enumerate(prior_rows):
        S[len(obs) * 2 + r, col_of[idx]] = 1

    result = least_squares(
        residuals, x0[free_idx], jac_sparsity=S, method='trf',
        bounds=(lo[free_idx], hi[free_idx]),
        loss=loss, f_scale=f_scale, x_scale='jac',
        max_nfev=max_nfev, verbose=verbose)

    x = unpack(result.x)
    for p, vi in enumerate(reg):
        v = views[vi]
        pb = p * 6
        ib = intr_offset + group_of_view[p] * 4
        v.rvec = x[pb:pb + 3].copy()
        v.tvec = x[pb + 3:pb + 6].copy()
        v.f, v.cx, v.cy, v.k1 = x[ib:ib + 4]
    points[track_ids] = x[pt_offset:].reshape(-1, 3)

    res = residuals(result.x)[:len(obs) * 2].reshape(-1, 2)
    err = np.linalg.norm(res, axis=1)
    return np.sqrt(np.mean(np.minimum(err, 10.0) ** 2))


def median_parallax_deg(views, points, valid, i, j):
    """Median triangulation angle (degrees) of valid points seen by
    views i and j - near zero for a rotation-only / same-tripod pair,
    whose triangulated depths are meaningless."""
    ci = views[i].camera_center()
    cj = views[j].camera_center()
    pts = points[valid]
    if len(pts) == 0:
        return 0.0
    v1 = pts - ci
    v2 = pts - cj
    cosang = np.einsum('ij,ij->i', v1, v2) / (
        np.linalg.norm(v1, axis=1) * np.linalg.norm(v2, axis=1) + 1e-12)
    return float(np.degrees(np.arccos(np.clip(np.abs(cosang), -1, 1))
                            ).mean())


def reconstruct(views, observations, init_pair=None, verbose=False,
                ba_every_registration=True, ba_stride=1,
                min_init_parallax_deg=2.0):
    """Full incremental pipeline: init pair -> PnP registration -> global BA.

    ba_stride: run the (expensive) global BA only every N successful
    registrations - between BAs a new view still gets its PnP+refine.

    Returns (points, valid) for the sparse tracks.
    """
    n_views = len(views)
    regs_since_ba = 0
    counts = np.zeros((n_views, n_views), dtype=int)
    for track in observations:
        vis = [vi for vi, _, _ in track]
        for a in range(len(vis)):
            for b in range(a + 1, len(vis)):
                counts[vis[a], vis[b]] += 1
                counts[vis[b], vis[a]] += 1
    if init_pair is not None:
        candidates = [tuple(init_pair)]
    else:
        pairs = [(i, j) for i in range(n_views) for j in range(i + 1,
                 n_views) if counts[i, j] >= 50]
        candidates = sorted(pairs, key=lambda ij: -counts[ij])[:20]

    # try candidate pairs until one yields real parallax: the pair with
    # the most shared tracks is often two shots from the same tripod
    # spot, whose essential-matrix cloud has meaningless depths
    chosen = None
    for i, j in candidates:
        for v in views:
            v.registered = False
        n_inl = initialize_pair(views, observations, i, j)
        points, valid = triangulate_tracks(views, observations,
                                           reproj_thresh=50.0)
        par = median_parallax_deg(views, points, valid, i, j)
        if verbose:
            print(f"init pair ({views[i].name}, {views[j].name}): "
                  f"{n_inl} E-inliers, {valid.sum()} tracks, "
                  f"parallax {par:.1f} deg")
        if par >= min_init_parallax_deg and valid.sum() >= 50:
            chosen = (i, j)
            break
    if chosen is None:
        raise RuntimeError("no init pair with sufficient parallax")
    i, j = chosen

    rms = bundle_adjust(views, observations, points, valid,
                        intrinsics='none')
    if verbose:
        print(f"  after pair BA: rms {rms:.2f}px, "
              f"{valid.sum()} tracks")

    while True:
        unreg = [k for k in range(n_views) if not views[k].registered]
        if not unreg:
            break
        # candidate order: views with most observations of valid tracks;
        # try each until one registers (registering it may unlock others)
        counts = []
        for k in unreg:
            c = sum(1 for t_idx, track in enumerate(observations)
                    if valid[t_idx] and any(vi == k for vi, _, _ in track))
            counts.append((c, k))
        counts.sort(reverse=True)
        registered_one = False
        for c, k in counts:
            if c < 6:
                break
            state = [(v.rvec.copy(), v.tvec.copy(), v.f, v.cx, v.cy, v.k1)
                     for v in views]
            n_inl = register_view_pnp(views[k], points, valid,
                                      observations, k)
            # a weakly-supported pose does more harm than good: defer this
            # view, it may gain support once other views are registered
            if n_inl < max(12, 0.1 * c):
                views[k].registered = False
                if verbose:
                    print(f"deferring {views[k].name}: only {n_inl} verified "
                          f"PnP inliers of {c} candidate tracks")
                continue
            new_points, new_valid = triangulate_tracks(views, observations,
                                                       reproj_thresh=50.0)
            do_ba = ba_every_registration and \
                (regs_since_ba + 1 >= ba_stride)
            if do_ba:
                n_reg = sum(1 for v in views if v.registered)
                new_rms = bundle_adjust(
                    views, observations, new_points, new_valid,
                    intrinsics='fk' if n_reg >= 3 else 'none')
            # if this registration hurt the fit, give it one second
            # chance with outliers pruned before rolling it back: a
            # correct pose often arrives with a tail of bad tracks
            if do_ba and new_rms > max(3.0 * rms, rms + 2.0):
                prune_observations(views, observations, new_points,
                                   new_valid,
                                   thresh=max(4.0, 2.0 * new_rms))
                new_points, new_valid = triangulate_tracks(
                    views, observations, reproj_thresh=50.0)
                new_rms = bundle_adjust(
                    views, observations, new_points, new_valid,
                    intrinsics='fk')
            if do_ba and new_rms > max(3.0 * rms, rms + 2.0):
                for v, (rv, tv, f, cx, cy, k1) in zip(views, state):
                    v.rvec, v.tvec = rv, tv
                    v.f, v.cx, v.cy, v.k1 = f, cx, cy, k1
                views[k].registered = False
                if verbose:
                    print(f"rolled back {views[k].name}: rms "
                          f"{rms:.2f} -> {new_rms:.2f}px")
                continue
            if do_ba:
                rms = new_rms
                regs_since_ba = 0
            else:
                regs_since_ba += 1
            points, valid = new_points, new_valid
            if verbose:
                print(f"registered {views[k].name} ({n_inl} PnP inliers), "
                      f"rms {rms:.2f}px, {valid.sum()} tracks")
            registered_one = True
            break
        if not registered_one:
            if verbose:
                print(f"cannot register remaining views: "
                      f"{[views[k].name for k in unreg]}")
            break

    # final staged refinement, tightening the outlier gates as the model
    # improves: pruning relative to the current fit avoids the selection
    # bias where a fixed tight gate removes exactly the border points
    # that constrain focal length and distortion
    points, valid = triangulate_tracks(views, observations,
                                       reproj_thresh=50.0)
    rms = bundle_adjust(views, observations, points, valid, intrinsics='fk',
                        max_nfev=300)
    n_removed = prune_observations(views, observations, points, valid,
                                   thresh=max(4.0, 2.0 * rms))
    points, valid = triangulate_tracks(views, observations,
                                       reproj_thresh=30.0)
    if verbose:
        print(f"final BA (fk): rms {rms:.2f}px, "
              f"pruned {n_removed} outlier observations")
    rms = bundle_adjust(views, observations, points, valid,
                        intrinsics='full', max_nfev=300)
    n_removed = prune_observations(views, observations, points, valid,
                                   thresh=max(2.0, 1.5 * rms))
    points, valid = triangulate_tracks(views, observations,
                                       reproj_thresh=20.0)
    rms = bundle_adjust(views, observations, points, valid,
                        intrinsics='full', max_nfev=300)
    if verbose:
        print(f"final BA (full): rms {rms:.2f}px, "
              f"{valid.sum()}/{len(valid)} tracks "
              f"(pruned {n_removed} more observations)")
    return points, valid


def triangulate_dense_pair(view_a, view_b, uv_a, uv_b):
    """Vectorized two-view triangulation for dense maps.

    uv_a, uv_b: (N,2) pixel observations in each view.
    Returns (N,3) points and (N,) max reprojection error in pixels.
    """
    na = view_a.undistort_normalize(uv_a)
    nb = view_b.undistort_normalize(uv_b)
    Pa = np.hstack([view_a.R, view_a.tvec.reshape(3, 1)])
    Pb = np.hstack([view_b.R, view_b.tvec.reshape(3, 1)])
    X = cv2.triangulatePoints(Pa, Pb, na.T.astype(np.float64),
                              nb.T.astype(np.float64))
    X = (X[:3] / X[3]).T
    ea, za = view_a.project(X)
    eb, zb = view_b.project(X)
    err = np.maximum(np.linalg.norm(ea - uv_a, axis=1),
                     np.linalg.norm(eb - uv_b, axis=1))
    err[(za <= 0) | (zb <= 0)] = np.inf
    return X, err


def fit_floor_and_normalize(points, camera_centers, margin=0.02,
                            ransac_iters=500, inlier_frac_tol=3.0):
    """Similarity transform mapping the cloud into normalized room coords.

    Finds the dominant plane (assumed floor), orients its normal so cameras
    are above it, aligns room axes with PCA, and scales the largest room
    dimension to 0..1 with y=0 on the floor.
    Returns a 4x4 matrix; apply as (M @ [x,y,z,1])[:3].
    """
    pts = points
    rng = np.random.default_rng(0)
    # scale-aware inlier threshold from cloud extent
    extent = np.percentile(pts, 98, axis=0) - np.percentile(pts, 2, axis=0)
    thresh = np.max(extent) * 0.01 * inlier_frac_tol

    best_inliers = None
    for _ in range(ransac_iters):
        sample = pts[rng.choice(len(pts), 3, replace=False)]
        n = np.cross(sample[1] - sample[0], sample[2] - sample[0])
        nn = np.linalg.norm(n)
        if nn < 1e-12:
            continue
        n = n / nn
        d = -n @ sample[0]
        dist = np.abs(pts @ n + d)
        inliers = dist < thresh
        if best_inliers is None or inliers.sum() > best_inliers.sum():
            best_inliers, best_plane = inliers, (n, d)
    n, d = best_plane
    # refine with least squares on inliers
    inl = pts[best_inliers]
    centroid = inl.mean(0)
    _, _, vt = np.linalg.svd(inl - centroid)
    n = vt[-1]

    # floor normal points toward the cameras (they're above the floor)
    cam_mean = np.mean(camera_centers, axis=0)
    if n @ (cam_mean - centroid) < 0:
        n = -n

    # rotation taking floor normal to +Y
    y = n
    tmp = np.array([1.0, 0, 0]) if abs(y[0]) < 0.9 else np.array([0, 0, 1.0])
    x = np.cross(tmp, y)
    x /= np.linalg.norm(x)
    z = np.cross(x, y)
    R1 = np.stack([x, y, z])  # world -> floor-aligned

    aligned = (pts - centroid) @ R1.T
    # principal room axes from the horizontal footprint
    xz = aligned[:, [0, 2]]
    xz_c = xz - xz.mean(0)
    cov = xz_c.T @ xz_c
    evals, evecs = np.linalg.eigh(cov)
    major = evecs[:, np.argmax(evals)]
    ang = np.arctan2(major[1], major[0])
    c, s = np.cos(-ang), np.sin(-ang)
    R2 = np.array([[c, 0, -s], [0, 1, 0], [s, 0, c]])
    aligned = aligned @ R2.T

    lo = np.percentile(aligned, 2, axis=0)
    hi = np.percentile(aligned, 98, axis=0)
    lo[1] = np.percentile(aligned[:, 1], 1)  # floor level
    scale = 1.0 / max(hi[0] - lo[0], hi[2] - lo[2])

    M = np.eye(4)
    RR = R2 @ R1
    M[:3, :3] = scale * RR
    M[:3, 3] = -scale * RR @ centroid - scale * lo + margin
    # y = 0 exactly at floor (no margin on the floor axis)
    M[1, 3] = -scale * (RR @ centroid)[1] - scale * lo[1]
    return M


def umeyama_alignment(src, dst):
    """Closed-form similarity (s, R, t) minimizing ||dst - (s R src + t)||.

    Returns 4x4 matrix. Used to compare a reconstruction against
    ground truth (e.g. old camamok output).
    """
    mu_s = src.mean(0)
    mu_d = dst.mean(0)
    sc = src - mu_s
    dc = dst - mu_d
    cov = dc.T @ sc / len(src)
    U, D, Vt = np.linalg.svd(cov)
    S = np.eye(3)
    if np.linalg.det(U) * np.linalg.det(Vt) < 0:
        S[2, 2] = -1
    R = U @ S @ Vt
    var = (sc ** 2).sum() / len(src)
    s = np.trace(np.diag(D) @ S) / var
    t = mu_d - s * R @ mu_s
    M = np.eye(4)
    M[:3, :3] = s * R
    M[:3, 3] = t
    return M


def apply_transform(M, points):
    return points @ M[:3, :3].T + M[:3, 3]

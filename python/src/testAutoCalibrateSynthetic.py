"""Synthetic end-to-end validation of the model-free calibration solver.

Simulates a room, projectors and camera scan positions; generates the same
kind of correspondences that gray-code decoding produces; runs the full
reconstruction; and reports error against the known ground truth geometry.

Run:  python3 testAutoCalibrateSynthetic.py
"""
import numpy as np
import cv2
from autoCalibrateCore import (
    View, reconstruct, triangulate_dense_pair,
    fit_floor_and_normalize, umeyama_alignment, apply_transform)

ROOM = np.array([10.0, 4.0, 6.0])  # length x, height y, width z
RNG = np.random.default_rng(42)
NOISE_PX = 0.3


def look_at(position, target):
    """OpenCV-convention pose (x right, y down, z forward)."""
    position = np.asarray(position, float)
    z = np.asarray(target, float) - position
    z /= np.linalg.norm(z)
    y0 = np.array([0.0, -1.0, 0.0])  # camera up = world up
    x = np.cross(y0, z)
    x /= np.linalg.norm(x)
    y = np.cross(z, x)
    R = np.stack([x, y, z])
    rvec = cv2.Rodrigues(R)[0].ravel()
    tvec = -R @ position
    return rvec, tvec


def intersect_room(origins, dirs):
    """Nearest intersection of rays with the inside of the room box."""
    n = len(dirs)
    best_t = np.full(n, np.inf)
    hits = np.zeros((n, 3))
    for axis in range(3):
        for bound in (0.0, ROOM[axis]):
            denom = dirs[:, axis]
            with np.errstate(divide='ignore', invalid='ignore'):
                t = (bound - origins[:, axis]) / denom
            pts = origins + dirs * t[:, None]
            ok = (t > 1e-6) & np.isfinite(t)
            for other in range(3):
                if other != axis:
                    ok &= (pts[:, other] > -1e-6) & \
                          (pts[:, other] < ROOM[other] + 1e-6)
            better = ok & (t < best_t)
            best_t[better] = t[better]
            hits[better] = pts[better]
    return hits, np.isfinite(best_t)


def make_truth_view(name, w, h, fov, pos, target, is_projector=False,
                    principal_point=None, k1=0.0):
    v = View(name, w, h, fov_deg=fov, is_projector=is_projector,
             principal_point=principal_point)
    v.k1 = k1
    v.rvec, v.tvec = look_at(pos, target)
    v.registered = True
    return v


def cast_view_pixels(view, us, vs):
    """World hit points for pixel grid of a view (its ground-truth rays)."""
    uv = np.stack([us, vs], axis=1).astype(np.float64)
    xn = view.undistort_normalize(uv)
    d_cam = np.hstack([xn, np.ones((len(xn), 1))])
    d_world = d_cam @ view.R  # R.T @ d, batched
    origin = view.camera_center()
    origins = np.tile(origin, (len(xn), 1))
    return intersect_room(origins, d_world)


def build_synthetic_scene():
    """Ground truth rig: 2 projector devices + 3 scan positions."""
    pw, ph = 1920, 1200
    # projectors mounted high, principal point shifted (lens shift);
    # proj0 lights the left end, proj1 the right end, overlapping mid-room
    truth_projs = [
        make_truth_view('proj0', pw, ph, 55.0, [3.0, 3.2, 3.1],
                        [2.5, 0.8, 2.0], True, principal_point=(960, 1080)),
        make_truth_view('proj1', pw, ph, 55.0, [6.5, 3.3, 2.9],
                        [7.5, 1.0, 4.0], True, principal_point=(980, 1150)),
    ]
    # scan positions mimic real captures: wide lens, shot from the corners,
    # every projector footprint photographed from at least two positions
    cw, ch = 4000, 3000
    truth_cams = [
        make_truth_view('scan0', cw, ch, 85.0, [9.0, 1.6, 5.0],
                        [2.0, 1.0, 2.0], k1=-0.06),
        make_truth_view('scan1', cw, ch, 85.0, [9.0, 1.6, 1.0],
                        [2.0, 1.0, 4.0], k1=-0.06),
        make_truth_view('scan2', cw, ch, 85.0, [1.0, 1.6, 5.0],
                        [8.0, 1.0, 2.0], k1=-0.06),
        make_truth_view('scan3', cw, ch, 85.0, [1.0, 1.6, 1.0],
                        [8.0, 1.0, 4.0], k1=-0.06),
    ]
    return truth_projs, truth_cams


def generate_tracks(truth_projs, truth_cams, step=60):
    """Simulate decoded gray-code correspondences.

    Track layout matches the real pipeline: one track per projector pixel,
    observed by the projector itself and by every camera that sees the
    surface point it lands on.
    """
    observations = []
    truth_points = []
    n_views_offset = len(truth_projs)
    for p_idx, proj in enumerate(truth_projs):
        uu, vv = np.meshgrid(
            np.arange(step // 2, proj.width, step),
            np.arange(step // 2, proj.height, step))
        us, vs = uu.ravel().astype(float), vv.ravel().astype(float)
        pts, hit = cast_view_pixels(proj, us, vs)
        for k in range(len(us)):
            if not hit[k]:
                continue
            track = [(p_idx, us[k] + RNG.normal(0, NOISE_PX),
                      vs[k] + RNG.normal(0, NOISE_PX))]
            for c_idx, cam in enumerate(truth_cams):
                uv, z = cam.project(pts[k][None])
                u, v = uv[0]
                if z[0] > 0 and 0 <= u < cam.width and 0 <= v < cam.height:
                    # occlusion check: the camera must see this point, i.e.
                    # its own ray to the point must hit the room at the point
                    o = cam.camera_center()[None]
                    d = (pts[k] - o)
                    d = d / np.linalg.norm(d)
                    hp, hh = intersect_room(o, d)
                    if hh[0] and np.linalg.norm(hp[0] - pts[k]) < 0.01:
                        track.append((n_views_offset + c_idx,
                                      u + RNG.normal(0, NOISE_PX),
                                      v + RNG.normal(0, NOISE_PX)))
            if len(track) >= 2:
                observations.append(track)
                truth_points.append(pts[k])
    return observations, np.array(truth_points)


def main():
    truth_projs, truth_cams = build_synthetic_scene()
    observations, truth_points = generate_tracks(truth_projs, truth_cams)
    n_multi = sum(1 for t in observations if len(t) >= 3)
    print(f"synthetic scene: {len(observations)} tracks "
          f"({n_multi} seen by >=2 cameras+projector)")

    # solver initial guesses mirror what the real loader can provide:
    # cameras get an EXIF-grade focal guess (few percent off) and share
    # one intrinsic set (same physical camera moved between scans);
    # projectors get only a rough throw-ratio guess and stay independent
    views = []
    for tp in truth_projs:
        views.append(View(tp.name, tp.width, tp.height, fov_deg=60.0,
                          is_projector=True))
    for tc in truth_cams:
        v = View(tc.name, tc.width, tc.height, fov_deg=87.5)
        v.intrinsic_group = 'camera'
        views.append(v)

    points, valid = reconstruct(views, observations, verbose=True)

    assert all(v.registered for v in views), "not all views registered"

    # --- accuracy vs ground truth (similarity-aligned) ---
    M = umeyama_alignment(points[valid], truth_points[valid])
    aligned = apply_transform(M, points[valid])
    err = np.linalg.norm(aligned - truth_points[valid], axis=1)
    diag = np.linalg.norm(ROOM)
    print(f"\nsparse cloud vs truth: rms {err.std() + err.mean():.4f} m, "
          f"median {np.median(err) * 100:.2f} cm, "
          f"p95 {np.percentile(err, 95) * 100:.2f} cm "
          f"(room diagonal {diag:.1f} m)")

    # recovered intrinsics check
    print("\nrecovered intrinsics (truth -> estimate):")
    for v, t in zip(views, truth_projs + truth_cams):
        print(f"  {v.name}: f {t.f:.0f} -> {v.f:.0f}, "
              f"pp ({t.cx:.0f},{t.cy:.0f}) -> ({v.cx:.0f},{v.cy:.0f}), "
              f"k1 {t.k1:.3f} -> {v.k1:.3f}")

    assert np.median(err) < 0.02 * diag, "median error above 2% of diagonal"

    # --- dense two-view triangulation check (proj0 vs scan seeing it) ---
    proj_v = views[0]
    cam_v = views[2]
    uv_p, uv_c, truth_d = [], [], []
    for t_idx, track in enumerate(observations):
        d = dict((vi, (u, v)) for vi, u, v in track)
        if 0 in d and 2 in d:
            uv_p.append(d[0])
            uv_c.append(d[2])
            truth_d.append(truth_points[t_idx])
    X, perr = triangulate_dense_pair(proj_v, cam_v,
                                     np.array(uv_p), np.array(uv_c))
    Xa = apply_transform(M, X)
    derr = np.linalg.norm(Xa - np.array(truth_d), axis=1)
    print(f"\ndense pair triangulation: median {np.median(derr)*100:.2f} cm, "
          f"reproj err median {np.median(perr):.2f} px")
    assert np.median(derr) < 0.02 * diag

    # --- floor detection + normalization ---
    cam_centers = np.array([v.camera_center() for v in views])
    N = fit_floor_and_normalize(points[valid], cam_centers)
    norm_pts = apply_transform(N, points[valid])
    truth_floor = truth_points[valid][:, 1] < 0.05
    floor_y = norm_pts[truth_floor][:, 1]
    print(f"\nnormalized cloud: floor pixels y median "
          f"{np.median(np.abs(floor_y)):.4f} (want ~0), "
          f"extent x [{norm_pts[:,0].min():.2f},{norm_pts[:,0].max():.2f}] "
          f"y [{norm_pts[:,1].min():.2f},{norm_pts[:,1].max():.2f}] "
          f"z [{norm_pts[:,2].min():.2f},{norm_pts[:,2].max():.2f}]")
    assert np.median(np.abs(floor_y)) < 0.02, "floor not at y=0"

    print("\nALL CHECKS PASSED")


if __name__ == '__main__':
    main()

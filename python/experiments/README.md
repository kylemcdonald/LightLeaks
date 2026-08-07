# Model-free calibration experiments

Goal: produce the projector-space `xyzMap-0.exr` / `confidenceMap-0.exr`
that the LightLeaks shader consumes **without a measured 3d model and
without camamok's manual 2D-3D clicking**.

The solver lives in `../src/autoCalibrateCore.py`; it is validated
end-to-end by `../src/testAutoCalibrateSynthetic.py` (median error
0.7 cm on a simulated 10x6x4 m room, intrinsics recovered within ~1%).

## Approach

The gray-code scans already contain everything needed:

- per scan, `camBinary` / `proMap` links every camera pixel to the
  projector pixel that lights it;
- across scans, the **projector pixel ID is a universal marker**: two
  cameras decoding the same projector pixel observed the same physical
  surface point - dense multi-view matches with no feature detection.

Pipeline: pairwise fundamental-matrix verification of correspondences ->
incremental SfM (parallax-checked init pair, multi-solver PnP with
verified-inlier acceptance, rollback with prune-and-refit second
chance) -> staged bundle adjustment with intrinsic priors and optional
shared-intrinsic groups -> dense per-projector-pixel triangulation ->
floor-plane + PCA gauge fixing into normalized room coordinates.

## TodaysArt 2018 validation (`autoCalibrateTodaysArt.py`)

Runs against the TodaysArt scan archive (fetch selected members from
the `lightleaks-data` GCS bucket with `zip_list.py` / `zip_fetch.py`),
comparing against the camamok-era `xyzMap-0.exr` as ground truth.

Findings from this archive (2018, pre-python pipeline):

- camera-camera correspondences are 40-68% epipolar-consistent between
  nearby scan positions; the outliers are structured (mirror-ball
  glints vs reflected caustics), not noise
- projector-camera correspondences are NOT pinhole-consistent (1-2%
  inliers): the three projectors were warped/blended, so the solver
  runs camera-only and projector pixels serve as track IDs
- the camera zoom was changed repeatedly during capture (GT focals
  2300-3600 px) and the archived JPEGs have no EXIF, so per-scan
  intrinsics are required; new captures should keep EXIF and a fixed
  zoom, which restores the much stronger shared-intrinsics mode
- several scans were shot in pairs from the same tripod position;
  init-pair selection must reject those (parallax check) or the whole
  reconstruction is built on meaningless depths
- the projectors aim directly at the mirror-ball pile, so most bright
  decoded pixels are view-dependent glints; honest triangulation
  coverage is ~4% of projector pixels vs camamok's model-painted 47%.
  With the afternoon session fully registered the recovered focals
  match ground truth within ~5% and the dense map's median error is
  0.13 model units against a GT that itself carries 5-36 px internal
  reprojection error.

## What a new capture needs for this to shine

1. keep EXIF on the photos (focal length) and do not touch the zoom
2. move the tripod between scans - never two scans from the same spot
3. include a few scans framing mostly walls/floor (diffuse surfaces)
4. any number of projectors is fine, but avoid warping/blending during
   the scan if projector-as-a-view triangulation is wanted

## Llum 2019 validation (`autoCalibrateLlum.py`)

The Can Framis courtyard install (python-era archive layout with
`proMap-python.png`). Much friendlier data than TodaysArt: diffuse
courtyard walls push camera-camera epipolar consistency to 60-78%.

Results: 7/10 scans registered, final bundle adjustment at 0.98 px rms,
recovered focals matching the ground-truth fits within a few pixels
(e.g. 3180 vs 3172, 2346 vs 2347), and the dense projector-space map
lands at **median error 2.3% / p90 7% of the lit-scene extent** against
the camamok ground truth. The camera zoom was again changed between
scans, and 4 of 10 archived per-scan xyzMaps are corrupted - both
handled automatically.

## LightsAllNight 2019 validation (`autoCalibrateLightsAllNight.py`)

The 4x30K-projector festival install (7680x1080 virtual). The only
archive captured with a FIXED zoom (ground-truth focals 2362-2458), so
the shared-intrinsics mode applies. Both capture sessions reconstruct
excellently in isolation - 0.61 px and 0.64 px rms, shared focal
recovered at 2479 / 2415 vs ~2400 ground truth - but they cannot be
merged: cross-session correspondences are only ~16% epipolar-consistent
(vs 40% within-session), the dense maps share only 102 projector
pixels, and bridge-camera PnP finds zero consistent points. Conclusion:
the mirror-ball pile was physically rearranged between the 6pm and
11pm sessions. The runner includes the island-split reconstruction and
a bridge-camera merge that applies whenever the scene actually stayed
still.

This adds a fifth capture rule: **don't move the balls (or projectors)
mid-capture - if the scene changes, rescan everything after.**

## Mesh reconstruction (`buildMesh.py` / `renderMesh.py`)

`buildMesh.py <result.npz> <out_stem>` turns a calibration run's dense
cloud into a Poisson mesh (`.ply`, loadable in MeshLab/Blender):
outlier removal -> normal estimation -> Poisson -> density trim.
This closes the README's old wish: the 3d model is now an *output* of
scanning instead of a hand-measured input.

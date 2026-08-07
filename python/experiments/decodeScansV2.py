"""Structured-light decoder v2 - re-decodes raw cameraImages with the
improvements from the audit:

- weighted-luma gray conversion (not blue-noise-amplifying mean)
- per-bit decoding both raw and high-passed, keeping the larger margin
  (the fixed high-pass erased coarse stripes' own signal on big walls)
- per-bit minimum margin as confidence (a pixel is only as good as its
  worst bit), instead of one global statistic
- LUT-free gray->binary decode
- sub-pixel code refinement via local median/mean pooling
- no hand masks required: downstream geometric verification replaces them

Outputs per scan: camCodeV2.exr (float32 projector x,y per camera
pixel), camConfV2.exr (min-margin), proMapV2.exr + proConfV2.exr
(projector-space winner map, float camera coords).
"""
import os
os.environ["OPENCV_IO_ENABLE_OPENEXR"] = "1"
import sys
import glob
import numpy as np
import cv2


def load_gray(fn):
    img = cv2.imread(fn, cv2.IMREAD_COLOR)  # BGR
    return (0.15 * img[..., 0] + 0.60 * img[..., 1] +
            0.25 * img[..., 2]).astype(np.float32)


def load_axis(scan_dir, axis):
    ndir = os.path.join(scan_dir, 'cameraImages', axis, 'normal')
    idir = os.path.join(scan_dir, 'cameraImages', axis, 'inverse')
    n_levels = len(glob.glob(os.path.join(ndir, '*.jpg')))
    assert n_levels > 0, f"no images in {ndir}"
    normals, inverses = [], []
    for i in range(n_levels):
        normals.append(load_gray(os.path.join(ndir, f'{i}.jpg')))
        inverses.append(load_gray(os.path.join(idir, f'{i}.jpg')))
    return np.stack(normals), np.stack(inverses)


def decode_axis(normal, inverse, hp_radius=101, drop_finest=2):
    """Per-bit dual-path decode. File index i is stored coarse-first
    after the capture app's inversion (level name = levelCount-level-1),
    so index 0 = MSB of the gray code.
    Returns integer code, min-margin, and the finest-bit analog diff."""
    n_levels, h, w = normal.shape
    # the finest stripe levels (1-2 projector px) are below the
    # camera/defocus resolution limit: their margins are noise and
    # their bits corrupt the LSBs. skip them; sub-pixel pooling
    # recovers finer-than-quantization resolution afterwards.
    use = max(3, n_levels - drop_finest)
    code = np.zeros((h, w), np.uint32)
    min_margin = np.full((h, w), np.inf, np.float32)
    for i in range(use):
        d_raw = normal[i] - inverse[i]
        # high-pass helps only when scatter varies between the pair;
        # use it when it produces a larger margin
        lp = cv2.blur(d_raw, (hp_radius, hp_radius))
        d_hp = d_raw - lp
        pick_hp = np.abs(d_hp) > np.abs(d_raw)
        d = np.where(pick_hp, d_hp, d_raw)
        bit = (d > 0).astype(np.uint32)
        code = (code << 1) | bit
        min_margin = np.minimum(min_margin, np.abs(d))
    # gray -> binary
    b = code.copy()
    shift = 1
    while shift < use:
        b ^= b >> shift
        shift <<= 1
    # scale back to full-resolution code units (center of the coarse bin)
    scale = 1 << (n_levels - use)
    return (b.astype(np.float32) + 0.5) * scale - 0.5, min_margin


def subpixel_refine(cx, cy, conf, conf_floor):
    """Local robust pooling: median of 3x3 confident neighbors, then
    mean of the inliers within 1.5 codes of the median -> float codes
    with single-bit-error suppression."""
    ok = conf > conf_floor
    out = []
    for c in (cx, cy):
        cm = np.where(ok, c, np.nan)
        stack = []
        for oy in (-1, 0, 1):
            for ox in (-1, 0, 1):
                stack.append(np.roll(np.roll(cm, oy, 0), ox, 1))
        stack = np.stack(stack)
        with np.errstate(all='ignore'):
            med = np.nanmedian(stack, axis=0)
            near = np.abs(stack - med[None]) <= 1.5
            pooled = np.nanmean(np.where(near, stack, np.nan), axis=0)
        out.append(np.where(np.isfinite(pooled), pooled, c))
    return out[0].astype(np.float32), out[1].astype(np.float32)


def build_promap(cx, cy, conf, pw, ph, bin_px=4):
    """Projector-space winner map with float camera coords.

    Codes are quantized to bin_px bins (drop_finest), so deciding
    winners at full projector resolution leaves the map empty between
    bin centers - downstream grid samples (step 4/12) then read zeros.
    Decide the winner per BIN, then splat it across the whole bin."""
    bw, bh = pw // bin_px, ph // bin_px
    xi = np.clip((cx / bin_px).astype(np.int32), 0, bw - 1)
    yi = np.clip((cy / bin_px).astype(np.int32), 0, bh - 1)
    flat = yi.ravel() * bw + xi.ravel()
    order = np.argsort(conf.ravel())
    h, w = conf.shape
    ys, xs = np.mgrid[0:h, 0:w]
    pro_cam = np.zeros((bh * bw, 2), np.float32)
    pro_conf = np.zeros(bh * bw, np.float32)
    f_sorted = flat[order]
    pro_cam[f_sorted, 0] = xs.ravel()[order]
    pro_cam[f_sorted, 1] = ys.ravel()[order]
    pro_conf[f_sorted] = conf.ravel()[order]
    pro_cam = pro_cam.reshape(bh, bw, 2)
    pro_conf = pro_conf.reshape(bh, bw)
    pro_cam = np.repeat(np.repeat(pro_cam, bin_px, 0), bin_px, 1)
    pro_conf = np.repeat(np.repeat(pro_conf, bin_px, 0), bin_px, 1)
    # pad any remainder (pw/ph not divisible by bin_px) with zeros
    if pro_cam.shape[0] != ph or pro_cam.shape[1] != pw:
        full_cam = np.zeros((ph, pw, 2), np.float32)
        full_conf = np.zeros((ph, pw), np.float32)
        full_cam[:pro_cam.shape[0], :pro_cam.shape[1]] = pro_cam[:ph, :pw]
        full_conf[:pro_conf.shape[0], :pro_conf.shape[1]] = pro_conf[:ph, :pw]
        return full_cam, full_conf
    return pro_cam, pro_conf


def write_arr(fn, arr):
    np.save(fn, arr.astype(np.float32))


def decode_scan(scan_dir, pw, ph, out_dir=None):
    out_dir = out_dir or scan_dir
    nv, iv = load_axis(scan_dir, 'vertical')
    code_x, margin_x = decode_axis(nv, iv)
    del nv, iv
    nh, ih = load_axis(scan_dir, 'horizontal')
    code_y, margin_y = decode_axis(nh, ih)
    del nh, ih
    conf = np.minimum(margin_x, margin_y)
    # normalize confidence to roughly match the old 0..~1 scale
    conf = conf / 50.0
    code_x = np.minimum(code_x, pw - 1)
    code_y = np.minimum(code_y, ph - 1)
    sx, sy = subpixel_refine(code_x, code_y, conf, 0.05)
    write_arr(os.path.join(out_dir, 'camCodeV2.npy'),
              np.dstack([sx, sy]))
    write_arr(os.path.join(out_dir, 'camConfV2.npy'), conf)
    pro_cam, pro_conf = build_promap(sx, sy, conf, pw, ph)
    write_arr(os.path.join(out_dir, 'proMapV2.npy'), pro_cam)
    write_arr(os.path.join(out_dir, 'proConfV2.npy'), pro_conf)
    n = (conf > 0.05).sum()
    print(f"{os.path.basename(scan_dir)}: {n} confident px "
          f"({100*n/conf.size:.1f}%), pro coverage "
          f"{(pro_conf>0.05).mean()*100:.1f}%")
    return conf


if __name__ == '__main__':
    root = sys.argv[1]
    pw, ph = int(sys.argv[2]), int(sys.argv[3])
    scans = sorted(d for d in os.listdir(root)
                   if os.path.isdir(os.path.join(root, d, 'cameraImages')))
    print(f"{len(scans)} scans with raw images")
    for s in scans:
        decode_scan(os.path.join(root, s), pw, ph)

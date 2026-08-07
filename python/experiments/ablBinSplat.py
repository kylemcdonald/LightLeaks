"""Bin-splat ablation, isolated. Codes are quantized to 4px bins
(centers at 2,6,10,...); the solver samples the projector map on a
step-4 grid (0,4,8,...). Without splatting the bin winner across its
full width, the two integer lattices never intersect -> ~0 coverage."""
import os
os.environ["OPENCV_IO_ENABLE_OPENEXR"] = "1"
import sys, numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from decode_v2 import subpixel_refine

RAW = os.path.join(HERE, 'lan_raw', 'SharedData', 'scan-1831')
PW, PH = 7680, 1080
STEP = 4
BIN = 4

code = np.load(os.path.join(RAW, 'camCodeC.npy'))     # HxWx2 proj coords
conf = np.load(os.path.join(RAW, 'camConfC.npy'))
cx, cy = code[..., 0], code[..., 1]
sx, sy = subpixel_refine(cx, cy, conf, 0.05)
sx = np.clip(sx, 0, PW - 1); sy = np.clip(sy, 0, PH - 1)
ok = conf > 0.05

def coverage_on_grid(pcam_conf):
    grid = pcam_conf[0:PH:STEP, 0:PW:STEP]
    return 100.0 * (grid > 0.05).mean(), (grid > 0.05).sum()

# --- WITHOUT bin-splat: winner placed only at its (quantized) pixel ---
pconf_nosplat = np.zeros((PH, PW), np.float32)
xi = np.round(sx).astype(np.int32)[ok]
yi = np.round(sy).astype(np.int32)[ok]
cf = conf[ok]
order = np.argsort(cf)
pconf_nosplat[yi[order], xi[order]] = cf[order]
cov_no, n_no = coverage_on_grid(pconf_nosplat)

# --- WITH bin-splat: decide per bin, repeat across the whole bin ---
bw, bh = PW // BIN, PH // BIN
bxi = np.clip((sx / BIN).astype(np.int32), 0, bw - 1)
byi = np.clip((sy / BIN).astype(np.int32), 0, bh - 1)
flat = (byi.ravel() * bw + bxi.ravel())
pconf_bin = np.zeros(bh * bw, np.float32)
o2 = np.argsort(conf.ravel())
pconf_bin[flat[o2]] = conf.ravel()[o2]
pconf_bin = pconf_bin.reshape(bh, bw)
pconf_splat = np.repeat(np.repeat(pconf_bin, BIN, 0), BIN, 1)
cov_yes, n_yes = coverage_on_grid(pconf_splat)

print(f"WITHOUT bin-splat: grid coverage {cov_no:.2f}%  ({n_no} px)")
print(f"WITH    bin-splat: grid coverage {cov_yes:.2f}%  ({n_yes} px)")
print(f"per-scan winner pixels (full res, either way): {int(ok.sum())}")

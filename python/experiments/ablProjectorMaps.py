"""Projector-space map under each DECODE/SAMPLING toggle, same mirror
cluster crop, colored by the camera column each projector pixel maps to
(the thing that stays smooth inside a mirror). Cheap: single scan."""
import os
os.environ["OPENCV_IO_ENABLE_OPENEXR"] = "1"
import sys, numpy as np, cv2
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from decode_v2 import subpixel_refine, build_promap
from diag_decoder import decode_axis_variant, load_axis

RAW = os.path.join(HERE, 'lan_raw', 'SharedData', 'scan-1831')
PW, PH = 7680, 1080
CROP = (40, 360, 120, 760)          # y0,y1,x0,x1 - the ceiling ball cluster
HUEMAX = None                        # set from mode C for consistent hue scale

def promap_from_camcode(cx, cy, conf, splat=True):
    sx, sy = subpixel_refine(np.clip(cx, 0, PW-1), np.clip(cy, 0, PH-1),
                             conf, 0.05)
    if splat:
        return build_promap(sx, sy, conf, PW, PH)
    # NO splat: winner placed only at its quantized projector pixel (comb)
    pconf = np.zeros((PH, PW), np.float32)
    pcam = np.zeros((PH, PW, 2), np.float32)
    ok = conf > 0.05
    ch, cw = conf.shape                          # camera-space grid
    cam_ys, cam_xs = np.mgrid[0:ch, 0:cw]
    xi = np.round(sx).astype(np.int32)[ok]       # target projector pixel
    yi = np.round(sy).astype(np.int32)[ok]
    cf = conf[ok]
    camx = cam_xs[ok]; camy = cam_ys[ok]         # winning camera pixel
    o = np.argsort(cf)                           # low->high, so high wins
    pconf[yi[o], xi[o]] = cf[o]
    pcam[yi[o], xi[o], 0] = camx[o]
    pcam[yi[o], xi[o], 1] = camy[o]
    return pcam, pconf

def render(pcam, pconf, tag, huemax):
    y0, y1, x0, x1 = CROP
    cam = pcam[y0:y1, x0:x1, 0]
    ok = pconf[y0:y1, x0:x1] > 0.05
    hue = np.clip(cam / huemax * 179, 0, 179).astype(np.uint8)
    val = np.where(ok, 255, 0).astype(np.uint8)
    rgb = cv2.cvtColor(np.dstack([hue, np.full_like(hue, 210), val]),
                       cv2.COLOR_HSV2BGR)
    rgb[~ok] = (16, 17, 20)
    up = cv2.resize(rgb, (rgb.shape[1]*2, rgb.shape[0]*2),
                    interpolation=cv2.INTER_NEAREST)
    cv2.imwrite(f'figs/pm_{tag}.png', up)
    print(f'  rendered pm_{tag}  coverage(crop) {100*ok.mean():.1f}%')

# hue scale reference from cached C
pmC = np.load('figs/promap_C.npy'); HUEMAX = pmC[..., 0].max()

# --- mode A (raw) from cached camCode A ---
cA = np.load(os.path.join(RAW, 'camCodeA.npy'))
qA = np.load(os.path.join(RAW, 'camConfA.npy'))
pcamA, pconfA = promap_from_camcode(cA[..., 0], cA[..., 1], qA, splat=True)
render(pcamA, pconfA, 'A', HUEMAX)

# --- C already rendered as promap_C; re-crop for consistent hue/region ---
render(pmC, np.load('figs/proconf_C.npy'), 'C', HUEMAX)
render(np.load('figs/promap_B.npy'), np.load('figs/proconf_B.npy'), 'B', HUEMAX)

# --- bin-splat OFF (comb) vs ON, mode C from cache ---
cC = np.load(os.path.join(RAW, 'camCodeC.npy'))
qC = np.load(os.path.join(RAW, 'camConfC.npy'))
pcam_ns, pconf_ns = promap_from_camcode(cC[..., 0], cC[..., 1], qC, splat=False)
render(pcam_ns, pconf_ns, 'nosplat', HUEMAX)
render(pmC, np.load('figs/proconf_C.npy'), 'splat', HUEMAX)   # = C

# --- drop_finest 0 (all levels) vs 2 (=C); fresh decode for drop0 ---
nv, iv = load_axis(RAW, 'vertical'); nh, ih = load_axis(RAW, 'horizontal')
cx0, mx0 = decode_axis_variant(nv, iv, 'C', drop_finest=0)
cy0, my0 = decode_axis_variant(nh, ih, 'C', drop_finest=0)
conf0 = np.minimum(mx0, my0) / 50.0
pcam0, pconf0 = promap_from_camcode(cx0, cy0, conf0, splat=True)
render(pcam0, pconf0, 'drop0', HUEMAX)
render(pmC, np.load('figs/proconf_C.npy'), 'drop2', HUEMAX)   # = C
print('decoder/sampling proMaps done (A,B,C,nosplat,splat,drop0,drop2)')

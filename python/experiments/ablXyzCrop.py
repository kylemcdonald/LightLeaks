"""Render projector-space xyz-map crops (room position -> RGB) for the
dense ablations, same mirror region as the decoder crops, quarter-res
grid. Consistent color normalization across a before/after pair."""
import sys, numpy as np, cv2

# step-4 grid: full-res crop y40-360,x120-760 -> /4
Y0, Y1, X0, X1 = 10, 90, 30, 190

def load_aligned(npz):
    z = np.load(npz)
    d = z['dense_xyz'].astype(np.float64)
    solved = np.isfinite(z['dense_err'])
    M = z['M']; H, W, _ = d.shape
    al = (M[:3, :3] @ d.reshape(-1, 3).T).T + M[:3, 3]
    xyz = al.reshape(H, W, 3); xyz[~solved] = np.nan
    return xyz

def render(xyz, lo, hi, tag):
    crop = xyz[Y0:Y1, X0:X1]
    ok = np.isfinite(crop).all(2)
    norm = np.clip((crop - lo) / (hi - lo), 0, 1)
    rgb = (norm[..., ::-1] * 255).astype(np.uint8)   # xyz->BGR
    rgb[~ok] = (16, 17, 20)
    up = cv2.resize(rgb, ((X1 - X0) * 6, (Y1 - Y0) * 6),
                    interpolation=cv2.INTER_NEAREST)
    cv2.imwrite(f'figs/{tag}.png', up)
    print(f'  {tag}: crop coverage {100*ok.mean():.1f}%')

def pair(before_npz, after_npz, tag_b, tag_a):
    xb = load_aligned(before_npz); xa = load_aligned(after_npz)
    # shared normalization from the after (baseline) valid region
    v = xa[np.isfinite(xa).all(2)]
    lo = np.percentile(v, 2, 0); hi = np.percentile(v, 98, 0)
    render(xb, lo, hi, tag_b); render(xa, lo, hi, tag_a)

if __name__ == '__main__':
    which = sys.argv[1]
    if which == 'fuse':
        pair('lan/result_ablfus.npz', 'lan/result_v2.npz', 'xyz_fuse_b', 'xyz_fuse_a')
    elif which == 'ball':
        pair('lan/result_ablball.npz', 'lan/result_v2.npz', 'xyz_ball_b', 'xyz_ball_a')

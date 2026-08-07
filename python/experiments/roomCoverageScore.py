"""Per-surface room coverage: does the map actually cover the room?

Motivation: every metric we reported was conditioned on pixels that got
mapped -- accuracy on mapped pixels, coverage as a % of the OLD map's pixel
count, agreement on the overlap. All of them are blind to an entire missing
surface. This scores the complement: take the room's six faces and ask what
fraction of each one a map covers.

Up is +z, established from the cameras' own up-vectors (mean [0,0,1]).
"""
import numpy as np

GRID = 64        # cells across each face
SLAB = 0.12      # slab thickness as a fraction of the room's short axis


def load(npz, key=None):
    z = np.load(npz)
    M = z['M']
    if key == 'gt':
        return z['gt'][z['gt_ok']].astype(float)
    s = np.isfinite(z['dense_err'])
    p = z['dense_xyz'][s].astype(float)
    return (M[:3, :3] @ p.T).T + M[:3, 3]


def faces(lo, hi):
    """(name, fixed axis, which end, the two in-plane axes)"""
    return [('floor',    2, lo[2], (0, 1)),
            ('ceiling',  2, hi[2], (0, 1)),
            ('wall -X',  0, lo[0], (1, 2)),
            ('wall +X',  0, hi[0], (1, 2)),
            ('wall -Y',  1, lo[1], (0, 2)),
            ('wall +Y',  1, hi[1], (0, 2))]


def coverage(pts, lo, hi, thick):
    out = {}
    for name, ax, at, (a, b) in faces(lo, hi):
        near = np.abs(pts[:, ax] - at) < thick
        p = pts[near]
        if len(p) == 0:
            out[name] = (0.0, 0)
            continue
        ua = ((p[:, a] - lo[a]) / (hi[a] - lo[a]) * (GRID - 1)).astype(int)
        ub = ((p[:, b] - lo[b]) / (hi[b] - lo[b]) * (GRID - 1)).astype(int)
        m = (ua >= 0) & (ua < GRID) & (ub >= 0) & (ub < GRID)
        g = np.zeros((GRID, GRID), bool)
        g[ua[m], ub[m]] = True
        out[name] = (g.mean(), int(near.sum()))
    return out


if __name__ == '__main__':
    mine = load('lan/result_v2.npz')
    noball = load('lan/result_ablball.npz')
    gt = load('lan/result_v2.npz', 'gt')
    allp = np.vstack([mine, gt])
    lo, hi = np.percentile(allp, 0.5, 0), np.percentile(allp, 99.5, 0)
    thick = SLAB * (hi - lo).min()
    print(f'room box {np.round(hi-lo,3)}   slab {thick:.4f}   grid {GRID}x{GRID}\n')
    R = {n: coverage(p, lo, hi, thick)
         for n, p in [('model-free', mine), ('no ball mask', noball),
                      ('camamok', gt)]}
    hdr = f"{'surface':10s}" + ''.join(f'{k:>16s}' for k in R)
    print(hdr); print('-' * len(hdr))
    for name, *_ in faces(lo, hi):
        row = f'{name:10s}'
        for k in R:
            c, n = R[k][name]
            row += f'{100*c:11.1f}% {"":3s}'
        print(row)
    print()
    for k in R:
        vals = [R[k][n][0] for n, *_ in faces(lo, hi)]
        worst = min(zip(vals, [n for n, *_ in faces(lo, hi)]))
        print(f'{k:14s} mean face coverage {100*np.mean(vals):5.1f}%   '
              f'worst face: {worst[1]} at {100*worst[0]:.1f}%')

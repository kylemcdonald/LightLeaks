"""Top-K projector->camera candidates, instead of one winner per pixel.

A projector pixel lights TWO things at once: the specular glint on a mirror
ball (bright, moves with the viewpoint) and the leak dot where that light
actually lands (dimmer, fixed in space). Winner-take-all in projector space
always keeps the glint and discards the landing -- so the surface the map is
supposed to record never reaches the solver.

Keep the best camera pixel per coarse CAMERA CELL per projector bin, then the
top K cells. The glint and its landing are far apart in the image, so they
survive as separate candidates and multi-view geometry can choose between
them later.
"""
import os, sys, numpy as np

ROOT = 'lan_raw/SharedData'
PW, PH = 7680, 1080
BIN = 4                 # projector bin (matches drop_finest quantization)
CELL = 48               # camera-space cell for non-max suppression
K = 4                   # candidates kept per projector bin
CONF = 0.05


def group_last(sorted_keys):
    """index of the last element of each run in a sorted key array"""
    if len(sorted_keys) == 0:
        return np.zeros(0, np.int64)
    change = np.empty(len(sorted_keys), bool)
    change[-1] = True
    change[:-1] = sorted_keys[1:] != sorted_keys[:-1]
    return np.where(change)[0]


def topk_for_scan(scan):
    d = os.path.join(ROOT, scan)
    code = np.load(os.path.join(d, 'camCodeC.npy'), mmap_mode='r')
    conf = np.asarray(np.load(os.path.join(d, 'camConfC.npy'), mmap_mode='r'))
    H, W = conf.shape
    m = conf > CONF
    ys, xs = np.nonzero(m)
    cx = np.asarray(code[:, :, 0])[m]
    cy = np.asarray(code[:, :, 1])[m]
    cf = conf[m]
    bw, bh = PW // BIN, PH // BIN
    bx = np.clip((cx / BIN).astype(np.int64), 0, bw - 1)
    by = np.clip((cy / BIN).astype(np.int64), 0, bh - 1)
    binid = by * bw + bx
    ncw = (W + CELL - 1) // CELL
    cell = (ys // CELL).astype(np.int64) * ncw + (xs // CELL)
    # 1) best pixel per (bin, camera cell)
    key = binid * (ncw * ((H + CELL - 1) // CELL) + 1) + cell
    order = np.lexsort((cf, key))
    key_s = key[order]
    last = group_last(key_s)
    sel = order[last]
    b2, c2, x2, y2 = binid[sel], cf[sel], xs[sel], ys[sel]
    # 2) top-K cells per bin
    order2 = np.lexsort((c2, b2))
    b3 = b2[order2]
    starts = np.empty(len(b3), bool)
    starts[0] = True
    starts[1:] = b3[1:] != b3[:-1]
    grp = np.cumsum(starts) - 1
    # rank within group, counting from the end (highest conf last)
    cnt = np.bincount(grp)
    ends = np.cumsum(cnt)
    rank = np.arange(len(b3)) - np.repeat(ends - cnt, cnt)
    keep = rank >= (np.repeat(cnt, cnt) - K)
    idx = order2[keep]
    outb, outc = b2[idx], c2[idx]
    outx, outy = x2[idx], y2[idx]
    slot = (rank[keep] - (np.repeat(cnt, cnt)[keep] - K)).astype(np.int64)
    pm = np.zeros((bh * bw, K, 2), np.float32)
    pc = np.zeros((bh * bw, K), np.float32)
    pm[outb, slot, 0] = outx
    pm[outb, slot, 1] = outy
    pc[outb, slot] = outc
    return pm.reshape(bh, bw, K, 2), pc.reshape(bh, bw, K)


if __name__ == '__main__':
    scans = sys.argv[1:] or sorted(
        d for d in os.listdir(ROOT) if d.startswith('scan-') and
        os.path.exists(os.path.join(ROOT, d, 'camConfC.npy')))
    for s in scans:
        pm, pc = topk_for_scan(s)
        np.save(os.path.join(ROOT, s, 'proMapK.npy'), pm)
        np.save(os.path.join(ROOT, s, 'proConfK.npy'), pc)
        nz = (pc > 0).sum(-1)
        print(f'{s}: bins with >=1 cand {100*(nz>=1).mean():5.1f}%  '
              f'>=2 {100*(nz>=2).mean():5.1f}%  >=3 {100*(nz>=3).mean():5.1f}%')

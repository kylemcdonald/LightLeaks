"""Within-mirror coherence, projector space, old vs my final map.
Coherence = local xyz gradient to adjacent valid projector pixels
(low = a mirror's pixels land together = smooth). Same 1/4-res grid,
point-sampled, both aligned to the room frame."""
import numpy as np

z = np.load('lan/result_v2.npz')
dense = z['dense_xyz'].astype(np.float64)   # 270x1920x3, my frame
gt = z['gt'].astype(np.float64)             # 270x1920x3, room frame
solved = np.isfinite(z['dense_err'])
gt_ok = z['gt_ok']
M = z['M']
H, W, _ = dense.shape

# align mine -> room frame
flat = dense.reshape(-1, 3)
al = (M[:3, :3] @ flat.T).T + M[:3, 3]
mine = al.reshape(H, W, 3)
mine[~solved] = np.nan
g = gt.copy(); g[~gt_ok] = np.nan

# room diagonal for normalization (from gt extent)
gv = gt[gt_ok]
diag = np.linalg.norm(gv.max(0) - gv.min(0))

# MATCHED set: only projector pixels where BOTH maps are valid, and
# both horizontal neighbors valid in both maps (interior). Same pixel
# locations scored for each map -> isolates smoothness from coverage.
def nbr_jump(mapxyz, ax):
    a = mapxyz
    b = np.roll(mapxyz, -1, axis=ax)
    return np.linalg.norm(a - b, axis=2), (np.isfinite(a).all(2) &
                                           np.isfinite(b).all(2))

for ax, axn in [(1, 'horizontal'), (0, 'vertical')]:
    do, oko = nbr_jump(g, ax)
    dm, okm = nbr_jump(mine, ax)
    both = oko & okm            # identical pixel set for both maps
    jo, jm = do[both], dm[both]
    print(f"[{axn}] matched interior pixels n={both.sum()}")
    print(f"    OLD  neighbor jump: median {100*np.median(jo)/diag:.2f}%  "
          f"p90 {100*np.percentile(jo,90)/diag:.2f}%  "
          f"frac>2% {100*(jo>0.02*diag).mean():.1f}%")
    print(f"    MINE neighbor jump: median {100*np.median(jm)/diag:.2f}%  "
          f"p90 {100*np.percentile(jm,90)/diag:.2f}%  "
          f"frac>2% {100*(jm>0.02*diag).mean():.1f}%")
print(f"room diagonal = {diag:.3f}")

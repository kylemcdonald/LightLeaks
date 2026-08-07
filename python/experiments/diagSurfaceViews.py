"""Orthographic plan + elevation views of both maps, so missing surfaces
are visible instead of inferred."""
import numpy as np, cv2

z = np.load('lan/result_v2.npz')
M = z['M']; s = np.isfinite(z['dense_err'])
mine = (M[:3, :3] @ z['dense_xyz'][s].astype(float).T).T + M[:3, 3]
gt = z['gt'][z['gt_ok']].astype(float)

lo = np.minimum(np.percentile(mine, 0.5, 0), np.percentile(gt, 0.5, 0))
hi = np.maximum(np.percentile(mine, 99.5, 0), np.percentile(gt, 99.5, 0))

VIEWS = [((0, 1), 'PLAN  X-Y  (looking down)'),
         ((0, 2), 'ELEV  X-Z  (looking along Y)'),
         ((1, 2), 'ELEV  Y-Z  (looking along X)')]
W = 460
tiles = []
for (a, b), title in VIEWS:
    ar = (hi[b]-lo[b]) / (hi[a]-lo[a])
    H = max(120, int(W*ar))
    row = []
    for pts, col in [(gt, (150, 150, 155)), (mine, (196, 200, 60))]:
        img = np.full((H, W, 3), 12, np.uint8)
        u = ((pts[:, a]-lo[a])/(hi[a]-lo[a])*(W-1)).astype(int)
        v = ((1-(pts[:, b]-lo[b])/(hi[b]-lo[b]))*(H-1)).astype(int)
        ok = (u >= 0) & (u < W) & (v >= 0) & (v < H)
        np.add.at(img, (v[ok], u[ok]), np.array(col, np.uint8)//3)
        row.append(np.clip(img, 0, 255))
    pad = np.full((H, 8, 3), 30, np.uint8)
    tiles.append((title, np.hstack([row[0], pad, row[1]])))

hdr = 30
total = sum(t.shape[0]+hdr+10 for _, t in tiles)+40
canvas = np.full((total, tiles[0][1].shape[1], 3), 12, np.uint8)
y = 10
cv2.putText(canvas, 'camamok GT (grey)        |        model-free (cyan)',
            (8, y+16), cv2.FONT_HERSHEY_SIMPLEX, .45, (200, 200, 205), 1, cv2.LINE_AA)
y += 34
for title, t in tiles:
    cv2.putText(canvas, title, (8, y+18), cv2.FONT_HERSHEY_SIMPLEX, .45,
                (120, 200, 195), 1, cv2.LINE_AA)
    y += hdr
    canvas[y:y+t.shape[0], :t.shape[1]] = t
    y += t.shape[0]+10
cv2.imwrite('figs/diag_surfaces.png', canvas)
print('wrote figs/diag_surfaces.png', canvas.shape)

"""Cross-section comparison: slice ground truth and reconstruction
clouds and overlay them - wall/floor profiles make accuracy legible."""
import os
import sys
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
result = sys.argv[1] if len(sys.argv) > 1 else \
    os.path.join(HERE, 'llum', 'result.npz')
title = sys.argv[2] if len(sys.argv) > 2 else 'Llum 2019'
out = sys.argv[3] if len(sys.argv) > 3 else \
    os.path.join(HERE, 'slice_comparison.png')

d = np.load(result)
xyz = d['dense_xyz'].reshape(-1, 3)
err = d['dense_err'].reshape(-1)
gt = d['gt'].reshape(-1, 3)
gt_ok = d['gt_ok'].reshape(-1)
M = d['M']

ours_ok = np.isfinite(err) & (err < 3)
ours = xyz[ours_ok] @ M[:3, :3].T + M[:3, 3]
gt_pts = gt[gt_ok]

allpts = np.vstack([gt_pts, ours])
lo = np.percentile(allpts, 1, 0)
hi = np.percentile(allpts, 99, 0)
span = np.linalg.norm(hi - lo)

both = gt_ok & ours_ok
ours_b = xyz[both] @ M[:3, :3].T + M[:3, 3]
e_pct = 100 * np.linalg.norm(ours_b - gt[both], axis=1) / span

fig, axes = plt.subplots(2, 3, figsize=(19, 10.5), facecolor='white')
fig.suptitle(
    f'How closely does the model-free calibration match camamok? — {title}\n'
    'cross-sections through both point clouds, ground truth GREY vs '
    f'reconstruction RED. median disagreement {np.median(e_pct):.1f}% '
    f'of scene diagonal, 80% of pixels within '
    f'{np.percentile(e_pct, 80):.1f}%', fontsize=14)


def slice_plot(ax, axis_sel, band_axis, band_lo, band_hi, ax_x, ax_y,
               name, zoom=None):
    for pts, color, size, alpha, label in (
            (gt_pts, '0.45', 2.5, 0.55, 'camamok ground truth'),
            (ours, 'crimson', 2.5, 0.55, 'model-free')):
        sel = (pts[:, band_axis] >= band_lo) & \
              (pts[:, band_axis] <= band_hi)
        p = pts[sel]
        ax.scatter(p[:, ax_x], p[:, ax_y], s=size, c=color, alpha=alpha,
                   linewidths=0, label=label)
    ax.set_aspect('equal')
    ax.set_title(name, fontsize=11)
    if zoom:
        ax.set_xlim(zoom[0], zoom[1])
        ax.set_ylim(zoom[2], zoom[3])
    ax.tick_params(labelsize=7)
    # 5%-of-diagonal scale bar
    bar = 0.05 * span
    x0, y0 = ax.get_xlim()[0], ax.get_ylim()[0]
    xr = ax.get_xlim()[1] - x0
    yr = ax.get_ylim()[1] - y0
    ax.plot([x0 + 0.06*xr, x0 + 0.06*xr + bar], [y0 + 0.06*yr]*2,
            color='k', lw=2)
    ax.text(x0 + 0.06*xr + bar/2, y0 + 0.085*yr, '5% of diagonal',
            ha='center', fontsize=7)


# choose slice bands from data density
y_mid = np.percentile(gt_pts[:, 1], 45)
y_band = 0.03 * span
plan_zoom = None

slice_plot(axes[0][0], None, 1, y_mid - y_band, y_mid + y_band, 0, 2,
           f'PLAN slice (horizontal cut at mid height)\n'
           'walls should trace the same lines')
axes[0][0].legend(loc='upper right', fontsize=8, markerscale=4)

# zoom into densest wall region of the plan slice
selg = (gt_pts[:, 1] >= y_mid - y_band) & (gt_pts[:, 1] <= y_mid + y_band)
pg = gt_pts[selg]
cx, cz = np.percentile(pg[:, 0], 50), np.percentile(pg[:, 2], 50)
w = 0.18 * span
slice_plot(axes[0][1], None, 1, y_mid - y_band, y_mid + y_band, 0, 2,
           'same slice, zoomed', zoom=(cx - w, cx + w, cz - w, cz + w))

# elevation slice through scene center
z_mid = np.percentile(gt_pts[:, 2], 50)
z_band = 0.03 * span
slice_plot(axes[0][2], None, 2, z_mid - z_band, z_mid + z_band, 0, 1,
           'ELEVATION slice (vertical cut)\nfloor and ball profile')

# projector-map patch: find densest region of common coverage
step = int(d['step'])
both_img = d['both']
h, w2 = both_img.shape
import scipy.ndimage as ndi
density = ndi.uniform_filter(both_img.astype(float), size=41)
py, px = np.unravel_index(np.argmax(density), density.shape)
r = 90
sl = (slice(max(0, py - r), min(h, py + r)),
      slice(max(0, px - r), min(w2, px + r)))
gt_img = d['gt'][sl]
ours_img = d['dense_xyz'][sl].copy()
err_img = d['dense_err'][sl]
Mi = M
ours_img = ours_img @ Mi[:3, :3].T + Mi[:3, 3]
ok_img = np.isfinite(err_img) & (err_img < 3)
gtok_img = d['gt_ok'][sl]


def to_rgb(img, mask):
    rgb = np.clip((img - lo) / (hi - lo), 0, 1)
    rgb[~mask] = 0.08
    return rgb


axes[1][0].imshow(to_rgb(gt_img, gtok_img), interpolation='nearest')
axes[1][0].set_title('projector-space map: ground truth\n'
                     '(zoomed patch, xyz as color)', fontsize=10)
axes[1][1].imshow(to_rgb(ours_img, ok_img), interpolation='nearest')
axes[1][1].set_title('projector-space map: model-free\n'
                     '(same patch - same colors = same 3d point)',
                     fontsize=10)
diff = np.full(err_img.shape, np.nan)
bo = ok_img & gtok_img
diff[bo] = 100 * np.linalg.norm(ours_img[bo] - gt_img[bo],
                                axis=-1) / span
im = axes[1][2].imshow(diff, cmap='inferno', vmin=0, vmax=10,
                       interpolation='nearest')
axes[1][2].set_title('per-pixel disagreement\n[% of scene diagonal]',
                     fontsize=10)
plt.colorbar(im, ax=axes[1][2], shrink=0.8)
for a in axes[1]:
    a.set_xticks([])
    a.set_yticks([])

plt.tight_layout(rect=(0, 0, 1, 0.9))
plt.savefig(out, dpi=115)
print('wrote', out)

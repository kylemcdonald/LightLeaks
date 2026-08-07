"""Side-by-side 3d comparison: camamok ground truth vs model-free
reconstruction, rendered identically so structural agreement is
directly visible."""
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
    os.path.join(HERE, 'comparison_render.png')

d = np.load(result)
xyz = d['dense_xyz'].reshape(-1, 3)
err = d['dense_err'].reshape(-1)
gt = d['gt'].reshape(-1, 3)
gt_ok = d['gt_ok'].reshape(-1)
M = d['M']

ours_ok = np.isfinite(err) & (err < 3)
from_isl = xyz[ours_ok] @ M[:3, :3].T + M[:3, 3]   # into GT frame
gt_pts = gt[gt_ok]

# shared color mapping: normalized xyz -> rgb over the union bounds
allpts = np.vstack([gt_pts, from_isl])
lo = np.percentile(allpts, 1, axis=0)
hi = np.percentile(allpts, 99, axis=0)
span = np.linalg.norm(hi - lo)


def colorize(p):
    return np.clip((p - lo) / (hi - lo), 0, 1)


# per-point error of ours vs nearest GT context: use the paired pixels
both = gt_ok & ours_ok
ours_b = xyz[both] @ M[:3, :3].T + M[:3, 3]
e_pair = np.linalg.norm(ours_b - gt[both], axis=1)
e_pct = 100 * e_pair / span

rng = np.random.default_rng(0)


def sub(p, n=25000):
    if len(p) <= n:
        return np.arange(len(p))
    return rng.choice(len(p), n, replace=False)


fig = plt.figure(figsize=(19, 11), facecolor='white')
fig.suptitle(
    f'Ground truth vs model-free reconstruction — {title}\n'
    f'identical viewpoints and colors; '
    f'{both.sum():,} directly comparable projector pixels, '
    f'median disagreement {np.median(e_pct):.1f}% of scene diagonal',
    fontsize=14)

views = [('view A', 18, -65), ('view B (top-down)', 75, -90)]
for row, (vname, elev, azim) in enumerate(views):
    for col, (pts, label) in enumerate([
            (gt_pts, 'camamok ground truth\n(manual: 3d model + clicking)'),
            (from_isl, 'model-free reconstruction\n(scans only, aligned)')]):
        ax = fig.add_subplot(2, 3, row * 3 + col + 1, projection='3d')
        s_idx = sub(pts)
        p = pts[s_idx]
        ax.scatter(p[:, 0], p[:, 2], p[:, 1], c=colorize(p), s=1.2,
                   alpha=0.75, linewidths=0)
        ax.set_xlim(lo[0], hi[0])
        ax.set_ylim(lo[2], hi[2])
        ax.set_zlim(lo[1], hi[1])
        ax.set_box_aspect((hi[0]-lo[0], hi[2]-lo[2],
                           max(hi[1]-lo[1], 1e-3)))
        ax.view_init(elev=elev, azim=azim)
        ax.set_axis_off()
        if row == 0:
            ax.set_title(label, fontsize=11)
        else:
            ax.text2D(0.5, -0.02, vname, transform=ax.transAxes,
                      ha='center', fontsize=9, color='0.4')

    # overlay: GT gray, ours red
    ax = fig.add_subplot(2, 3, row * 3 + 3, projection='3d')
    s1 = sub(gt_pts, 20000)
    s2 = sub(from_isl, 20000)
    ax.scatter(gt_pts[s1, 0], gt_pts[s1, 2], gt_pts[s1, 1],
               c='0.65', s=1.0, alpha=0.5, linewidths=0,
               label='ground truth')
    ax.scatter(from_isl[s2, 0], from_isl[s2, 2], from_isl[s2, 1],
               c='crimson', s=1.0, alpha=0.5, linewidths=0,
               label='model-free')
    ax.set_xlim(lo[0], hi[0])
    ax.set_ylim(lo[2], hi[2])
    ax.set_zlim(lo[1], hi[1])
    ax.set_box_aspect((hi[0]-lo[0], hi[2]-lo[2], max(hi[1]-lo[1], 1e-3)))
    ax.view_init(elev=elev, azim=azim)
    ax.set_axis_off()
    if row == 0:
        ax.set_title('overlay — grey GT, red ours\n'
                     '(structures should coincide)', fontsize=11)
        ax.legend(loc='upper right', fontsize=8, markerscale=6)

# inset: cumulative error curve
axc = fig.add_axes((0.055, 0.06, 0.16, 0.13))
xs = np.sort(e_pct)
axc.plot(xs, np.arange(1, len(xs)+1) / len(xs) * 100, color='#4878cf')
axc.axvline(np.median(e_pct), color='crimson', ls='--', lw=0.8)
axc.set_xlim(0, min(15, xs[-1]))
axc.set_ylim(0, 100)
axc.set_xlabel('disagreement [% of scene diagonal]', fontsize=8)
axc.set_ylabel('% of pixels', fontsize=8)
axc.tick_params(labelsize=7)
axc.set_title(f'median {np.median(e_pct):.1f}%  ·  '
              f'80% of pixels < {np.percentile(e_pct, 80):.1f}%',
              fontsize=8)

plt.tight_layout(rect=(0, 0, 1, 0.92))
plt.savefig(out, dpi=115)
print('wrote', out)

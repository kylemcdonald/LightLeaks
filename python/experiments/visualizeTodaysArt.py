"""Render TodaysArt real-data validation: recovered vs camamok GT."""
import os
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
d = np.load(os.path.join(HERE, 'todaysart', 'result.npz'))
dense_xyz = d['dense_xyz']
dense_err = d['dense_err']
gt = d['gt']
gt_ok = d['gt_ok']
both = d['both']
M = d['M']
names = d['view_names']
vp = d['view_params']

h, w, _ = gt.shape
aligned = dense_xyz.reshape(-1, 3) @ M[:3, :3].T + M[:3, 3]
aligned = aligned.reshape(h, w, 3)

err = np.linalg.norm(aligned - gt, axis=2)
err_show = np.where(both, err, np.nan)

lo = np.nanpercentile(np.where(gt_ok[..., None], gt, np.nan), 2, (0, 1))
hi = np.nanpercentile(np.where(gt_ok[..., None], gt, np.nan), 98, (0, 1))


def to_rgb(xyz, mask):
    rgb = (xyz - lo) / (hi - lo)
    rgb = np.clip(rgb, 0, 1)
    rgb[~mask] = 0.08
    return rgb


ours_ok = np.isfinite(dense_err) & (dense_err < 3.0)
err_med = np.median(err[both])
err_p90 = np.percentile(err[both], 90)
extent = np.linalg.norm(hi - lo)

fig, axes = plt.subplots(4, 1, figsize=(15, 12.5), facecolor='white')
fig.suptitle(
    'Model-free calibration on REAL data — TodaysArt 2018 '
    '(3 projectors @1400x1050, 15 scans)\n'
    f'projector-space XYZ map, no 3d model / no camamok vs camamok '
    f'ground truth: median error {err_med:.3f}, p90 {err_p90:.3f} '
    f'(room extent {extent:.1f} model units)',
    fontsize=13)

axes[0].imshow(to_rgb(gt, gt_ok), aspect='auto', interpolation='nearest')
axes[0].set_title('camamok-era ground truth xyzMap-0.exr (xyz→rgb)',
                  fontsize=10)
axes[1].imshow(to_rgb(aligned, ours_ok), aspect='auto',
               interpolation='nearest')
axes[1].set_title('model-free reconstruction (aligned, same color '
                  'mapping) — from gray-code correspondences only',
                  fontsize=10)
im = axes[2].imshow(err_show, aspect='auto', cmap='inferno',
                    vmin=0, vmax=min(5 * err_med, np.nanmax(err_show)),
                    interpolation='nearest')
axes[2].set_title('per-pixel error vs ground truth (model units)',
                  fontsize=10)
fig.colorbar(im, ax=axes[2], fraction=0.025, pad=0.01)
for ax in axes[:3]:
    ax.set_xticks([])
    ax.set_yticks([])
    for x in (w / 3, 2 * w / 3):
        ax.axvline(x, color='white', lw=0.5, alpha=0.5)

ax4 = axes[3]
ax4.hist(err[both], bins=80, range=(0, 5 * err_med), color='#4878cf',
         edgecolor='white')
ax4.axvline(err_med, color='crimson', ls='--',
            label=f'median {err_med:.3f}')
ax4.axvline(err_p90, color='orange', ls='--', label=f'p90 {err_p90:.3f}')
ax4.set_xlabel('error [model units]')
ax4.set_ylabel('pixels')
ax4.legend(fontsize=9)
cov_ours = ours_ok.mean() * 100
cov_gt = gt_ok.mean() * 100
lines = [f'coverage: ours {cov_ours:.1f}%  camamok {cov_gt:.1f}%',
         '', 'recovered views (f, pp, k1):']
for n, p in zip(names, vp):
    if p[10] > 0:
        lines.append(f'{n}: f={p[6]:.0f} pp=({p[7]:.0f},{p[8]:.0f}) '
                     f'k1={p[9]:.3f}')
ax4.text(0.99, 0.97, '\n'.join(lines), transform=ax4.transAxes,
         fontsize=7, family='monospace', ha='right', va='top')

plt.tight_layout(rect=(0, 0, 1, 0.93))
out = os.path.join(HERE, 'todaysart_validation.png')
plt.savefig(out, dpi=110)
print('saved', out)

"""Spinning turntable GIF: mapped pixels (colored by position) plus
recovered camera positions."""
import os
import sys
import io
import numpy as np
import cv2
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
result = sys.argv[1] if len(sys.argv) > 1 else \
    os.path.join(HERE, 'llum', 'result.npz')
title = sys.argv[2] if len(sys.argv) > 2 else 'Llum 2019'
out = sys.argv[3] if len(sys.argv) > 3 else \
    os.path.join(HERE, 'turntable.gif')

d = np.load(result)
xyz = d['dense_xyz'].reshape(-1, 3)
err = d['dense_err'].reshape(-1)
src = d['source'].reshape(-1) if 'source' in d else \
    np.where(np.isfinite(err), 2, 0)
ok = src > 0
pts = xyz[ok]

vp = d['view_params']
names = [str(n) for n in d['view_names']]
cams = []
for k, p in enumerate(vp):
    if p[10] > 0:
        R = cv2.Rodrigues(p[0:3])[0]
        C = -R.T @ p[3:6]
        fwd = R.T @ np.array([0, 0, 1.0])
        cams.append((names[k].replace('scan-', ''), C, fwd))

rng = np.random.default_rng(0)
if len(pts) > 40000:
    pts = pts[rng.choice(len(pts), 40000, replace=False)]

lo = np.percentile(pts, 1, 0)
hi = np.percentile(pts, 99, 0)
col = np.clip((pts - lo) / (hi - lo), 0, 1)
span = np.linalg.norm(hi - lo)

frames = []
n_frames = 48
for fi in range(n_frames):
    az = -60 + fi * 360.0 / n_frames
    fig = plt.figure(figsize=(8.4, 6.6), facecolor='black')
    ax = fig.add_subplot(111, projection='3d', facecolor='black')
    ax.scatter(pts[:, 0], pts[:, 2], pts[:, 1], c=col, s=1.1,
               alpha=0.75, linewidths=0)
    for nm, C, fwd in cams:
        ax.scatter([C[0]], [C[2]], [C[1]], marker='o', s=52,
                   color='white', edgecolors='crimson', linewidths=1.6,
                   zorder=5)
        tip = C + fwd * 0.06 * span
        ax.plot([C[0], tip[0]], [C[2], tip[2]], [C[1], tip[1]],
                color='crimson', lw=1.6)
        ax.text(C[0], C[2], C[1] + 0.03 * span, nm, fontsize=6.5,
                color='white', ha='center')
    ax.set_xlim(lo[0], hi[0])
    ax.set_ylim(lo[2], hi[2])
    ax.set_zlim(lo[1], hi[1])
    ax.set_box_aspect((hi[0] - lo[0], hi[2] - lo[2],
                       max(hi[1] - lo[1], 1e-3)))
    ax.view_init(elev=24, azim=az)
    ax.set_axis_off()
    ax.text2D(0.5, 0.965, f'{title} — mapped pixels + recovered '
              'cameras', transform=ax.transAxes, color='white',
              fontsize=11, ha='center')
    fig.subplots_adjust(left=0, right=1, top=1, bottom=0)
    buf = io.BytesIO()
    fig.savefig(buf, format='png', dpi=95, facecolor='black')
    plt.close(fig)
    buf.seek(0)
    frames.append(Image.open(buf).convert('P',
                                          palette=Image.ADAPTIVE,
                                          colors=200))

frames[0].save(out, save_all=True, append_images=frames[1:],
               duration=110, loop=0, optimize=True)
print('wrote', out, f'({os.path.getsize(out)/1e6:.1f} MB)')

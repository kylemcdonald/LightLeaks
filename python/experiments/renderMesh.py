"""Render a reconstructed mesh + cloud with matplotlib (no GPU/EGL)."""
import os
import sys
import numpy as np
import open3d as o3d
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

stem = sys.argv[1] if len(sys.argv) > 1 else \
    os.path.join(os.path.dirname(os.path.abspath(__file__)),
                 'todaysart_mesh')
title = sys.argv[2] if len(sys.argv) > 2 else 'TodaysArt 2018'

mesh = o3d.io.read_triangle_mesh(stem + '.ply')
mesh = mesh.simplify_quadric_decimation(9000)
mesh.compute_triangle_normals()
V = np.asarray(mesh.vertices)
T = np.asarray(mesh.triangles)
N = np.asarray(mesh.triangle_normals)
pcd = o3d.io.read_point_cloud(stem + '_cloud.ply')
P = np.asarray(pcd.points)

light = np.array([0.4, 0.8, 0.45])
light = light / np.linalg.norm(light)
shade = 0.35 + 0.65 * np.abs(N @ light)

fig = plt.figure(figsize=(19, 6.8), facecolor='white')
views = [('front', 12, -88), ('corner', 28, -50), ('top-down', 80, -90)]
for k, (name, elev, azim) in enumerate(views):
    ax = fig.add_subplot(1, 3, k + 1, projection='3d')
    tris = V[T]
    # painter order: sort by depth along view direction
    a, e = np.radians(azim), np.radians(elev)
    view_dir = np.array([np.cos(e) * np.cos(a), np.cos(e) * np.sin(a),
                         np.sin(e)])
    order = np.argsort(tris.mean(1) @ np.array(
        [view_dir[0], view_dir[2], view_dir[1]]))
    polys = tris[order][:, :, [0, 2, 1]]
    cols = plt.cm.bone(0.25 + 0.6 * shade[order])
    pc = Poly3DCollection(polys, facecolors=cols, edgecolors='none')
    ax.add_collection3d(pc)
    ax.scatter(P[::4, 0], P[::4, 2], P[::4, 1], s=0.3, c='crimson',
               alpha=0.35, linewidths=0)
    lo = V.min(0)
    hi = V.max(0)
    ax.set_xlim(lo[0], hi[0])
    ax.set_ylim(lo[2], hi[2])
    ax.set_zlim(lo[1], hi[1])
    ax.set_box_aspect((hi[0] - lo[0], hi[2] - lo[2],
                       max(hi[1] - lo[1], 1e-3)))
    ax.view_init(elev=elev, azim=azim)
    ax.set_title(name, fontsize=11)
    ax.set_axis_off()
fig.suptitle(
    f'3d model reconstructed from gray-code scans alone — {title}\n'
    f'Poisson mesh ({len(T)} triangles, grey) + triangulated points '
    '(red). No model.dae, no camamok.', fontsize=13)
plt.tight_layout(rect=(0, 0, 1, 0.9))
out = stem + '_render.png'
plt.savefig(out, dpi=110)
print('wrote', out)

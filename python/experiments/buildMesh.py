"""Reconstruct a 3d mesh from the dense triangulated point cloud.

Reads result.npz from a calibration run, cleans the cloud, estimates
normals, runs Poisson surface reconstruction, and exports .ply plus
rendered views. No 3d model was used to produce the input - this IS
the recovered model of the space.
"""
import os
import sys
import numpy as np
import open3d as o3d

HERE = os.path.dirname(os.path.abspath(__file__))
result = sys.argv[1] if len(sys.argv) > 1 else \
    os.path.join(HERE, 'todaysart', 'result.npz')
out_stem = sys.argv[2] if len(sys.argv) > 2 else \
    os.path.join(HERE, 'todaysart_mesh')

d = np.load(result)
xyz = d['dense_xyz'].reshape(-1, 3)
err = d['dense_err'].reshape(-1)
ok = np.isfinite(err) & (err < 2.0)
pts = xyz[ok]
print(f"{ok.sum()} points from dense map")

pcd = o3d.geometry.PointCloud()
pcd.points = o3d.utility.Vector3dVector(pts)

# statistical outlier removal: kill lone flying points
pcd, _ = pcd.remove_statistical_outlier(nb_neighbors=16, std_ratio=2.0)
print(f"{len(pcd.points)} points after outlier removal")

# normals from local neighborhoods, oriented consistently
bbox = pcd.get_axis_aligned_bounding_box()
scale = np.linalg.norm(bbox.get_extent())
pcd.estimate_normals(o3d.geometry.KDTreeSearchParamHybrid(
    radius=scale * 0.02, max_nn=30))
pcd.orient_normals_consistent_tangent_plane(30)

mesh, densities = o3d.geometry.TriangleMesh.create_from_point_cloud_poisson(
    pcd, depth=8)
# trim poisson's hallucinated surface where there was no data
densities = np.asarray(densities)
mesh.remove_vertices_by_mask(densities < np.quantile(densities, 0.06))
mesh.remove_degenerate_triangles()
mesh.remove_unreferenced_vertices()
print(f"mesh: {len(mesh.vertices)} vertices, {len(mesh.triangles)} tris")

o3d.io.write_point_cloud(out_stem + '_cloud.ply', pcd)
o3d.io.write_triangle_mesh(out_stem + '.ply', mesh)
print("wrote", out_stem + '.ply')

# offscreen renders from a few angles
mesh.compute_vertex_normals()
mesh.paint_uniform_color([0.75, 0.78, 0.85])
try:
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt

    renders = []
    center = mesh.get_center()
    for name, eye_off in [('front', [0, 0.25, 1.2]),
                          ('corner', [1.0, 0.7, 1.0]),
                          ('top', [0.02, 1.6, 0.02])]:
        r = o3d.visualization.rendering.OffscreenRenderer(1280, 860)
        mat = o3d.visualization.rendering.MaterialRecord()
        mat.shader = 'defaultLit'
        r.scene.add_geometry('mesh', mesh, mat)
        pmat = o3d.visualization.rendering.MaterialRecord()
        pmat.shader = 'defaultUnlit'
        pmat.point_size = 2.0
        r.scene.add_geometry('cloud', pcd, pmat)
        r.scene.set_background([1, 1, 1, 1])
        eye = center + np.array(eye_off) * scale
        r.setup_camera(60.0, center, eye, [0, 1, 0])
        img = np.asarray(r.render_to_image())
        renders.append((name, img))
    fig, axes = plt.subplots(1, len(renders),
                             figsize=(6.5 * len(renders), 5.2))
    for ax, (name, img) in zip(np.atleast_1d(axes), renders):
        ax.imshow(img)
        ax.set_title(name)
        ax.axis('off')
    fig.suptitle('Poisson mesh reconstructed from gray-code scans only '
                 f'({len(mesh.vertices)} vertices)', fontsize=13)
    plt.tight_layout()
    plt.savefig(out_stem + '_render.png', dpi=100)
    print("wrote", out_stem + '_render.png')
except Exception as e:
    print("offscreen render failed:", e)

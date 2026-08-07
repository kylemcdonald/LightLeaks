"""Rebuild the interactive viewer's DATA blob from the current v2 result.
Encoding must match viewer decode(): int16 q, out=(q+32768)/65535 in [0,1]
normalized within the shared bbox."""
import numpy as np, base64, json, cv2

MAXPTS = 60000
rng = np.random.default_rng(7)

def enc(pts):
    """pts (N,3) already normalized to [0,1] -> base64 int16"""
    q = np.clip(pts, 0, 1) * 65535.0 - 32768.0
    return base64.b64encode(q.astype('<i2').tobytes()).decode()

def cam_centers(view_params):
    C = []
    for row in view_params:
        rvec = row[0:3]; tvec = row[3:6]; reg = row[10]
        if reg < 0.5:
            continue
        R, _ = cv2.Rodrigues(np.asarray(rvec, float))
        C.append((-R.T @ np.asarray(tvec, float)))
    return np.array(C) if C else np.zeros((0, 3))

def build(npz, names_key='view_names'):
    z = np.load(npz, allow_pickle=True)
    M = z['M']
    solved = np.isfinite(z['dense_err'])
    d = z['dense_xyz'].astype(np.float64)
    ours = d[solved]
    ours = (M[:3, :3] @ ours.T).T + M[:3, 3]           # -> room frame
    gt = z['gt'].astype(np.float64)[z['gt_ok']]
    cams = cam_centers(z['view_params'])
    if len(cams):
        cams = (M[:3, :3] @ cams.T).T + M[:3, 3]
    names = [str(n).replace('scan-', '') for n, row in
             zip(z[names_key], z['view_params']) if row[10] >= 0.5]
    # shared bbox from gt+ours (robust percentiles so outliers don't crush it)
    allp = np.vstack([ours, gt])
    lo = np.percentile(allp, 0.5, axis=0); hi = np.percentile(allp, 99.5, axis=0)
    def sub(a, n):
        if len(a) > n:
            a = a[rng.choice(len(a), n, replace=False)]
        return a
    ours_s, gt_s = sub(ours, MAXPTS), sub(gt, MAXPTS)
    nrm = lambda a: (a - lo) / (hi - lo)
    return {
        'lo': lo.tolist(), 'hi': hi.tolist(),
        'ours': enc(nrm(ours_s)), 'n_ours': len(ours_s),
        'gt': enc(nrm(gt_s)), 'n_gt': len(gt_s),
        'cams': enc(nrm(cams)), 'n_cams': len(cams),
        'cam_names': names,
    }

out = {'LightsAllNight': build('lan/result_v2.npz'), 'Llum': build('llum/result.npz')}
open('viewer_DATA.js', 'w').write('const DATA = ' + json.dumps(out) + ';')
for k,d in out.items(): print(k, d['n_ours'], d['n_gt'], d['n_cams'], d['cam_names'])
d = out['LightsAllNight']
print('LAN: ours', d['n_ours'], 'gt', d['n_gt'], 'cams', d['n_cams'], d['cam_names'])

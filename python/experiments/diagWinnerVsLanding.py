"""For missing-plane pixels: does ANY camera's winner correspond to
camamok's landing? If not, the leak dot never wins the per-projector-pixel
competition and no fusion rule downstream can recover it."""
import os, numpy as np, cv2
ROOT='lan_raw/SharedData'; STEP=4; PW,PH=7680,1080; CONF=0.05
z=np.load('lan/result_v2.npz',allow_pickle=True); M=z['M']; Minv=np.linalg.inv(M)
gt=z['gt'].astype(float); gt_ok=z['gt_ok']; zc=np.percentile(gt[gt_ok][:,2],99.5)
plane=gt_ok&(gt[:,:,2]>0.85*zc)
gt_nat=((Minv[:3,:3]@gt.reshape(-1,3).T).T+Minv[:3,3]).reshape(gt.shape)
dy,dx=np.mgrid[0:PH:STEP,0:PW:STEP]
names=[str(n) for n in z['view_names']]
hits=np.zeros(plane.shape,np.int16); seen=np.zeros(plane.shape,np.int16)
for nm,row in zip(names,z['view_params']):
    if row[10]<0.5: continue
    p=os.path.join(ROOT,nm)
    if not os.path.exists(os.path.join(p,'proConfC.npy')): continue
    conf=np.asarray(np.load(os.path.join(p,'proConfC.npy'),mmap_mode='r')[dy,dx])
    pm=np.asarray(np.load(os.path.join(p,'proMapC.npy'),mmap_mode='r')[dy,dx])
    sel=plane&(conf>CONF); seen+=sel
    X=gt_nat[sel]
    R,_=cv2.Rodrigues(np.asarray(row[0:3],float)); t=np.asarray(row[3:6],float)
    f,cx,cy,k1=row[6],row[7],row[8],row[9]
    cam=(R@X.T).T+t; fr=cam[:,2]>1e-9
    x=cam[:,0]/np.where(fr,cam[:,2],np.nan); y=cam[:,1]/np.where(fr,cam[:,2],np.nan)
    r2=x*x+y*y; d=1+k1*r2
    u=f*d*x+cx; v=f*d*y+cy
    obs=pm[sel]
    e=np.hypot(u-obs[:,0],v-obs[:,1])
    h=np.zeros(plane.shape,bool); h[sel]=np.isfinite(e)&(e<30)
    hits+=h
m=plane&(seen>=2)
print(f'missing-plane pixels seen by >=2 cams: {m.sum()}')
v=hits[m]
print(f'  cameras whose WINNER matches camamok landing (<30px):')
for k in range(0,5):
    print(f'    {k} cams: {100*(v==k).mean():5.1f}%')
print(f'    >=3 cams: {100*(v>=3).mean():5.1f}%')
print()
print('=> if most pixels have 0-1 matching winners, the leak dot is not a winner;')
print('   the fix must add candidates BEFORE winner-take-all, not after.')

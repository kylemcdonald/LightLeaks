import numpy as np, cv2
Y0,Y1,X0,X1=10,90,30,190
def load(npz):
    z=np.load(npz); d=z['dense_xyz'].astype(np.float64); s=np.isfinite(z['dense_err']); M=z['M']
    H,W,_=d.shape; al=(M[:3,:3]@d.reshape(-1,3).T).T+M[:3,3]; x=al.reshape(H,W,3); x[~s]=np.nan; return x
xb=load('lan/result_ablfus.npz'); xa=load('lan/result_v2.npz')
v=xa[np.isfinite(xa).all(2)]; diag=np.linalg.norm(np.percentile(v,98,0)-np.percentile(v,2,0))
cb=xb[Y0:Y1,X0:X1]; ca=xa[Y0:Y1,X0:X1]
okb=np.isfinite(cb).all(2); oka=np.isfinite(ca).all(2); both=okb&oka
dist=np.linalg.norm(cb-ca,axis=2)
changed=both&(dist>0.02*diag)
# base = after map (muted), changed pixels highlighted coral
lo=np.percentile(v,2,0); hi=np.percentile(v,98,0)
norm=np.clip((ca-lo)/(hi-lo),0,1); rgb=(norm[...,::-1]*255*0.32).astype(np.uint8)  # dim after-map
rgb[~oka]=(16,17,20)
rgb[changed]=(255,255,255)
up=cv2.resize(rgb,((X1-X0)*6,(Y1-Y0)*6),interpolation=cv2.INTER_NEAREST)
cv2.imwrite('figs/xyz_fuse_diff.png',up)
print(f'changed pixels in crop: {changed.sum()} of {both.sum()} both-valid ({100*changed.sum()/max(both.sum(),1):.0f}%)')

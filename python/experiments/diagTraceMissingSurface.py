"""Where do the missing-plane projector pixels die?"""
import os, numpy as np
os.environ["OPENCV_IO_ENABLE_OPENEXR"]="1"
ROOT='lan_raw/SharedData'; STEP=4; PW,PH=7680,1080; CONF=0.05
z=np.load('lan/result_v2.npz',allow_pickle=True)
gt=z['gt'].astype(float); gt_ok=z['gt_ok']
gtall=gt[gt_ok]; zc=np.percentile(gtall[:,2],99.5)
plane = gt_ok & (gt[:,:,2] > 0.85*zc)      # the plane holding 74% of camamok
other = gt_ok & ~plane
print(f'grid {gt.shape[:2]}  GT pixels {gt_ok.sum()}  on missing plane {plane.sum()}  elsewhere {other.sum()}')

scans=sorted(d for d in os.listdir(ROOT) if d.startswith('scan-')
             and os.path.exists(os.path.join(ROOT,d,'proConfC.npy')))
dy,dx=np.mgrid[0:PH:STEP,0:PW:STEP]
nok=np.zeros(dy.shape,np.int16)
for s in scans:
    c=np.load(os.path.join(ROOT,s,'proConfC.npy'),mmap_mode='r')
    nok += (np.asarray(c[dy,dx])>CONF).astype(np.int16)
    del c
np.save('figs/_nok.npy',nok)
for nm,m in [('MISSING PLANE',plane),('elsewhere',other)]:
    v=nok[m]
    print(f'{nm:15s} n={m.sum():7d}  decoded by 0 cams {100*(v==0).mean():5.1f}%  '
          f'1 cam {100*(v==1).mean():5.1f}%  >=2 cams {100*(v>=2).mean():5.1f}%  median cams {np.median(v):.0f}')

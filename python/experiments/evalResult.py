import sys, numpy as np
b=np.load('lan/result_v2.npz'); gt=b['gt'].astype(float); gt_ok=b['gt_ok']
gtv=gt[gt_ok]; zc=np.percentile(gtv[:,2],99.5)
diag=np.linalg.norm(gtv.max(0)-gtv.min(0))
print(f'{"result":26s}{"n":>8}{"on-plane":>10}{"err vs camamok":>16}{"z p99":>8}')
print(f'{"camamok GT":26s}{len(gtv):8d}{100*(gtv[:,2]>0.85*zc):>9.1f}%' if False else
      f'{"camamok GT":26s}{len(gtv):8d}{100*(gtv[:,2]>0.85*zc).mean():9.1f}%{"-":>16}{np.percentile(gtv[:,2],99):8.3f}')
for fn in sys.argv[1:]:
    try:
        z=np.load(fn)
    except Exception as e:
        print(f'{fn:26s}  MISSING'); continue
    M=z['M']; s=np.isfinite(z['dense_err'])
    q=(M[:3,:3]@z['dense_xyz'][s].astype(float).T).T+M[:3,3]
    both=z['both'] if 'both' in z.files else None
    if both is not None and both.sum()>0:
        mm=(M[:3,:3]@z['dense_xyz'][both].astype(float).T).T+M[:3,3]
        e=np.linalg.norm(mm-gt[both],axis=1); err=f'{100*np.median(e)/diag:6.2f}% (n={both.sum()})'
    else: err='n/a'
    print(f'{fn.split("/")[-1]:26s}{s.sum():8d}{100*(q[:,2]>0.85*zc).mean():9.1f}%{err:>16}{np.percentile(q[:,2],99):8.3f}')

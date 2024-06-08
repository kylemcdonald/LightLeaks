import numpy as np
def to_single_rgb(img):
    img = np.asarray(img)
    if len(img.shape) == 4: # take first frame from animations
        return img[0,:,:,:]
    if len(img.shape) == 2: # convert gray to rgb
        img = img[:,:,np.newaxis]
        return np.repeat(img, 3, 2) # might np.tile(img, [1,1,3]) be faster?
    if img.shape[-1] == 4: # drop alpha
        return img[:,:,:3]
    else:
        return img
    
def to_single_gray(img):
    img = np.asarray(img)
    if len(img.shape) == 2:
        return img[:,:,np.newaxis]
    elif img.shape[2] == 3:
        return img.mean(axis=2)
    else:
        return img

def rb_swap(img):
    if len(img.shape) < 3:
        return img
    if img.shape[2] == 3:
        return img[...,(2,1,0)]
    if img.shape[2] == 4:
        return img[...,(2,1,0,3)]
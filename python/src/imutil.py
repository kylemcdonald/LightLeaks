import os
import cv2
import numpy as np
import PIL.Image
import shutil
from color_conversion import to_single_rgb, to_single_gray, rb_swap

try: # Python 2
    from cStringIO import StringIO as BytesIO
except: # Python 3
    from io import BytesIO
    
# should add code to automatically scale 0-1 to 0-255
def imshow(img, fmt='png', retina=False, zoom=None):
    import IPython.display
    if img is None:
        raise TypeError('input image not provided')
    
    if isinstance(img, str):
        if img.startswith('http:') or img.startswith('https:'):
            IPython.display.display(IPython.display.Image(url=img, retina=retina))
        else:
            IPython.display.display(IPython.display.Image(filename=img, retina=retina))
        return
    
    if len(img.shape) == 1:
        n = len(img)
        side = int(np.sqrt(n))
        if (side * side) == n:
            img = img.reshape(side, side)
        else:
            raise ValueError('input is one-dimensional', img.shape)
    if len(img.shape) == 3 and img.shape[-1] == 1:
        img = img.squeeze()
    img = np.uint8(np.clip(img, 0, 255))
    if fmt == 'jpg':
        fmt = 'jpeg'
    if fmt == 'jpeg':
        img = to_single_rgb(img)
    image_data = BytesIO()
    PIL.Image.fromarray(img).save(image_data, fmt)
    height, width = img.shape[:2]
    if zoom is not None:
        width *= zoom
        height *= zoom
    IPython.display.display(IPython.display.Image(data=image_data.getvalue(),
                                                  width=width,
                                                  height=height,
                                                  retina=retina))

# jpeg4py is 2x as fast as opencv for jpegs, but more unstable
def imread(filename, mode=None, ext=None):
    if ext is None:
        _, ext = os.path.splitext(filename)
    ext = ext.lower()
    img = cv2.imread(filename, cv2.IMREAD_UNCHANGED)
    if img is not None:
        if len(img.shape) > 2:
            img = rb_swap(img)
    if img is not None:
        if mode == 'rgb':
            img = to_single_rgb(img)
        elif mode == 'gray':
            img = to_single_gray(img)
    return img

def imwrite(filename, img):
    if img is None:
        return
    if len(img.shape) == 2:
        return cv2.imwrite(filename, img)
    if len(img.shape) == 3:
        if img.shape[-1] == 3:
            return cv2.imwrite(filename, img[...,::-1])
        if img.shape[-1] == 4:
            return cv2.imwrite(filename, img[...,(2,1,0,3)])
        else:
            raise Exception('Unsupported number of channels')
    else:
        raise Exception('Unsupported image shape')

def downsample(img, scale=None, output_wh=None, max_side=None, min_side=None, block_size=None, mode=None):
    if max_side is not None:
        cur_max_side = max(img.shape[:2])
        scale = max_side / cur_max_side
    if min_side is not None:
        cur_min_side = min(img.shape[:2])
        scale = min_side / cur_min_side
    if scale is not None:
        output_wh = (int(np.round(img.shape[1]*scale)),
                     int(np.round(img.shape[0]*scale)))
    if block_size is not None:
        output_wh = (img.shape[1]//block_size, img.shape[0]//block_size)
    else:
        block_size = img.shape[1]//output_wh[0]
    if block_size > 1:
        img = cv2.blur(img, (block_size, block_size))
    return cv2.resize(img, output_wh, interpolation=cv2.INTER_AREA if mode is None else mode)

def upsample(img, scale=None, output_wh=None, max_side=None, min_side=None, mode=None):
    if max_side is not None:
        cur_max_side = max(img.shape[:2])
        scale = max_side / cur_max_side
    if min_side is not None:
        cur_min_side = min(img.shape[:2])
        scale = min_side / cur_min_side
    if output_wh is None:
        output_wh = (int(np.round(img.shape[1]*scale)),
                     int(np.round(img.shape[0]*scale)))
    return cv2.resize(img, output_wh, interpolation=cv2.INTER_CUBIC if mode is None else mode)

# output_wh value None in one dimension means "scale proportionally to other dimension"
# output_wh value -1 in one dimension means "use the existing value"
def imresize(img, scale=None, output_wh=None, max_side=None, min_side=None, mode=None):
    big = True
    if max_side is not None:
        cur_max_side = max(img.shape[:2])
        big = max_side > cur_max_side
    elif min_side is not None:
        cur_min_side = min(img.shape[:2])
        big = min_side > cur_min_side
    elif output_wh is not None:
        if output_wh[0] is None:
            scale = output_wh[1] / img.shape[0]
            output_wh = None
        elif output_wh[1] is None:
            scale = output_wh[0] / img.shape[1]
            output_wh = None
        elif output_wh[0] == -1:
            output_wh[0] = img.shape[1]
        elif output_wh[1] == -1:
            output_wh[1] = img.shape[0]
        if output_wh is not None:
            big = output_wh[0] > img.shape[1]
    if scale is not None:
        big = scale > 1
    
    if big:
        return upsample(img, scale, output_wh, max_side, min_side, mode)
    else:
        return downsample(img, scale, output_wh, max_side, min_side, mode)

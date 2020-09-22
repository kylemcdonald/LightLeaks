
from utils.progress import *
from utils.imutil import *
import glob
from numba import jit,njit 
from multiprocessing import Pool, cpu_count
from scipy import ndimage
from itertools import product
from matplotlib import pyplot as plt
import cv2
import json
import click
import numpy as np
import os
from tqdm import tqdm
import re
from functools import partial
from colorama import Fore, Back, Style, init
init(autoreset=True)


def processScans(data_dir, prefix, dest_folder, force_reprocess, blur_distance):
    projector_size = get_projector_size(data_dir)
    click.echo("Projector resolution %i x %i (from settings.json)" %
               (projector_size[0], projector_size[1]))

    scan_folders = sorted(
        [f for f in os.listdir(data_dir) if re.match('^'+prefix, f)])

    if not force_reprocess:
        scan_folders = list(filter(lambda x: not os.path.isdir(
            os.path.join(data_dir, x, dest_folder)), scan_folders))

    if len(scan_folders) == 0:
        click.secho(
            f"No scans found in {data_dir} with prefix {prefix}", err=True, fg="red")
        return

    for scan in tqdm(scan_folders, desc='Processing scans'):
        # Load all the scan images into memory
        tqdm.write(scan+f": Loading scan using {cpu_count()} threads's")

        normal_h, inverse_h, normal_v, inverse_v, reference_image, cam_mask_image, min_image = load_scan(
            scan, data_dir, blur_distance)

        tqdm.write(scan+": Calculating diferences")
        diff_code_h, diff_code_v, confidence = calculate_diference(
            normal_h, inverse_h, normal_v, inverse_v, cam_mask_image)

        tqdm.write(scan+": Packing binary image")
        packed_h, packed_v = pack_image(diff_code_h, diff_code_v)

        tqdm.write(scan+": Building projector map")
        pro_map, pro_confidence = build_pro_map(
            packed_v, packed_h, confidence, projector_size[0], projector_size[1])

        tqdm.write(scan+": Storing scan results")
        store_results(os.path.join(data_dir, scan, dest_folder), pro_map, pro_confidence,
                      confidence, reference_image, min_image, np.dstack((packed_v, packed_h)))

    # print(scan_folders)

    # load_scan("scan-0028")


def get_projector_size(data_dir):
    with open(data_dir+'/settings.json') as json_file:
        data = json.load(json_file)
        proj_width = data['projectors'][0]['width']
        proj_height = data['projectors'][0]['height']
    return proj_width, proj_height


def gausian_blur(img, distance):
    return cv2.GaussianBlur(img, (distance, distance), 0)

def image_load_job(blur_distance, fn):
    # faster to do conversion to gray here (in parallel) rather than later
    data = imread(fn).mean(axis=2)

    lowpass = gausian_blur(data, blur_distance)
    gauss_highpass = data - lowpass
    gauss_highpass = gauss_highpass / 2
    gauss_highpass = gauss_highpass + 128
    gauss_highpass = np.clip(gauss_highpass, 0, 255)
    gauss_highpass = np.abs(gauss_highpass)
    return gauss_highpass.astype(np.uint8)


def load_scan(scan_name, data_dir, blur_distance):
    # global scan_name, cam_mask_image, normal_h, inverse_h, normal_v, inverse_v, reference_image, min_image
    count_vertical_files_normal = len(glob.glob1(os.path.join(
        data_dir, scan_name, 'cameraImages/vertical/normal'), "*.jpg"))
    count_vertical_files_inverse = len(glob.glob1(os.path.join(
        data_dir, scan_name, 'cameraImages/vertical/inverse'), "*.jpg"))
    count_horizontal_files_normal = len(glob.glob1(os.path.join(
        data_dir, scan_name, 'cameraImages/horizontal/normal'), "*.jpg"))
    count_horizontal_files_inverse = len(glob.glob1(os.path.join(
        data_dir, scan_name, 'cameraImages/horizontal/inverse'), "*.jpg"))
    assert count_vertical_files_normal > 0
    assert count_vertical_files_normal == count_vertical_files_inverse
    assert count_horizontal_files_normal == count_horizontal_files_inverse

    # print(count_horizontal_files_normal)
    # print(glob.glob1(os.path.join(
    # data_dir, scan_name, 'cameraImages/vertical/normal'), "*.jpg"))
    try:
        cam_mask_image = imread(os.path.join(data_dir, scan_name, "mask.png"))
        cam_mask_image = cv2.cvtColor(cam_mask_image, cv2.COLOR_BGR2GRAY)
    except:
        cam_mask_image = None
        tqdm.write(Fore.YELLOW + scan_name+": No mask loadeed")

    filenames = []
    descriptions = []
    for direction, n in (('vertical', count_vertical_files_normal), ('horizontal', count_horizontal_files_normal)):
        for parity in ('normal', 'inverse'):
            cur = [(direction, parity, i) for i in range(n)]
            descriptions.extend(cur)
            filenames.extend([os.path.join(
                data_dir, scan_name, 'cameraImages/{}/{}/{}.jpg'.format(*e)) for e in cur])

    with Pool(processes=cpu_count()) as pool:
        func = partial(image_load_job, blur_distance)
        images = list(
            tqdm(pool.imap(func, filenames), total=len(filenames), desc=f"Preprocess {scan_name}", leave=False))
        # images = pool.map(image_load_job, filenames)
    # Load images in right buckets

    normal_h = np.asarray([i for i, d in zip(
        images, descriptions) if d[0] == 'horizontal' and d[1] == 'normal'])
    inverse_h = np.asarray([i for i, d in zip(
        images, descriptions) if d[0] == 'horizontal' and d[1] == 'inverse'])
    normal_v = np.asarray([i for i, d in zip(
        images, descriptions) if d[0] == 'vertical' and d[1] == 'normal'])
    inverse_v = np.asarray([i for i, d in zip(
        images, descriptions) if d[0] == 'vertical' and d[1] == 'inverse'])

    images_raw = np.asarray([i for i in images])
    min_image = np.amin(images_raw, axis=0)
    reference_image = cv2.equalizeHist(min_image)

    tqdm.write(scan_name+": Horizontal images loaded: %d" % len(normal_h))
    tqdm.write(scan_name+": Vertical images loaded: %d" % len(normal_v))

    return normal_h, inverse_h, normal_v, inverse_v, reference_image, cam_mask_image, min_image


def calculate_diference(normal_h, inverse_h, normal_v, inverse_v, cam_mask_image):
    # Calculate differeence
    diff_code_horizontal = normal_h.astype(
        np.float32) - inverse_h.astype(np.float32)
    diff_code_vertical = normal_v.astype(
        np.float32) - inverse_v.astype(np.float32)

    confidence_horizontal = np.sum(
        np.abs(diff_code_horizontal), axis=0) // len(diff_code_vertical)
    confidence_vertical = np.sum(
        np.abs(diff_code_vertical), axis=0) // len(diff_code_horizontal)

    # Another approach: Minimal confidence score (except highest frquency)
    # confidence_horizontal = np.min(np.abs(diff_code_horizontal[5:6]), axis=0)
    # confidence_vertical = np.min(np.abs(diff_code_vertical[5:6]), axis=0)

    confidence_vertical /= 256.0
    confidence_horizontal /= 256.0

    confidence = np.mean((confidence_horizontal, confidence_vertical), axis=0)

    confidence[cam_mask_image == 0] = 0

    return diff_code_horizontal, diff_code_vertical, confidence


# Binary packing
def pack_raw(channels, dtype=np.uint16):
    packed = (channels > 0).astype(dtype)
    n = len(packed)
    for i in range(n):
        packed[i] <<= n - i - 1
    return packed.sum(axis=0).astype(dtype)


def gray_to_binary(packed, n, dtype=np.uint16):
    codes = 1 << n
    lut = np.zeros(codes)
    for binary in range(codes):
        gray = (binary >> 1) ^ binary
        lut[gray] = binary
    return lut[packed].astype(dtype)


def pack_binary(channels, dtype=np.uint16):
    return gray_to_binary(pack_raw(channels, dtype), len(channels), dtype)


def pack_image(diff_code_horizontal, diff_code_vertical):
    packed_horizontal = pack_binary(diff_code_horizontal)
    packed_vertical = pack_binary(diff_code_vertical)

    # packed_vertical[cam_mask_image == 0] = 0
    # packed_horizontal[cam_mask_image == 0] = 0

    # packed = np.dstack((packed_vertical, packed_horizontal, np.zeros_like(packed_horizontal)))
    # packed = np.dstack((packed_vertical, packed_horizontal))
    return packed_horizontal, packed_vertical


# Pro Map
def add_channel(x):
    return np.pad(x, pad_width=((0, 0), (0, 0), (0, 1)), mode='constant', constant_values=0)


@jit('uint16(uint16[:], uint16[:], uint16[:], int64, int64)')
def build_pro_map(packed_vertical, packed_horizontal, confidence, w, h):
    packed_vertical = np.minimum(packed_vertical, w-1)
    packed_horizontal = np.minimum(packed_horizontal, h-1)
    cam_map = np.dstack((packed_horizontal, packed_vertical))

    pro_map = np.zeros((h, w, 2), dtype=np.uint16)
    pro_confidence = np.zeros((h, w), dtype=np.float32)

    for i in range(cam_map.shape[0]):
        for j in range(cam_map.shape[1]):
            idx = cam_map[i, j]
            pconf = confidence[i, j]
            # (j, i) should probably be swapped, but
            # this is how it was done in c++
            if pconf > 0 and pconf > pro_confidence[idx[0], idx[1]]:
                pro_map[idx[0], idx[1]] = (j, i)
                pro_confidence[idx[0], idx[1]] = pconf
    return (add_channel(pro_map), pro_confidence)


def store_results(out_path, pro_map, pro_confidence, confidence, reference_image, min_image, packed):
    if not os.path.exists(out_path):
        os.makedirs(out_path)

    # print('storing in '+out_path)
    #   print(pro_confidence.dtype)
    # Store results
    imwrite(os.path.join(out_path,'proMap.png'), pro_map)
    imwrite(os.path.join(out_path, 'proConfidence.exr'), pro_confidence)
    imwrite(os.path.join(out_path, 'camConfidence.exr'), confidence)
    imwrite(os.path.join(out_path, 'referenceImage.jpg'), reference_image)

    imwrite(os.path.join(out_path, 'minImage.png'), min_image)

    #   im = np.dstack((packed) np.zeros_like(packed_horizontal)))
    #   imwrite('camBinary.exr') im)
    np.save(os.path.join(out_path, 'camBinary.npy'), packed)

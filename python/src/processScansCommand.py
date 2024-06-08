from imutil import *
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
            f"No scans found needing processing in {data_dir} with prefix {prefix}", err=True, fg="red")
        return

    for scan in tqdm(scan_folders, desc='Processing scans'):
        # Load all the scan images into memory
        tqdm.write(scan+f": Loading scan using {cpu_count()} threads")
        if cpu_count() == 1:
            tqdm.write(Fore.YELLOW + "Warning: Only using 1 cpu! This will take a long time. If using docker, make sure that all CPU's are enabled in settings")

        normal_h, inverse_h, normal_v, inverse_v, reference_image, cam_mask_image, min_image = load_scan(
            scan, data_dir, blur_distance)

        tqdm.write(scan+": Calculating diferences")
        diff_code_h, diff_code_v, confidence = calculate_diference(
            normal_h, inverse_h, normal_v, inverse_v, cam_mask_image)

        tqdm.write(scan+": Packing binary image")
        packed_h, packed_v = pack_image(diff_code_h, diff_code_v, data_dir)

        tqdm.write(scan+": Building projector map")
        pro_map, pro_confidence = build_pro_map(
            packed_v, packed_h, confidence, projector_size[0], projector_size[1])

        tqdm.write(scan+": Storing scan results")
        store_results(os.path.join(data_dir, scan, dest_folder), pro_map, pro_confidence,
                      confidence, reference_image, min_image, np.dstack((packed_v, packed_h)))


def get_settings(data_dir):
    with open(data_dir+'/settings.json') as json_file:
        return json.load(json_file)
    
def get_projector_size(data_dir):
    data = get_settings(data_dir)
    proj_width = data['projectors'][0]['width']
    proj_height = data['projectors'][0]['height']
    return proj_width, proj_height


def gaussian_blur(img, distance):
    # Gaussian blur is the slowest part of the app
    return cv2.GaussianBlur(img, (distance, distance), 0)

def image_load_job(blur_distance, cam_mask_image, fn):
    # faster to do conversion to gray here (in parallel) rather than later
    data = imread(fn).mean(axis=2)
    data[cam_mask_image == 0] = 0

    if blur_distance > 0:
        lowpass = gaussian_blur(data, blur_distance)
        gauss_highpass = data - lowpass
        gauss_highpass = gauss_highpass / 2
        gauss_highpass = gauss_highpass + 128
        gauss_highpass = np.clip(gauss_highpass, 0, 255)
        gauss_highpass = np.abs(gauss_highpass)
        return [gauss_highpass.astype(np.uint8), data.astype(np.uint8)]
    else:
        return [data.astype(np.uint8), data.astype(np.uint8)]


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
        cam_mask_image = imread(os.path.join(data_dir, scan_name, "camamok/mask.jpg"))
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
        func = partial(image_load_job, blur_distance, cam_mask_image)
        images = np.asarray(list(
            tqdm(pool.imap(func, filenames), total=len(filenames), desc=f"Preprocess {scan_name}", leave=False)))
    
    # Calculate the min image
    normal = np.asarray([i[1] for i, d in zip(
        images, descriptions) if d[1] == 'normal'])
    inverse = np.asarray([i[1] for i, d in zip(
        images, descriptions) if d[1] == 'inverse'])
    min_images = np.vstack((normal[np.newaxis],inverse[np.newaxis])).min(0)
    min_image = np.mean(min_images, axis=0)

    # Calculate the mean of the min
    # min_mean = np.mean(min_images)
    
    # Normalization factor to ensure 
    # mean_goal = 94.0 # Why this number? Just because...
    # gain_factor = mean_goal / min_mean
    # tqdm.write(f'{scan_name}: Mean value {min_mean} gain factor {gain_factor}')

    # # Gain the non-highpass filtered images (in index 1). 
    # # Doing this in loop to keep memory usage down
    # for i in range(len(images)):
    #     images[i,1] = (images[i,1] * gain_factor).astype(np.uint8)
    
    # Load images in right buckets
    normal_h = np.asarray([i for i, d in zip(
        images, descriptions) if d[0] == 'horizontal' and d[1] == 'normal'])
    inverse_h = np.asarray([i for i, d in zip(
        images, descriptions) if d[0] == 'horizontal' and d[1] == 'inverse'])
    normal_v = np.asarray([i for i, d in zip(
        images, descriptions) if d[0] == 'vertical' and d[1] == 'normal'])
    inverse_v = np.asarray([i for i, d in zip(
        images, descriptions) if d[0] == 'vertical' and d[1] == 'inverse'])

    reference_image = cv2.equalizeHist(min_image.astype(np.uint8)) 

    tqdm.write(scan_name+": Horizontal images loaded: %d" % len(normal_h))
    tqdm.write(scan_name+": Vertical images loaded: %d" % len(normal_v))

    return normal_h, inverse_h, normal_v, inverse_v, reference_image, cam_mask_image, min_image


def calculate_diference(normal_h, inverse_h, normal_v, inverse_v, cam_mask_image):
    # Calculate differeence
    diff_code_horizontal = normal_h[:,0].astype(
        np.float32) - inverse_h[:,0].astype(np.float32)
    diff_code_vertical = normal_v[:,0].astype(
        np.float32) - inverse_v[:,0].astype(np.float32)

    # Calculate difference on the non-highpass filtered images, used for confidence calculation
    # diff_code_horizontal_conf = normal_h[:,1].astype(
    #     np.float32) - inverse_h[:,1].astype(np.float32)
    # diff_code_vertical_conf = normal_v[:,1].astype(
    #     np.float32) - inverse_v[:,1].astype(np.float32)

    normal = np.vstack([normal_h[:,1], normal_v[:,1]])
    inverse = np.vstack([inverse_h[:,1], inverse_v[:,1]])

    # print("normal shape", normal.shape)

    bright = np.max([normal,inverse], axis=0)
    dark = np.min([normal,inverse], axis=0)

    std_bright = bright.std(axis=0)
    std_dark = dark.std(axis=0)

    diff = bright - dark
    confidence = (diff.mean(0) / (std_dark + std_bright)).astype(np.float32) / 5.0 # Why 5? Just because
    confidence[(std_dark + std_bright) == 0] = 0
    
    # print("confidence shape",confidence.shape)

    # # u =0
    # # out_path = '/SharedData/scan-0630T12-26-48/processedScan'
    # # for i in diff_code_horizontal_conf:
    # #     u += 1
    # #     imwrite(os.path.join(out_path, f'_test{u}.jpg'), np.abs(i))

    # # confidence_horizontal = np.min(
    # #     np.abs(diff_code_horizontal_conf), axis=0) 
    # # confidence_vertical = np.min(
    # #     np.abs(diff_code_vertical_conf), axis=0)
    # confidence_horizontal = np.sum(
    #     np.abs(diff_code_horizontal_conf[2:]), axis=0) // len(diff_code_horizontal_conf[2:])
    # confidence_vertical = np.sum(
    #     np.abs(diff_code_vertical_conf[2:]), axis=0) // len(diff_code_vertical_conf[2:])

    
    # # Another approach: Minimal confidence score (except highest frquency)
    # # confidence_horizontal = np.min(np.abs(diff_code_horizontal[5:6]), axis=0)
    # # confidence_vertical = np.min(np.abs(diff_code_vertical[5:6]), axis=0)

    # confidence_vertical /= 256.0
    # confidence_horizontal /= 256.0

    # # imwrite(os.path.join(out_path, f'_confidence_horizontal.exr'), confidence_horizontal)
    # # imwrite(os.path.join(out_path, f'_confidence_vertical.exr'), confidence_vertical )

    # confidence = np.mean((confidence_horizontal, confidence_vertical), axis=0)

    confidence[cam_mask_image == 0] = 0

    return diff_code_horizontal, diff_code_vertical, confidence


# Binary packing
def pack_raw(channels, dtype=np.uint16):
    packed = (channels > 0).astype(dtype)
    n = len(packed)
    for i in range(n):
        packed[i] <<= n - i - 1
    return packed.sum(axis=0).astype(dtype)


def pack_binary(channels, data_dir, dtype=np.uint16):
    lut = load_lut(data_dir, len(channels))
    packed = pack_raw(channels, dtype)
    return lut[packed].astype(dtype)


def pack_image(diff_code_horizontal, diff_code_vertical, data_dir):
    packed_horizontal = pack_binary(diff_code_horizontal, data_dir)
    packed_vertical = pack_binary(diff_code_vertical, data_dir)
    return packed_horizontal, packed_vertical


# Load the graycode lut image
def load_lut(data_dir, bits):
    settings = get_settings(data_dir)
    codeType = settings['projectors'][0]['codeType']
    lut_img = imread(os.path.join(data_dir,f"../codes/{codeType}/{bits}.png"))
    if lut_img is None:
        raise Exception(f"Graycode lut image {codeType}/{bits} not found")

    codes = pack_raw(lut_img[::-1] == 255)
    lut = np.zeros(len(codes), np.uint32)
    for i,code in enumerate(codes):
        lut[code] = i
    return lut


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

def imwrite(filename, img):
    if img is not None:
        if len(img.shape) > 2:
            img = img[...,::-1]
    return cv2.imwrite(filename, img)

def store_results(out_path, pro_map, pro_confidence, confidence, reference_image, min_image, packed):
    if not os.path.exists(out_path):
        os.makedirs(out_path)
    
    # Store results
    imwrite(os.path.join(out_path,'proMap.png'), pro_map)
    imwrite(os.path.join(out_path, 'proConfidence.exr'), pro_confidence)
    imwrite(os.path.join(out_path, 'camConfidence.exr'), confidence)
    # imwrite(os.path.join(out_path, 'referenceImage.jpg'), reference_image)
    
    cv2.imwrite(os.path.join(out_path, 'proConfidence.jpg'), pro_confidence * 255 / np.max(pro_confidence), [int(cv2.IMWRITE_JPEG_QUALITY), 80])
    cv2.imwrite(os.path.join(out_path, 'referenceImage.jpg'), reference_image, [int(cv2.IMWRITE_JPEG_QUALITY), 80])

    imwrite(os.path.join(out_path, 'minImage.png'), min_image)
    np.save(os.path.join(out_path, 'camBinary.npy'), packed)

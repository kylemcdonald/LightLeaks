import os
os.environ["OPENCV_IO_ENABLE_OPENEXR"]="1"

import click
import json
import re
from tqdm import tqdm
from imutil import *
import numpy as np
import math

PROCESSED_SCAN_FOLDER = 'processedScan'


def buildXyzMap(data_dir, prefix):
    projector_size = get_projector_size(data_dir)
    click.echo("Projector resolution %i x %i (from settings.json)" %
               (projector_size[0], projector_size[1]))

    if not os.path.exists(os.path.join(data_dir, 'mask-0.png')):
        click.secho(
            f'Error: Projector mask not found at path {os.path.join(data_dir, "mask-0.png")}', err=True, fg='red')
        return
    
    scan_folders = sorted(
        [f for f in os.listdir(data_dir) if re.match('^'+prefix, f)])
    scan_folders = list(filter(lambda x: os.path.isdir(os.path.join(data_dir, x)), scan_folders))

    if len(scan_folders) == 0:
        click.secho(
            f"No scans found {data_dir} with prefix {prefix}", err=True, fg="red")
        return

    deduped = None

    for i, folder in tqdm(enumerate(scan_folders), total=len(scan_folders)):
        tqdm.write(folder + f": Loading processed scan")

        processed_path = os.path.join(data_dir, folder, PROCESSED_SCAN_FOLDER)
        cam_confidence = imread(os.path.join(
            processed_path, 'camConfidence.exr'))
        cam_binary_map = np.load(os.path.join(processed_path, 'camBinary.npy'))

        cam_width = cam_confidence.shape[1]
        cam_height = cam_confidence.shape[0]
        # tqdm.write(f"{folder}: Camera size {cam_width}x{cam_height}")

        # Load binary file from camamok
        cam_xyz_map = np.fromfile(os.path.join(
            data_dir, folder, 'camamok', 'xyzMap.raw'), np.float32)

        # Determine scale factor of binary file (probably 4 if code hasnt changed in camamok)
        scale_factor = math.floor(
            1/math.sqrt((len(cam_xyz_map) / 4) / (cam_width * cam_height)))
        tqdm.write(folder + f": upscaling xyz map by {scale_factor}")

        # Reshape camamok xyz map
        cam_xyz_map = cam_xyz_map.reshape(
            int(cam_height / scale_factor), int(cam_width / scale_factor), 4)[:, :, 0:3]
        cam_xyz_map = upsample(cam_xyz_map, scale=scale_factor)
        
        tqdm.write(folder + f": xyz map size {cam_xyz_map.shape}")

        

        # tqdm.write(f'{folder}: cam xyz minimum: {np.min(cam_xyz_map)}, max: {np.max(cam_xyz_map)}')

        assert len(cam_confidence) > 0
        assert len(cam_binary_map) > 0
        assert len(cam_xyz_map) > 0

        tqdm.write(folder + f": Packing data")
        packed = pack_maps(cam_confidence, cam_binary_map,
                           cam_xyz_map, i, projector_size)

        tqdm.write(
            f'{folder}: Packed {packed.shape[0]:,} pixels. Removing duplicate pixels')
        if deduped is not None:
            #         print('deduped before:', deduped.shape)
            packed = np.vstack((packed, deduped))
    #         print('packed after:', packed.shape)
        deduped = dedupe(packed)

        tqdm.write(
            f'{folder}: {deduped.shape[0]:,} pixels in deduplicated stack')

    click.echo("Done processing scanes. Unpacking projector map")

    projector_xyz, projector_confidence, cam_index_map, cam_pixel_index = unpack_maps(
        deduped, projector_size)

    cam_index_map_colored = np.copy(cam_index_map)
    cam_index_map_colored[projector_confidence < 0.1] = -1
    cam_index_map_colored = cam_index_map_colored * \
        255 / (cam_index_map.max()+1)
    cam_index_map_colored = cv2.applyColorMap(
        cam_index_map_colored.astype(np.uint8), cv2.COLORMAP_JET)
    # imshow(cam_index_map_colored, fmt='jpg')

    # Store result
    debug_out_path = os.path.join(data_dir, 'BuildXYZ')
    if not os.path.exists(debug_out_path):
        os.makedirs(debug_out_path)

    projector_mask = imread(os.path.join(
        data_dir, 'mask-0.png')).mean(axis=2) / 255
    projector_confidence_masked = projector_confidence * \
        projector_mask[:, :, np.newaxis]
    imwrite(os.path.join(debug_out_path, 'confidenceMap-0.exr'),
            projector_confidence_masked.astype(np.float32))
    imwrite(os.path.join(debug_out_path, 'xyzMap-0.exr'),
            projector_xyz.astype(np.float32))
    imwrite(os.path.join(debug_out_path, 'camIndexMap.png'), cam_index_map)
    imwrite(os.path.join(debug_out_path, 'camIndexMapColored.png'),
            cam_index_map_colored)

    with open(os.path.join(debug_out_path, "BuildXYZOutput.txt"), "w") as text_file:
        def t(text):
            text_file.write("%s\n" % text)
            click.echo(text)

        t("Scans used:")
        for s in scan_folders:
            t("\t%s" % s)
        t("Resolution: %ix%i" % (projector_size[0], projector_size[1]))

        threshold = 0.05
        t("\nCoverage (threshold %.2f):" % threshold)
        masked_camIndexMap = np.copy(cam_index_map)
        masked_camIndexMap[projector_confidence < threshold] = -1
        u, c = np.unique(masked_camIndexMap, return_counts=True)
        for _u, _c in zip(u, c):
            if _u != -1:
                t("\tScan %i (%s): %.2f%% (%i)" %
                  (_u, scan_folders[int(_u)], 100*_c / sum(c), _c))
            else:
                t("\tNo scan: %.2f%% (%i)" % (100*_c / sum(c), _c))


def get_projector_size(data_dir):
    with open(os.path.join(data_dir, 'settings.json')) as json_file:
        data = json.load(json_file)
        proj_width = data['projectors'][0]['width']
        proj_height = data['projectors'][0]['height']
    return proj_width, proj_height


def overflow_fix(cam_binary_map, proj_size):
    cam_binary_map[(cam_binary_map[:, 0] >= proj_size[0]) | (
        cam_binary_map[:, 1] >= proj_size[1])] = [0, 0]

# rows, cols = camHeight, camWidth
# confidence.shape: rows, cols (float)
# cam_binary_map.shape: rows, cols, 2 (int)
# cam_xyz_map.shape: rows, cols, 3 (float)
# cam_index: int


def pack_maps(confidence, cam_binary_map, cam_xyz_map, cam_index, proj_size):
    """ Pack camera confidence, cam binary projector map and camera xyz map """
    # prepare confidence_flat
    confidence_flat = confidence.reshape(-1, 1)

    # prepare cam_binary_mapFlat
    cam_binary_map_flat = cam_binary_map.reshape((-1, 2))
    overflow_fix(cam_binary_map_flat, proj_size)
    cam_binary_map_flat = np.ravel_multi_index(cam_binary_map_flat.transpose()[
                                               ::-1], (proj_size[1], proj_size[0])).reshape(-1, 1)

    # prepare cam_xyz_map_flat
    # scale = len(cam_binary_map) / len(cam_xyz_map)
    cam_xyz_map_flat = cam_xyz_map.reshape(-1, 3)

    # DEBUG STUFF
    # Pack camera index into array
    cam_index_flat = np.full((cam_xyz_map_flat.shape[0], 1), cam_index)

    # Cam Pixel Index
    cam_pixel_index = np.arange(cam_xyz_map_flat.shape[0])[:, np.newaxis]

    # stack and return everything in shape: (rows x cols), 7
    return np.hstack((confidence_flat, cam_binary_map_flat, cam_xyz_map_flat, cam_index_flat, cam_pixel_index))


def dedupe(packed):
    # get indices sorted by confidence, use ::-1 to put max confidence first
    packedSortedIndices = packed[:, 0].argsort()[::-1]
    packedSorted = packed[packedSortedIndices]

    # get unique packedSorted indices
    _, indices = np.unique(packedSorted.transpose()[1], return_index=True)
    return packedSorted[indices]


def unpack_maps(packed, proj_size):
    """ Unpack projector xyz map and projector confidence """
    proj_width = proj_size[0]
    proj_height = proj_size[1]
    projector_xyz = np.zeros((proj_height, proj_width, 3))
    projector_confidence = np.zeros((proj_height, proj_width, 1))
    cam_index = np.full((proj_height, proj_width, 1), -1)
    cam_pixel_index = np.zeros((proj_height, proj_width, 1))

    # assign xyzMap values use proMapFlat indices
    # packed[:,0] contains confidence value
    # packed[:,1] contains binary code (projector pixel coordinate)
    # packed[:,2:5] contains xyz coordinate
    # packed[:,5] contains camera index (debug)
    # packed[:,6] contains camera pixel index (debug)
    proMapFlat = packed[:, 1].astype(np.int32)
    projector_confidence.reshape(-1)[proMapFlat] = packed[:, 0]
    projector_xyz.reshape(-1, 3)[proMapFlat] = packed[:, 2:5]

    # DEBUG STUFF
    cam_index.reshape(-1)[proMapFlat] = packed[:, 5]
    cam_pixel_index.reshape(-1)[proMapFlat] = packed[:, 6].astype(np.uint64)

    return projector_xyz, projector_confidence, cam_index, cam_pixel_index

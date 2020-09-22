import glob
from numba import jit
from multiprocessing import Pool, cpu_count
from scipy import ndimage
from itertools import product
from matplotlib import pyplot as plt
import cv2
import json
import click

import processScansCommand
# Utils is added at this path in docker image:
# from utils.imutil import *
# from utils.progress import *

blur_distance = 301
processing_name = 'promap-'+str(blur_distance)
data_dir = '/home/jovyan/SharedData'


@click.group()
# @click.option('--debug/--no-debug', default=False)
def cli():
    pass
    # click.echo('Debug mode is %s' % ('on' if debug else 'off'))


@cli.command()
@click.option('--force', expose_value=True, is_flag=True, help="Force reprocess scans, even if they have already been processed")
@click.option('--datadir', default='/home/jovyan/SharedData', help="Path to SharedData containing scan folders")
@click.option('--prefix', default='scan', help="Prefix for scan folder names to process. Default is 'scan'")
@click.option('--destination', default='processedScan', help="Name of folder to store results within the scan folder", show_default=True)
@click.option('--blur', default=301, help="Set the gausian blur radius", show_default=True)
def processScans(datadir, prefix, destination, force, blur):
    click.echo(f"Preprocessing images with a blur radius of {blur}")
    processScansCommand.processScans(data_dir=datadir, prefix=prefix, dest_folder=destination, force_reprocess=force, blur_distance=blur)


if __name__ == '__main__':
    cli()

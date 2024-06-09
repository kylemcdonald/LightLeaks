import click
import buildXyzMapCommand

@click.group()
def cli():
    pass

@cli.command()
@click.option('--force', expose_value=True, is_flag=True, help="Force reprocess scans, even if they have already been processed")
@click.option('--datadir', default='../../SharedData', help="Path to SharedData containing scan folders")
@click.option('--prefix', default='scan', help="Prefix for scan folder names to process. Default is 'scan'")
@click.option('--destination', default='processedScan', help="Name of folder to store results within the scan folder", show_default=True)
@click.option('--blur', default=301, help="Set the gausian blur radius", show_default=True)
def processScans(datadir, prefix, destination, force, blur):
    # Importing processScansCommand here to not trigger jit compilation unless needed
    import processScansCommand
    click.echo(f"Preprocessing images with a blur radius of {blur}")
    processScansCommand.processScans(
        data_dir=datadir, prefix=prefix, dest_folder=destination, force_reprocess=force, blur_distance=blur)


@cli.command()
@click.option('--datadir', default='../../SharedData', help="Path to SharedData containing scan folders")
@click.option('--prefix', default='scan', help="Prefix for scan folder names to process. Default is 'scan'")
def buildXyzMap(datadir, prefix):
    click.echo("Building xyz map")
    buildXyzMapCommand.buildXyzMap(data_dir=datadir, prefix=prefix)


if __name__ == '__main__':
    cli()

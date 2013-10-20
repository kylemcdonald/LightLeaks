# Light Leaks

"Light Leaks" is an immersive installation built from a pile of mirror balls and a few projectors, created for CLICK Festival 2013 in Elsinore, Denmark.

* https://github.com/kylemcdonald/ofxCv (in addons)
* https://github.com/kylemcdonald/ofxControlPanel (in addons)
* https://github.com/YCAMInterlab/ProCamToolkit (in apps folder)

## Calibration Process

0. Capture multiple structured light calibration patterns using `ProCamSampleEdsdk` or `ProCamSampleEdsdkOsc` with `EdsdkOsc`. Make sure the projector size and OSC hosts match your configuration.
0. Place the resulting data in a folder called `scan/cameraImages/` in `SharedData/`. Make sure the values `int proWidth, proHeight;` match your projection configuration. Run `ProCamScan` and this will generate `proConfidence.exr` and `proMap.png`.
0. Place your `model.dae` and a `referenceImage.jpg` of the well lit space in `camamok/bin/data/`. Run `camamok` on your reference image. Hit the (back tick key) to generate the normals, then press the `saveXyzMap` button to save the normals.
0. Place the resulting `xyzMap.exr` and `normalMap.exr` inside `SharedData/scan/`.
0. Run `BuildXyzMap`. This will produce `SharedData/confidenceMap.exr`, `SharedData/xyzMap.exr` and `SharedData/normalMap.exr`
0. Copy the results of `BuildXyzMap` into `ProcessXyzMap/bin/data/` and run `ProcessXyzMap`.
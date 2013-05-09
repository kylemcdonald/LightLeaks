# Light Leaks

"Light Leaks" is an immersive installation built from a pile of mirror balls and a few projectors, created for CLICK Festival 2013 in Elsinore, Denmark.

* https://github.com/kylemcdonald/ofxCv (in addons)
* https://github.com/kylemcdonald/ofxControlPanel (in addons)
* https://github.com/YCAMInterlab/ProCamToolkit (in apps folder)

## Calibration Process

1. Capture multiple structured light calibration patterns using `ProCamSampleEdsdk` or `ProCamSampleEdsdkOsc` with `EdsdkOsc`. Make sure the projector size and OSC hosts match your configuration.
2. Place the resulting data in a folder called `scan` in `ProCamScan`. Make sure the values `int proWidth, proHeight;` match your projection configuration.
3. Place your 1/4 scale `referenceImage.jpg` of the well lit space in `camamok`. Run `camamok` on your reference image.
4. Place the resulting `xyzMap.exr` in a folder inside `BuildXyzMap` along with the `proConfidence.exr` and `proMap.png` from `ProCamScan`.
5. Run `BuildXyzMap`.
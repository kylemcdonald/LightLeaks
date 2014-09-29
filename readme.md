# Light Leaks

"Light Leaks" is an immersive installation built from a pile of mirror balls and a few projectors, created for CLICK Festival 2013 in Elsinore, Denmark.

* https://github.com/kylemcdonald/ofxCv (in addons)
* https://github.com/kylemcdonald/ofxControlPanel (in addons)
* https://github.com/YCAMInterlab/ProCamToolkit (in apps folder)

## Calibration Process

0. Capture multiple structured light calibration patterns using `ProCamSample` with `EdsdkOsc`. Make sure the projector size and OSC hosts match your configuration in `settings.xml`.
0. Place the resulting data in a folder called `scan/cameraImages/` in `SharedData/`. Run `ProCamScan` and this will generate `proConfidence.exr` and `proMap.png`.
0. Place your `model.dae` and a `referenceImage.jpg` of the well lit space in `camamok/bin/data/`. Run `camamok` on your reference image. Hit the (back tick key) to generate the normals, then press the `saveXyzMap` button to save the normals.
0. Place the resulting `xyzMap.exr` and `normalMap.exr` inside `SharedData/scan/`.
0. Run `BuildXyzMap`. This will produce `SharedData/confidenceMap.exr`, `SharedData/xyzMap.exr` and `SharedData/normalMap.exr`
0. Copy the results of `BuildXyzMap` into `ProcessXyzMap/bin/data/` and run `ProcessXyzMap`.

## Redesign

1. Run the CalibrationCapture app from a laptop that is on the same network as the computer that is connected to the projectors.
2. On the computer connected to the projector run the Calibration app.
3. Plug the camera into the laptop, and position it in a location where you can see at least N points. The app will tell you whether the image is over or underexposed.
4. *Transfer the images to the Calibration app*, it will decode all the images and let you know how accurately it could reconstruct the environment.
5. In the Calibration app, select control points until the model lines up.
6. Start the LightLeaks app.

Better interaction design:

* use remote trigger instead of second laptop?
* handle file copying for people
* automatically build reference images? remove all reflections then equalize histogram?
* UI for creating masks

# Install Notes

## Click Festival (2012)

* Mac Mini
* 1024x768 (native resolution) with Triple Head to Go

## La Gaîté Lyrique (2014)

* iMac
* 1280x1024 with Triple Head to Go on projectiondesign F32 sx+ (native 1400x1050) inset on the sensor
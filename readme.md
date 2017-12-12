# Light Leaks

"Light Leaks" is an immersive installation built from a pile of mirror balls and a few projectors, created for CLICK Festival 2013 in Elsinore, Denmark.

The repo is meant to be used with openFrameworks 0.10.0 (a0bd41a75).

* https://github.com/kylemcdonald/ofxCv @ c171afa
* https://github.com/kylemcdonald/ofxControlPanel @ c45e93856ba9bab2a70b7e1f6e44255399b91637
* https://github.com/kylemcdonald/ofxEdsdk @ a40f6e4d85b11585edb98ccfc0d743436980a1f2
* https://github.com/dzlonline/ofxBiquadFilter @ 87dbafce8036c09304ff574401026725c16013d1

## Calibration Process

Before doing any calibration, it's essential to measure the room and produce a `model.dae` file that includes all the geometry you want to project on. We usually build this file in SketchUp with a laser rangefinder for measurements, then save with "export two-sided faces" enabled, and finally load the model into MeshLab and save it again. MeshLab changes the order of the axes, and saves the geometry in a way that makes it easier to load into OpenFrameworks. (Note: at BLACK we ignored the "export two-sided faces" step and the MeshLab step, and camamok was modified slightly to work for this situation.)

0. Capture multiple structured light calibration patterns using `ProCamSample` with `EdsdkOsc`. Make sure the projector size and OSC hosts match your configuration in `settings.xml`. If the camera has image stabilization, make sure to turn it off.
0. Place the resulting data in a folder called `scan/cameraImages/` in `SharedData/`. Run `ProCamScan` and this will generate `proConfidence.exr` and `proMap.png`.
0. Place your `model.dae` and a `referenceImage.jpg` of the well lit space in `camamok/bin/data/`. Run `camamok` on your reference image. Hit the 'o' key to generate the normals, then press the `saveXyzMap` button to save the normals.
0. Place the resulting `xyzMap.exr` and `normalMap.exr` inside `SharedData/scan/`.
0. Run `BuildXyzMap` and drag `SharedData/scan` into the app. This will produce `SharedData/scan/camConfidence.exr` and `SharedData/scan/xyzMap.exr`. Repeat this step for multiple scans, then hit "s" to save the output. This will produce `SharedData/confidenceMap.exr` and `SharedData/xyzMap.exr`.
0. Run `LightLeaks`.

# Install Notes

* Each projector should be focused on the mirror balls, outputting native pixels (no scaling or keystoning) and framing the entire collection of mirror balls.

## Click Festival (2012)

* Mac Mini
* 1024x768 (native resolution) with TripleHead2Go

## La Gaîté Lyrique (2014)

* iMac
* 1280x1024 with TH2G on projectiondesign F32 sx+ (native 1400x1050) inset on the sensor
* When calibrating, create a network from the calibration computer that shares the ethernet and therefore provides DHCP.
* BlackMagic grabber and SDI camera for interaction

## Scopitone Festival (2015)

* Mac Pro (the bin)
* 3x [Mitsubishi UD8350U](http://www.mitsubishielectric.com/bu/projectors/products/data/high_resolution/ud8350u_lu_features.html) 6,500 Lumens, 1920x1200
* 2 projectors running through TripleHead2Go, the last directly from the Mac Pro
* One monitor hidden in the back closet

## BLACK Festival (2017)

20x15x4.4m room with 42 mirror balls on floor and 4 projectors at 3.25m high, 7.2m away from center of mirror balls.

Temporarily used a laptop and Mac Pro together for calibration, leading to the "Primary" and "Secondary" ProCamSample instances. This solution doesn't really work because lost OSC messages to the Secondary machine cause irrecoverable problems.

* Mac Pro with 4x [Apple Mini DisplayPort to Dual-Link Display Adapter, with USB Extension](https://www.apple.com/shop/product/MB571LL/A/mini-displayport-to-dual-link-dvi-adapter)
* 4x [Christie D12HD-H 1DLP projectors](https://www.christiedigital.com/en-us/business/products/projectors/1-chip-dlp/h-series/Christie-D12HD-H) at 10,500 ANSI Lumens, all running at 1920x1080.
* Additional monitor for debugging. Display arrangement was set so the monitor was the last screen to the right.
* [Scarlett Focusrite 18i8](https://us.focusrite.com/usb-audio-interfaces/scarlett-18i8) audio interface.

# Future ideas

## Redesign

1. Run the CalibrationCapture app from a laptop that is on the same network as the computer that is connected to the projectors.
2. On the computer connected to the projector run the Calibration app.
3. Plug the camera into the laptop, and position it in a location where you can see at least N points. The app will tell you whether the image is over or underexposed.
4. *Transfer the images to the Calibration app*, it will decode all the images and let you know how accurately it could reconstruct the environment.
5. In the Calibration app, select control points until the model lines up.
6. Start the LightLeaks app.

## Other ideas

* Auto calibrate from video taken with iPhone. Using BMC to encode the gray code signal. 
* Auto create point mesh from images so 3d model is not needed, and camamok is not required. 
* Change ProCamScan into a CLI that automatically is triggered by ProCamSample for live feedback on progress (highlight/hide points that have good confidence)

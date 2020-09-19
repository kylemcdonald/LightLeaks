<script lang="ts">
  import { Color, Matrix3, Matrix4, Vector2, Vector3 } from "three";
  import { Model } from "./model";
  import * as cv from "./cv";
  import ModelView from "./ModelView.svelte";
  import ImageView from "./ImageView.svelte";
  import CalibrationSettings from "../CalibrationSettings.svelte";
  import PointList from "./PointList.svelte";
  import { onMount } from "svelte";
  import ScanList from "./ScanList.svelte";
  import {renderSceneToArray} from "./exportRenderer";

  const modelViewModel = new Model(
    "/SharedData/model.dae",
    0.2,
		new Color("white"),
		"xray"
	);
	
  const imageViewModel = new Model(
    "/SharedData/model.dae",
    0.3,
    new Color("orange")
  );
  const exportModel = new Model(
    "/SharedData/model.dae",
    0.3,
		new Color(),
		"xyzMap"
  );

  let calibrationFlags = {
    CV_CALIB_FIX_PRINCIPAL_POINT: true,
    CV_CALIB_FIX_ASPECT_RATIO: true,
    CV_CALIB_FIX_K1: true,
    CV_CALIB_FIX_K2: true,
    CV_CALIB_FIX_K3: true,
    CV_CALIB_ZERO_TANGENT_DIST: true,
  };
  let calibrationErrorValue = -1;
  let calibrationValues = {
    fovx: 0,
    fovy: 0,
    aspectRatio: 0,
    focalLength: 0,
    principalPoint: new Vector2(0),
    fx: 0,
    fy: 0,
  };

  let objectPoints = [];
  let imagePoints = [];
  $: imagePoints && objectPoints && calibrationFlags && runCalibration();

  let highlightedIndex = -1;

  let calibratedModelViewMatrix: Matrix4 = new Matrix4();
  let cameraMatrix: Matrix3 = new Matrix3();
  let calibratedCamera;

  let selectedObjectPoint: Vector3 = undefined;

  let imageWidth = 0;
  let imageHeight = 0;

  // Is the app currently in a state of placing new image point?
  let placingImagePoint: boolean = false;

  let scans: string[];
  let loadedScan: string;
  async function loadScan(name: string) {
    loadedScan = name;
    await loadCalibration(name);
  }

  async function saveCalibration() {		
		const btn = document.getElementById('savebutton');
		btn.innerText = 'Saving	...';

    const data = {
			objectPoints,
      imagePoints,
			calibrationFlags,
			calibrationErrorValue		
    };


    await fetch("/saveCalibration", {
      method: "POST",
      cache: "no-cache",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({
        data,
        scan: loadedScan,
      }),
		});

		renderSceneToArray(imageWidth, imageHeight, exportModel, calibratedModelViewMatrix, cameraMatrix);
		
		setTimeout(()=>{
			btn.innerText = "Save"
		}, 1000)
	}
	
	function reset(){
		objectPoints = [];
		imagePoints = [];
		calibrationErrorValue = -1;
	}

  async function loadCalibration(name: string) {
    calibrationErrorValue = -1;

    if (!name) return;
    try {
      const data = await fetch(`/SharedData/${name}/camamok.json`).then((res) =>
        res.json()
      );
      objectPoints = data.objectPoints.map((p) => new Vector3(p.x, p.y, p.z));
      imagePoints = data.imagePoints.map((p) => new Vector2(p.x, p.y));
      calibrationFlags = data.calibrationFlags;
    } catch (e) {
      objectPoints = [];
      imagePoints = [];
    }
  }

  function addCalibrationPoint(objectPoint: Vector3, imagePoint: Vector2) {
    objectPoints = [...objectPoints, objectPoint];
    imagePoints = [...imagePoints, imagePoint];
  }

  async function runCalibration() {
    if (imageWidth == 0 || imagePoints.length < 3) return;

    calibrationErrorValue = -1;

		await cv.waitForLoad();
		
    const ret = cv.calibrateCamera(
      objectPoints,
      imagePoints,
      new Vector2(imageWidth, imageHeight),
      calibrationFlags
    );
    cameraMatrix = ret.cameraMatrix;
    calibratedModelViewMatrix = ret.matrix;

    calibrationValues = cv.calibrationMatrixValues(
      cameraMatrix,
      new Vector2(imageWidth, imageHeight)
    );

    calibrationErrorValue = ret.error;

    // updateCalibrateCameraResult(matrix, cameraMatrix);

    // imageView.setCalibratedMatrix(matrix, cameraMatrix);
  }

  onMount(async () => {
    document.addEventListener("keydown", (ev) => {
      if (ev.key == "Backspace" || ev.key == "Delete") {
        if (highlightedIndex != -1) {
          objectPoints.splice(highlightedIndex, 1);
          imagePoints.splice(highlightedIndex, 1);
          objectPoints = [...objectPoints];
          imagePoints = [...imagePoints];
          highlightedIndex = -1;
        }
      }

      if (
        (window.navigator.platform.match("Mac") ? ev.metaKey : ev.ctrlKey) &&
        ev.keyCode == 83
      ) {
        ev.preventDefault();
        saveCalibration();
      }
    });
    scans = await fetch("/scans").then((res) => res.json());
    loadScan(scans[0]);
  });
</script>

<style>
  main {
    display: flex;
    flex-direction: column;
    align-items: stretch;
    height: 100%;
    background-color: #202020;
    color: white;
  }
  .panel-row {
    display: flex;
    /* width: 100%; */
    /* height: 100%; */
    /* position: absolute; */
    align-items: stretch;
    flex-direction: row;
    overflow: hidden;
  }

  .panel {
    /* flex: 1; */
    position: relative;
  }

  #topbar {
    width: 100%;
    border-bottom: 1px solid gray;
    height: 50px;
		box-sizing: content-box;
		display: flex;
		justify-content: space-between;
	}
	
	#savebutton {
		margin-right: 20px;
		margin-top: 9px;
	}

  h1 {
    font-size: 20px;
    font-weight: 100;
    margin-left: 10px;
  }
</style>

<main>
  <div id="topbar">
		<h1>Light Leaks | Camamok</h1>
		<div class="rightbuttons">
			<button id="resetbutton" on:click={()=> reset() }>Reset</button>
			<button id="savebutton" on:click={()=> saveCalibration() }>Save</button>
		</div>
  </div>
  <div class="panel-row" style="flex:1; ">
    <div class="panel" style="    overflow: scroll;">
      <ScanList
        {scans}
        {loadedScan}
        on:loadscan={(ev) => loadScan(ev.detail)} />
      <CalibrationSettings
        bind:calibrationFlags
        errorValue={calibrationErrorValue}
        imageSize={new Vector2(imageWidth, imageHeight)}
        focalLength={calibrationValues.focalLength}
        fov={new Vector2(calibrationValues.fovx, calibrationValues.fovy)}
        principalPoint={calibrationValues.principalPoint}
        aspectRatio={calibrationValues.aspectRatio} />
			<PointList bind:objectPoints bind:imagePoints bind:highlightedIndex />

    </div>

    <div class="panel" style="flex:1">
      <ModelView
        model={modelViewModel}
        {objectPoints}
        bind:selectedPoint={selectedObjectPoint}
        bind:highlightedIndex
				{calibratedCamera}
				showCamera={calibrationErrorValue != -1}
        on:selectpoint={(ev) => (placingImagePoint = true)}
        on:deselectpoint={() => (placingImagePoint = false)} />
    </div>
    <div class="panel" style="flex:1;">
      <ImageView
        model={imageViewModel}
        imageUrls={loadedScan ? [
					`/SharedData/${loadedScan}/referenceImage.jpg`,
					`/SharedData/${loadedScan}/cameraImages/vertical/inverse/6.jpg`,
					] : undefined}
        {imagePoints}
        {calibratedModelViewMatrix}
        {cameraMatrix}
        bind:calibratedCamera
        bind:highlightedIndex
        placeNewMarker={placingImagePoint}
        showModel={calibrationErrorValue != -1}
        on:imageloaded={(ev) => {
          imageWidth = ev.detail.width;
          imageHeight = ev.detail.height;
          runCalibration();
        }}
        on:imageclick={(ev) => {
          if (placingImagePoint) {
            addCalibrationPoint(selectedObjectPoint, ev.detail);
            placingImagePoint = false;
            selectedObjectPoint = undefined;
          }
        }}
        on:imagepointmove={(ev) => {
          imagePoints[ev.detail.index] = ev.detail.point;
        }} />
      <div />
    </div>
  </div>
  <div class="panel-row">
    <div class="panel" />
  </div>
</main>

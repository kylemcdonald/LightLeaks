<script lang="ts">
  import { Matrix3, Matrix4, Vector2, Vector3 } from "three";
  import { Model } from "./model";
  import * as cv from "./cv";
  import ModelView from "./ModelView.svelte";
  import ImageView from "./ImageView.svelte";

  const modelViewModel = new Model("/model.dae");
  const imageViewModel = new Model("/model.dae");

  let objectPoints = [];
  let imagePoints = [];

  const js3 = [
    {
      imagePoint: { x: 3442.7510330578507, y: 1450.60847107438 },
      modelPoint: {
        x: 0.15000000114440917,
        y: -3.330669099286458e-17,
        z: 0.15000000114440917,
      },
    },
    {
      imagePoint: { x: 1856.5289256198348, y: 1613.752066115703 },
      modelPoint: {
        x: 8.549999822998046,
        y: 0.0600000016689303,
        z: -1.100000008392334,
      },
    },
    {
      imagePoint: { x: 1856.5289256198348, y: 2142.148760330579 },
      modelPoint: {
        x: 8.549999822998046,
        y: 2.510000086975098,
        z: -1.1000000083923334,
      },
    },
    {
      imagePoint: { x: 2393.3236650766407, y: 1776.5139887603552 },
      modelPoint: {
        x: 18.319999597167968,
        y: -1.9984015026011486e-15,
        z: 9.000000262451172,
      },
    },
    {
      imagePoint: { x: 414.1487603305786, y: 1028.2314049586778 },
      modelPoint: {
        x: -0.15000000114440917,
        y: 1.9984015026011486e-15,
        z: -9.000000262451172,
      },
    },
    {
      imagePoint: { x: 3940.744834710743, y: 958.6952479338842 },
      modelPoint: {
        x: -4.55000018005371,
        y: 1.0158540268744366e-15,
        z: -4.574999816894531,
      },
    },
    {
      imagePoint: { x: 64.2644628099174, y: 2177.851239669421 },
      modelPoint: {
        x: 8.875000140380859,
        y: 2.4500000671386735,
        z: -8.374999652099609,
      },
    },
    {
      imagePoint: { x: 1254.9979338842975, y: 171.6787190082648 },
      modelPoint: {
        x: -4.55000018005371,
        y: 2.0317080537488733e-15,
        z: -9.149999633789061,
      },
    },
  ];
  for (const row of js3) {
    objectPoints = [
      ...objectPoints,
      new Vector3(row.modelPoint.x, row.modelPoint.y, row.modelPoint.z),
    ];
    imagePoints = [
      ...imagePoints,
      new Vector2(row.imagePoint.x, row.imagePoint.y),
    ];
  }

  let calibratedModelViewMatrix: Matrix4 = new Matrix4();
  let cameraMatrix: Matrix3 = new Matrix3();
  let calibratedCamera;

  let selectedObjectPoint: Vector3 = undefined;

  let imageWidth = 0;
  let imageHeight = 0;

  // Is the app currently in a state of placing new image point?
  let placingImagePoint = false;

  function addCalibrationPoint(objectPoint: Vector3, imagePoint: Vector2) {
    objectPoints = [...objectPoints, objectPoint];
    imagePoints = [...imagePoints, imagePoint];

    runCalibration();
  }

  async function runCalibration() {
    if (imageWidth == 0 || imagePoints.length < 3) return;

    await cv.waitForLoad();

    const ret = cv.calibrateCamera(
      objectPoints,
      imagePoints,
      new Vector2(imageWidth, imageHeight)
    );
    cameraMatrix = ret.cameraMatrix;
    calibratedModelViewMatrix = ret.matrix;
    // updateCalibrateCameraResult(matrix, cameraMatrix);

    // imageView.setCalibratedMatrix(matrix, cameraMatrix);
  }
</script>

<style>
  .wrapper {
    display: flex;
    width: 100%;
    height: 100%;
    position: absolute;
    align-items: stretch;
    flex-direction: row;
  }

  .panel {
    flex: 1;
  }
</style>

<main>
  <div class="wrapper">
    <div class="panel">
      <ModelView
        model={modelViewModel}
        {objectPoints}
        bind:selectedPoint={selectedObjectPoint}
        {calibratedCamera}
        on:selectpoint={(ev) => (placingImagePoint = true)}
        on:deselectpoint={() => (placingImagePoint = false)} />
    </div>
    <div class="panel">
      <ImageView
        model={imageViewModel}
        src="/image.jpg"
        {imagePoints}
        {calibratedModelViewMatrix}
        {cameraMatrix}
        bind:calibratedCamera
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
          runCalibration();
          //console.log(ev.detail)
        }} />
      <div />
    </div>
  </div>
</main>

<script lang="ts">
  import {
    Color,
    PerspectiveCamera,
    Scene,
    Vector3,
    WebGLRenderer,
    Vector2,
    Raycaster,
    GridHelper,
    AxesHelper,
    Group,
    CameraHelper,
  } from "three";

  import { OrbitControls } from "three/examples/jsm/controls/OrbitControls";
  import { FlyControls } from "three/examples/jsm/controls/FlyControls";
  import type { Model } from "./model";
  import { createEventDispatcher, onMount } from "svelte";
  import { MarkerMesh } from "./markerMesh";

  export let model: Model;
  export let objectPoints: Vector3[] = [];
  export let selectedPoint: Vector3 | undefined = undefined;
  $: {
    if (selectedPoint) {
      selectedVertexMarker.position.copy(selectedPoint);
      selectedVertexMarker.visible = true;
    } else {
      selectedVertexMarker.visible = false;
    }
  }
  export let calibratedCamera: PerspectiveCamera;
  let cameraHelper = undefined;
  $: {
    if (calibratedCamera && !cameraHelper) {
      cameraHelper = new CameraHelper(calibratedCamera);
      scene.add(cameraHelper);
    }
  }

  let renderer: WebGLRenderer;
  let camera: PerspectiveCamera;
  let canvas: HTMLCanvasElement;
  let controls: OrbitControls;

  let mouse = new Vector2();
  let mouseDown = false;
  let mouseDownMovedDist = 0;

  const scene = new Scene();
  $: scene.add(model);

  const hoveredVertexMarker = new MarkerMesh(new Color("yellow"));
  const selectedVertexMarker = new MarkerMesh(new Color("green"));
  scene.add(hoveredVertexMarker);
  scene.add(selectedVertexMarker);

  const objectPointsGroup = new Group();
  scene.add(objectPointsGroup);
  $: {
    while (objectPointsGroup.children.length > objectPoints.length) {
      objectPointsGroup.remove(
        objectPointsGroup.children[objectPointsGroup.children.length - 1]
      );
    }

    while (objectPointsGroup.children.length < objectPoints.length) {
      const m = new MarkerMesh();
      m.color = new Color("red");
      objectPointsGroup.add(m);
    }

    for (let i = 0; i < objectPoints.length; i++) {
      objectPointsGroup.children[i].position.copy(objectPoints[i]);
    }
  }

  const dispatch = createEventDispatcher<{
    selectpoint: Vector3;
    deselectpoint: void;
  }>();

  onMount(() => {
    canvas = document.getElementById("model-canvas")! as HTMLCanvasElement;

    renderer = new WebGLRenderer({
      antialias: true,
      canvas,
    });
    renderer.setClearColor(new Color("rgb(50,50,50)"));

    camera = new PerspectiveCamera(
      60,
      canvas?.parentElement!.offsetWidth / canvas?.parentElement!.offsetHeight,
      0.1,
      10000
    );

    controls = new OrbitControls(camera, renderer.domElement);

    scene.add(new AxesHelper(30));

    camera.position.set(2, 2, 2);
    camera.lookAt(new Vector3(0, 0, 0));

    renderCanvas();

    canvas.addEventListener("pointermove", onMouseMove, false);
    canvas.addEventListener("pointerdown", onMouseDown);
    document.addEventListener("pointerup", onMouseUp);
  });

  function renderCanvas() {
    requestAnimationFrame(() => renderCanvas());
    renderer.setSize(
      canvas?.parentElement!.offsetWidth,
      canvas?.parentElement!.offsetHeight
    );

    renderer.render(scene, camera);

    camera.aspect =
      canvas?.offsetWidth / canvas?.offsetHeight;
    camera.updateProjectionMatrix();

    controls.update();
  }

  function onMouseMove(event: MouseEvent) {
    event.preventDefault();
    mouse.x = (event.offsetX / canvas.offsetWidth) * 2 - 1;
    mouse.y = -(event.offsetY / canvas.offsetHeight) * 2 + 1;

    if (mouseDown) {
      mouseDownMovedDist +=
        Math.abs(event.movementX) + Math.abs(event.movementY);
    }

    // Raycast vertices in the model
    // if (!selectedPoint) {
    const raycaster = new Raycaster();
    raycaster.setFromCamera(mouse, camera);

    const intersects = raycaster.intersectObjects(model.children, true);

    const threshold = 50;
    if (intersects.length > 0) {
      let distance;
      const intersect = intersects[0];
      const point = intersect.point;
      const object = intersect.object;
      let vertex = null;
      const test = object.worldToLocal(point);
      const points = model.verticesPositions;

      let curDist = -1;
      for (let i = 0; i < points.length; i++) {
        distance = points[i].distanceTo(test);
        if (distance > threshold) {
          continue;
        } else if (curDist === -1 || distance < curDist) {
          curDist = distance;
          vertex = object.localToWorld(points[i].clone());
        }
      }
      if (vertex) {
        hoveredVertexMarker.position.copy(vertex);
        hoveredVertexMarker.visible = true;
      } else {
        hoveredVertexMarker.visible = false;
      }
      // }
    }
  }

  function onMouseDown(event: MouseEvent) {
    mouseDown = true;
    mouseDownMovedDist = 0;
  }

  function onMouseUp(event: MouseEvent) {
    if (mouseDown) {
      mouseDown = false;

      if (mouseDownMovedDist < 5) {
        const hoveredPoint = hoveredVertexMarker.position.clone();
        if (
          selectedPoint?.distanceTo(hoveredPoint) < 0.01 ||
          !hoveredVertexMarker.visible
        ) {
          selectedPoint = undefined;
          dispatch("deselectpoint");
        } else {
          selectedPoint = hoveredPoint;
          dispatch("selectpoint", selectedPoint);
        }
      }
    }
  }
</script>

<style>
   #view {
    position: relative;
    height: 100%;
    width: 100%;
  }
  #infobox {
    position: absolute;
    bottom: 0;
    color: white;
    font-family: monospace;
    padding: 3px;
  }

  #toolbar {
    position: absolute;
    top: 0;
    width: 100%;
    /* background-color: rgba(0, 0, 0, 0); */
    /* border-bottom: 1px solid #00000080; */
    padding: 5px 5px 0px 5px;
    display: flex;
    gap: 10px;
  }
</style>

<div id="view">
  <div id="toolbar">
    <select bind:value={model.mode}>
      <option value="wireframe">Wireframe</option>
      <option value="shaded">Shaded</option>
      <option value="xyzMap">XYZ Map</option>
    </select>
  </div>
<canvas id="model-canvas" />

{#if selectedPoint}
  <div id="infobox">
    ({selectedPoint.x.toFixed(2)}, {selectedPoint.y.toFixed(2)}, {selectedPoint.z.toFixed(2)})
  </div>
{/if}

</div>
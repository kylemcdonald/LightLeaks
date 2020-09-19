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
  import Stats from "stats.js";
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
  let cameraHelper: CameraHelper = undefined;
  $: {
    if (calibratedCamera && !cameraHelper) {
      cameraHelper = new CameraHelper(calibratedCamera);
      scene.add(cameraHelper);
    }
  }
  export let showCamera: boolean = false;
  $: if (cameraHelper) cameraHelper.visible = showCamera;

  let renderer: WebGLRenderer;
  let camera: PerspectiveCamera;
  let canvas: HTMLCanvasElement;
  let controls: OrbitControls;

  let mouse = new Vector2();
  let mouseDown = false;
  let mouseDownMovedDist = 0;

  const scene = new Scene();
  $: scene.add(model);

  const hoveredVertexMarker = new MarkerMesh(new Color("red"), 0.5);
  const selectedVertexMarker = new MarkerMesh(new Color("green"));
  scene.add(hoveredVertexMarker);
  scene.add(selectedVertexMarker);

  export let highlightedIndex = -1;
  $: {
    for (let c of objectPointsGroup.children as MarkerMesh[]) {
      c.color = new Color("red");
    }
    if (highlightedIndex != -1) {
      (objectPointsGroup.children[
        highlightedIndex
      ] as MarkerMesh).color = new Color("rgb(0,200,60)");
    }
  }
  export let highlightedVertex: Vector3 | undefined;
  $: {
    if (highlightedVertex) {
      hoveredVertexMarker.position.copy(highlightedVertex);
      hoveredVertexMarker.visible = true;
    } else {
      hoveredVertexMarker.visible = false;
    }
  }

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

  let stats;

  onMount(() => {
    canvas = document.getElementById("model-canvas")! as HTMLCanvasElement;

    // stats = new Stats();
    // stats.showPanel( 1 ); // 0: fps, 1: ms, 2: mb, 3+: custom
    // canvas.parentElement.appendChild( stats.dom );
    // stats.dom.style.position = 'absolute'
    // stats.dom.style.bottom = '0'
    // stats.dom.style.right = '0'
    // stats.dom.style.top = 'initial'
    // stats.dom.style.left = 'initial'

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

    canvas.parentElement.addEventListener("pointermove", onMouseMove, false);
    canvas.parentElement.addEventListener("pointerdown", onMouseDown);
    document.addEventListener("pointerup", onMouseUp);
  });

  function renderCanvas() {
    requestAnimationFrame(() => renderCanvas());
    // stats.begin();
    renderer.setSize(
      canvas?.parentElement!.offsetWidth,
      canvas?.parentElement!.offsetHeight
    );

    renderer.render(scene, camera);

    camera.aspect = canvas?.offsetWidth / canvas?.offsetHeight;
    camera.updateProjectionMatrix();

    controls.update();
    // stats.end();
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
    const { vertex, index, dist } = model.findClosestVertex(mouse, camera);
    // model.highlightVertexIndex(index);

    highlightedVertex = undefined;
    if (vertex) {
      const epsilon = 0.1;
      highlightedIndex = -1;
      for (let i = 0; i < objectPoints.length; i++) {
        if (objectPoints[i].distanceTo(vertex) < epsilon) {
          highlightedIndex = i;
          break;
        }
      }

      if (highlightedIndex == -1 && dist < 0.1 && vertex) {
        highlightedVertex = vertex.clone();
        canvas.style.cursor = "pointer";
      } else {
        canvas.style.cursor = "initial";

      }
    }
  }

  function onMouseDown(event: MouseEvent) {
    if(event.button == 0){
      mouseDown = true;
      mouseDownMovedDist = 0;
    }
  }

  function onMouseUp(event: MouseEvent) {
    if (mouseDown) {
      mouseDown = false;

      if (mouseDownMovedDist < 5) {
        const hoveredPoint = highlightedVertex.clone();
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
      <option value="xray">XRay</option>
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

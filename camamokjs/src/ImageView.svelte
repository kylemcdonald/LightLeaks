<script lang="ts">
  import { createEventDispatcher, onMount } from "svelte";
  import Stats from "stats.js";

  import {
    PerspectiveCamera,
    WebGLRenderer,
    Vector2,
    OrthographicCamera,
    Color,
    Scene,
    MOUSE,
    ImageUtils,
    PlaneGeometry,
    Mesh,
    MeshBasicMaterial,
    TextureLoader,
    Vector3,
    Group,
    Matrix4,
    Matrix3,
    Material,
  } from "three";
  import { OrbitControls } from "three/examples/jsm/controls/OrbitControls";
  import { calibrateCamera, makeProjectionMatrix } from "./cv";
  import { MarkerMesh } from "./markerMesh";
  import type { Model } from "./model";

  export let model: Model;
  $: calibratedScene.add(model);

  export let imageUrls: string[];
  let selectedImageUrlIndex = 0;
  $: {
    if (imageUrls) loadImage(imageUrls[selectedImageUrlIndex]);
  }

  export let imagePoints: Vector2[];
  export let objectPoints: Vector3[];

  export let calibratedModelViewMatrix: Matrix4;
  export let cameraMatrix: Matrix3;
  $: {
    calibratedCamera.matrixAutoUpdate = false;
    calibratedCamera.matrix.getInverse(calibratedModelViewMatrix);
    calibratedCamera.updateMatrixWorld(true);
    const projMatrix = makeProjectionMatrix(
      cameraMatrix,
      new Vector2(imageWidth, imageHeight)
    );
    calibratedCamera.projectionMatrix.copy(projMatrix);
    calibratedCamera.projectionMatrixInverse.getInverse(
      calibratedCamera.projectionMatrix
    );
  }

  export let pictureOpacity = 1;
  $: if (imagePlane) (imagePlane.material as Material).opacity = pictureOpacity;
  export let showModel: boolean = true;
  $: if (model) model.visible = showModel;

  export let highlightedIndex = -1;
  $: {
    for (let c of imagePointsGroup.children as MarkerMesh[]) {
      c.color = new Color("red");
    }
    if (highlightedIndex != -1) {
      (imagePointsGroup.children[
        highlightedIndex
      ] as MarkerMesh).color = new Color("rgb(0,200,60)");
    }
  }
  export let highlightedVertex: undefined | Vector3;
  $: {
    if (highlightedVertex) {
      hoveredVertexMarker.position.copy(highlightedVertex);
      hoveredVertexMarker.visible = true;
    } else {
      hoveredVertexMarker.visible = false;
    }
  }

  export let placeNewMarker: boolean = false;

  const dispatch = createEventDispatcher<{
    imageloaded: Vector2;
    imageclick: Vector2;
    imagepointmove: { index: number; point: Vector2 };
  }>();

  let renderer: WebGLRenderer;
  let imageCamera = new OrthographicCamera(
    innerWidth / -2,
    innerWidth / 2,
    innerHeight / 2,
    innerHeight / -2,
    1,
    1000
  );
  imageCamera.position.set(0, 0, 10);
  imageCamera.lookAt(new Vector3(0, 0, 0));

  export let calibratedCamera = new PerspectiveCamera();

  let controls: OrbitControls;

  let canvas: HTMLCanvasElement;

  let scene = new Scene();
  let calibratedScene = new Scene();
  let imagePlane: Mesh | undefined;

  let imageWidth = 0;
  let imageHeight = 0;
  let imageAspect = 1;

  let mouse = new Vector2();
  let mouseDown = false;
  let mouseDownMovedDist = 0;

  let selectedMarkerIndex = -1;
  const cursorMarker = new MarkerMesh(new Color("red"), 0.5);
  cursorMarker.renderOrder = 3000;
  const hoveredVertexMarker = new MarkerMesh(new Color("red"), 0.5);
  hoveredVertexMarker.renderOrder = 3000;
  $: cursorMarker.visible = placeNewMarker;
  scene.add(cursorMarker);
  calibratedScene.add(hoveredVertexMarker);

  let stats;

  const imagePointsGroup = new Group();
  scene.add(imagePointsGroup);
  $: {
    while (imagePointsGroup.children.length > imagePoints.length) {
      imagePointsGroup.remove(
        imagePointsGroup.children[imagePointsGroup.children.length - 1]
      );
    }

    while (imagePointsGroup.children.length < imagePoints.length) {
      const m = new MarkerMesh();
      m.renderOrder = 2000;
      m.color = new Color("red");
      imagePointsGroup.add(m);
    }

    for (let i = 0; i < imagePoints.length; i++) {
      imagePointsGroup.children[i].position.x = imagePoints[i].x;
      imagePointsGroup.children[i].position.y = imagePoints[i].y;
    }
  }

  const objectPointsGroup = new Group();
  calibratedScene.add(objectPointsGroup);
  $: {
    while (objectPointsGroup.children.length > objectPoints.length) {
      objectPointsGroup.remove(
        objectPointsGroup.children[objectPointsGroup.children.length - 1]
      );
    }

    while (objectPointsGroup.children.length < objectPoints.length) {
      const m = new MarkerMesh();
      m.renderOrder = 1000;
      m.color = new Color("rgb(100,50,50)");
      objectPointsGroup.add(m);
    }

    for (let i = 0; i < objectPoints.length; i++) {
      objectPointsGroup.children[i].position.copy(objectPoints[i]);
    }
  }

  onMount(() => {
    canvas = document.getElementById("image-canvas")! as HTMLCanvasElement;

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
    renderer.setClearColor(new Color("rgb(10,10,10)"));

    controls = new OrbitControls(imageCamera, renderer.domElement);
    // Enable left mouse button to pan
    controls.mouseButtons.LEFT = MOUSE.RIGHT;
    controls.enableRotate = false;

    canvas.parentElement.addEventListener("pointermove", onMouseMove, false);
    canvas.parentElement.addEventListener("pointerdown", onMouseDown);
    document.addEventListener("pointerup", onMouseUp);

    renderCanvas();
  });

  function renderCanvas() {
    requestAnimationFrame(() => renderCanvas());

    // stats.begin();

    const w = canvas?.parentElement!.offsetWidth;
    const h = canvas?.parentElement!.offsetHeight;

    renderer.setSize(w, h);

    imageCamera.left = 0;
    imageCamera.right = imageWidth;
    imageCamera.top = (imageAspect * imageHeight) / (w / h);
    imageCamera.bottom = 0;
    imageCamera.updateProjectionMatrix();

    renderer.render(scene, imageCamera);
    renderer.autoClearColor = false;

    // Project the corners of the image (0,imageSize) to screen space (-1,1)
    const { viewportX, viewportY, viewportWidth, viewportHeight } = viewport();

    renderer.setViewport(viewportX, viewportY, viewportWidth, viewportHeight);
    renderer.render(calibratedScene, calibratedCamera);
    renderer.autoClearColor = true;

    controls.update();
    // stats.end();
  }

  function viewport() {
    const w = canvas?.parentElement!.offsetWidth;
    const h = canvas?.parentElement!.offsetHeight;

    const p1 = new Vector3(0, 0, 0);
    p1.project(imageCamera);
    const p2 = new Vector3(imageWidth, imageHeight, 0);
    p2.project(imageCamera);

    // Calculate the threejs viewport for the calibrated camera
    const viewportX = w / 2 + (p1.x * w) / 2;
    const viewportY = h / 2 + (p1.y * h) / 2;
    const viewportWidth = ((p2.x - p1.x) * w) / 2;
    const viewportHeight = ((p2.y - p1.y) * h) / 2;
    return {
      viewportX,
      viewportY,
      viewportWidth,
      viewportHeight,
    };
  }

  function loadImage(url) {
    if (imagePlane) {
      scene.remove(imagePlane);
    }

    const loader = new TextureLoader();
    loader.load(url, (tex) => {
      imageWidth = tex.image.width;
      imageHeight = tex.image.height;
      imageAspect = imageWidth / imageHeight;

      const material = new MeshBasicMaterial({
        map: tex,
        opacity: 1.0,
        transparent: true,
      });

      imagePlane = new Mesh(
        new PlaneGeometry(imageWidth, imageHeight),
        material
      );
      imagePlane.renderOrder = -10;
      imagePlane.position.setX(imageWidth / 2);
      imagePlane.position.setY(imageHeight / 2);
      imagePlane.name = "image";
      scene.add(imagePlane);

      dispatch("imageloaded", new Vector2(imageWidth, imageHeight));
    });
  }

  /* Calculate image coordinate from screen coordinate */
  function imageCoordinate(x: number, y: number): Vector2 {
    const p = new Vector3(x, y, 0);
    p.unproject(imageCamera);
    return new Vector2(p.x, p.y);
  }

  /* Find nearest marker. Return -1 if none is found */
  function findHoveredMarkerIndex(x: number, y: number): number {
    const coord = imageCoordinate(x, y);

    let bestIndex = -1;
    let bestDist = -1;
    for (let i = 0; i < imagePoints.length; i++) {
      const dist = imagePoints[i].distanceTo(coord);
      if (bestIndex == -1 || bestDist > dist) {
        bestDist = dist;
        bestIndex = i;
      }
    }

    const threshold = Math.abs(
      new Vector3(0, 0, 0).unproject(imageCamera).x -
        new Vector3(30 / canvas.width, 0, 0).unproject(imageCamera).x
    );
    if (bestDist < threshold) {
      return bestIndex;
    } else {
      return -1;
    }
  }

  function onMouseMove(event: MouseEvent) {
    mouse.x = (event.offsetX / canvas.offsetWidth) * 2 - 1;
    mouse.y = -(event.offsetY / canvas.offsetHeight) * 2 + 1;
    const imageCoord = imageCoordinate(mouse.x, mouse.y);
    cursorMarker.position.x = imageCoord.x;
    cursorMarker.position.y = imageCoord.y;

    if (mouseDown) {
      mouseDownMovedDist +=
        Math.abs(event.movementX) + Math.abs(event.movementY);

      if (selectedMarkerIndex != -1) {
        if (imageCoord) {
          dispatch("imagepointmove", {
            index: selectedMarkerIndex,
            point: imageCoord,
          });
        }
      }
    }

    const markerIndex = findHoveredMarkerIndex(mouse.x, mouse.y);
    highlightedIndex = markerIndex;
    highlightedVertex = undefined;
    if (markerIndex != -1) {
      canvas.style.cursor = "pointer";
    } else {
      if (showModel) {
        const {
          viewportX,
          viewportY,
          viewportWidth,
          viewportHeight,
        } = viewport();
        const mousePosInViewport = new Vector2(event.offsetX, event.offsetY);
        mousePosInViewport.y = canvas.offsetHeight - mousePosInViewport.y;

        mousePosInViewport.x -= viewportX;
        mousePosInViewport.y -= viewportY;

        mousePosInViewport.x /= viewportWidth;
        mousePosInViewport.y /= viewportHeight;

        mousePosInViewport.x = mousePosInViewport.x * 2 - 1;
        mousePosInViewport.y = mousePosInViewport.y * 2 - 1;

        if (
          mousePosInViewport.x > -1 &&
          mousePosInViewport.x < 1 &&
          mousePosInViewport.y > -1 &&
          mousePosInViewport.y < 1
        ) {
          const { dist, vertex } = model.findClosestVertex(
            mousePosInViewport,
            calibratedCamera
          );
          if (dist < 0.1) {
            highlightedVertex = vertex.clone();
            canvas.style.cursor = "pointer";
          } else {
            canvas.style.cursor = "initial";
          }
        } else {
          canvas.style.cursor = "initial";
        }
      } else {
        canvas.style.cursor = "initial";
      }
    }
  }

  function onMouseDown(event: MouseEvent) {
    if (event.button == 0) {
      mouseDown = true;
      mouseDownMovedDist = 0;

      selectedMarkerIndex = findHoveredMarkerIndex(mouse.x, mouse.y);
      if (selectedMarkerIndex != -1) {
        controls.enablePan = false;
      }
    }
  }

  function onMouseUp(event: MouseEvent) {
    if (mouseDown) {
      mouseDown = false;
      controls.enablePan = true;
      // controls.mouseButtons.LEFT = MOUSE.RIGHT;

      if (mouseDownMovedDist < 5) {
        const imageCoord = imageCoordinate(mouse.x, mouse.y);
        if (imageCoord || highlightedVertex) {
          dispatch("imageclick", imageCoord);
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
    <button
      on:click={() => {
        if (pictureOpacity == 1) {
          pictureOpacity = 0.5;
        } else if (pictureOpacity == 0) {
          pictureOpacity = 1;
        } else {
          pictureOpacity = 0;
        }
      }}
      data-toggled={pictureOpacity > 0}>
      <i class="material-icons">photo</i>
    </button>
    <button on:click={() => (showModel = !showModel)} data-toggled={showModel}>
      <i class="material-icons">layers</i>
    </button>

    <select bind:value={model.mode}>
      <option value="xray">XRay</option>
      <option value="wireframe">Wireframe</option>
      <option value="shaded">Shaded</option>
      <option value="xyzMap">XYZ Map</option>
    </select>

    <select bind:value={selectedImageUrlIndex}>
      {#if imageUrls}
        {#each imageUrls as url, i}
          <option value={i}>{url.split('/')[url.split('/').length - 1]}</option>
        {/each}
      {/if}
    </select>
  </div>
  <canvas id="image-canvas" />
</div>

<script lang="ts">
  import { createEventDispatcher, onMount } from "svelte";

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
  } from "three";
  import { OrbitControls } from "three/examples/jsm/controls/OrbitControls";
  import { makeProjectionMatrix } from "./cv";
  import { MarkerMesh } from "./markerMesh";
  import type { Model } from "./model";

  export let model: Model;
  $: calibratedScene.add(model);

  export let src: string;
  $: loadImage(src);

  export let imagePoints: Vector2[];

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

  export let showPicture = true;
  $: if(imagePlane) imagePlane.visible = showPicture;
  export let showModel = true;
  $: if(model) model.visible = showModel;

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
      m.color = new Color("red");
      imagePointsGroup.add(m);
    }

    for (let i = 0; i < imagePoints.length; i++) {
      imagePointsGroup.children[i].position.x = imagePoints[i].x;
      imagePointsGroup.children[i].position.y = imagePoints[i].y;
    }
  }

  onMount(() => {
    canvas = document.getElementById("image-canvas")! as HTMLCanvasElement;

    renderer = new WebGLRenderer({
      antialias: true,
      canvas,
    });
    renderer.setClearColor(new Color("rgb(10,10,10)"));

    controls = new OrbitControls(imageCamera, renderer.domElement);
    // Enable left mouse button to pan
    controls.mouseButtons.LEFT = MOUSE.RIGHT;
    controls.enableRotate = false;

    canvas.addEventListener("pointermove", onMouseMove, false);
    canvas.addEventListener("pointerdown", onMouseDown);
    document.addEventListener("pointerup", onMouseUp);

    renderCanvas();
  });

  function renderCanvas() {
    requestAnimationFrame(() => renderCanvas());
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
    const p1 = new Vector3(0, 0, 0);
    p1.project(imageCamera);
    const p2 = new Vector3(imageWidth, imageHeight, 0);
    p2.project(imageCamera);

    // Calculate the threejs viewport for the calibrated camera
    const viewportX = w / 2 + (p1.x * w) / 2;
    const viewporty = h / 2 + (p1.y * h) / 2;
    const viewportWidth = ((p2.x - p1.x) * w) / 2;
    const viewportHeight = ((p2.y - p1.y) * h) / 2;

    renderer.setViewport(viewportX, viewporty, viewportWidth, viewportHeight);
    renderer.render(calibratedScene, calibratedCamera);
    renderer.autoClearColor = true;

    controls.update();
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
      });

      imagePlane = new Mesh(
        new PlaneGeometry(imageWidth, imageHeight),
        material
      );
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

    if (mouseDown) {
      mouseDownMovedDist +=
        Math.abs(event.movementX) + Math.abs(event.movementY);

      if (selectedMarkerIndex != -1) {
        const imageCoord = imageCoordinate(mouse.x, mouse.y);
        if (imageCoord) {
          dispatch("imagepointmove", {
            index: selectedMarkerIndex,
            point: imageCoord,
          });
        }
      }
    }

    const markerIndex = findHoveredMarkerIndex(mouse.x, mouse.y);
    if (markerIndex != -1) {
      canvas.style.cursor = "pointer";
    } else {
      canvas.style.cursor = "initial";
    }
  }

  function onMouseDown(event: MouseEvent) {
    mouseDown = true;
    mouseDownMovedDist = 0;

    selectedMarkerIndex = findHoveredMarkerIndex(mouse.x, mouse.y);
    if (selectedMarkerIndex != -1) {
      controls.enablePan = false;
    }
  }

  function onMouseUp(event: MouseEvent) {
    if (mouseDown) {
      mouseDown = false;
      controls.enablePan = true;

      if (mouseDownMovedDist < 5) {
        const imageCoord = imageCoordinate(mouse.x, mouse.y);
        if (imageCoord) {
          dispatch("imageclick", imageCoord);
        }
      }
    }
  }
</script>

<style>
  
  #view {
    position: relative;
    height: 100%;;
    width: 100%;;
  }
  #toolbar {
    position: absolute;
    top:0;
    width:100%;
    background-color: rgba(0,0,0, 0.7);
    border-bottom: 1px solid #00000080;
    padding: 5px;
  }

</style>

<div id="view">
  <div id="toolbar">
  <button 
    on:click={()=>showPicture = !showPicture}
    data-toggled={showPicture}
    >
    <i class="material-icons">photo</i>
  </button>
  <button 
    on:click={()=>showModel = !showModel}
    data-toggled={showModel}
    >
    <i class="material-icons">layers</i>
  </button>
  </div>
  <canvas id="image-canvas" />
</div>
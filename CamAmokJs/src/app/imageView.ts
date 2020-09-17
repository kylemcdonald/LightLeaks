import {
  Color,
  PerspectiveCamera,
  Scene,
  Vector3,
  Vector2,
  Raycaster,
  Renderer,
  OrthographicCamera,
  Mesh,
  ImageUtils,
  MeshBasicMaterial,
  PlaneGeometry,
  Matrix4,
  WebGLRenderer,
  Camera,
  Matrix3,
  Object3D,
} from "three";

import { OrbitControls } from "three/examples/jsm/controls/OrbitControls";
import { Model } from "./model";
import { Selection } from "./selection";
import {makeProjectionMatrix} from './cv';

const canvas = document.getElementById("image-canvas")! as HTMLCanvasElement;

export class ImageView {
  onPlaceMarker: (coord: Vector2) => void = () => {};
  onMoveMarker: (marker: Selection) => void = () => {};

  public readonly renderer = new WebGLRenderer({
    antialias: true,
    canvas: canvas,
  });

  public scene = new Scene();
  public camera = new OrthographicCamera(
    innerWidth / -2,
    innerWidth / 2,
    innerHeight / 2,
    innerHeight / -2,
    1,
    1000
  );

  // public callibratedCamera =  new Camera(  );
  callibratedCamera = new PerspectiveCamera();
  calibratedScene = new Scene();

  seletedVertex: Vector3 | null = null;

  controls: OrbitControls;
  controls2: OrbitControls;

  private brick: Model;

  imageWidth: number = 5184;
  imageHeight: number = 3456;
  get imageAspect() {
    return this.imageWidth / this.imageHeight;
  }

  private mouse: Vector2 = new Vector2();
  private raycaster = new Raycaster();

  placeMarkerOnClick = false;

  private mouseDown = false;
  private mouseDownMoved = false;

  private selectedMarker: Selection | undefined;

  // private selectorMesh = new Selection();

  constructor() {
    this.controls2 = new OrbitControls(
      this.callibratedCamera,
      this.renderer.domElement
    );
    this.controls = new OrbitControls(this.camera, this.renderer.domElement);

    // this.callibratedCamera.position.set(1,1,1)
    this.controls.enableRotate = false;

    this.brick = new Model();

    ImageUtils.loadTexture(
      require("../../assets/image.jpg").default,
      undefined,
      (tex) => {
        this.imageWidth = tex.image.width;
        this.imageHeight = tex.image.height;

        const img = new MeshBasicMaterial({
          map: tex,
        });
        // img.map.needsUpdate = true;
        const plane = new Mesh(
          new PlaneGeometry(this.imageWidth, this.imageHeight),
          img
        );
        plane.position.setX(this.imageWidth / 2);
        plane.position.setY(this.imageHeight / 2);
        plane.name = "image";
        this.scene.add(plane);
      }
    );
    this.calibratedScene.add(this.brick);

    // this.scene.add(this.selectorMesh);

    this.camera.position.set(0, 0, 10);
    this.camera.lookAt(new Vector3(0, 0, 0));

    canvas.addEventListener("pointermove", this.onMouseMove.bind(this), false);

    canvas.addEventListener("click", this.onClick.bind(this));
    canvas.addEventListener("pointerdown", this.onMouseDown.bind(this));
    document.addEventListener("pointerup", this.onMouseUp.bind(this));

    // this.controls.update();
  }

  public setCalibratedMatrix(matrix: Matrix4, cameraMatrix: Matrix3) {
    this.callibratedCamera.matrixAutoUpdate = false;
    this.callibratedCamera.matrix.getInverse(matrix);
    // this.callibratedCamera.matrix.copy(matrix);
    this.callibratedCamera.updateMatrixWorld(true);



    console.log("ASDASDASDASD")
    // console.log(matrix.pos)

    
    // console.log("cameraMatrix",cameraMatrix)
    // cameraMatrix.
    // this.callibratedCamera.projectionMatrix.identity();

    const ret = makeProjectionMatrix(cameraMatrix, new Vector2(this.imageWidth, this.imageHeight))
    // console.log("##",ret);

    // const c = cameraMatrix.toArray();
    
    // this.callibratedCamera.fov = ret.fovy;
    // this.callibratedCamera.projectionMatrix.makePerspective(ret.fovx, canvas?.parentElement!.offsetWidth / canvas?.parentElement!.offsetHeight, 0.1, 1000)
    // this.callibratedCamera.projectionMatrix.set(
      //   c[0], c[1], c[2], 0,
      //   c[3], c[4], c[5], 0,
      //   c[6], c[7], c[8], 0
      // )
      // this.callibratedCamera.projectionMatrix.multiply()
      // this.callibratedCamera.projectionMatrix.makeOrthographic(-10,10,-10,10,0,10000)
      // this.callibratedCamera.projectionMatrix.makePerspective(    45,
      //   innerWidth / innerHeight,
      //   0.1,
      //   100000);
      this.callibratedCamera.projectionMatrix.copy(ret);
    this.callibratedCamera.projectionMatrixInverse.getInverse(
      this.callibratedCamera.projectionMatrix
    );
    // //
    // console.log("Projec", this.callibratedCamera.projectionMatrix);
    // this.callibratedCamera.updateMatrix();

    // this.callibratedCamera.updateProjectionMatrix();
    // this.callibratedCamera.updateMatrix();
    // console.log(matrix)
  }

  public render() {
    const w = canvas?.parentElement!.offsetWidth;
    const h = canvas?.parentElement!.offsetHeight;
    this.renderer.setSize(w, h);

    this.camera.left = 0;
    this.camera.right = this.imageWidth;
    this.camera.top = (this.imageAspect * this.imageHeight) / (w / h);
    this.camera.bottom = 0;
    this.camera.updateProjectionMatrix();

    this.renderer.render(this.scene, this.camera);
    this.renderer.autoClearColor = false;
    
    
    this.renderer.render(this.calibratedScene, this.callibratedCamera);
    this.renderer.autoClearColor = true;

    // requestAnimationFrame(() => this.render());
    // this.adjustCanvasSize();
    this.controls.update();
    this.controls2.update();
  }

  private imageCoordinate(x: number, y: number): Vector2 | undefined {
    this.raycaster.setFromCamera(new Vector2(x, y), this.camera);
    const intersects = this.raycaster.intersectObject(this.scene, true);
    if (intersects.length > 0) {
      for (const inter of intersects) {
        if (inter.object.name === "image") {
          return new Vector2(inter.point.x, inter.point.y);
        }
      }
    }
  }

  get markers() {
    const markers: Object3D[] = [];
    this.scene.traverse((o) => {
      if (o.userData["marker"]) {
        markers.push(o);
      }
    });
    return markers;
  }

  private raycastMarker(x: number, y: number): Selection | undefined {
    const coord = this.imageCoordinate(x, y);

    const c = new Vector3(coord?.x, coord?.y);

    const sorted = this.markers.sort((a, b) =>
      a.position.distanceToSquared(c) < b.position.distanceToSquared(c) ? -1 : 0
    );
    if (sorted.length > 0 && sorted[0].position.distanceTo(c) < 30) {
      return sorted[0] as Selection;
    } else {
      return undefined;
    }
  }

  public onMouseMove(event: MouseEvent) {
    // event.preventDefault();
    const x = (event.offsetX / canvas.offsetWidth) * 2 - 1;
    const y = -(event.offsetY / canvas.offsetHeight) * 2 + 1;

    if (this.mouseDown) {
      this.mouseDownMoved = true;

      if (this.selectedMarker) {
        const imageCoord = this.imageCoordinate(x, y);
        if (imageCoord) {
          this.selectedMarker.position.set(imageCoord.x, imageCoord.y, 0);
          this.onMoveMarker(this.selectedMarker);
        }
      }
    }

    this.mouse.x = x;
    this.mouse.y = y;

    const marker = this.raycastMarker(x, y);
    if (marker) {
      canvas.style.cursor = "pointer";
    } else {
      canvas.style.cursor = "initial";
    }
  }

  public onClick(event: MouseEvent) {
    const x = (event.offsetX / canvas.offsetWidth) * 2 - 1;
    const y = -(event.offsetY / canvas.offsetHeight) * 2 + 1;

    const imageCoord = this.imageCoordinate(x, y);
    if (imageCoord && this.placeMarkerOnClick) {
      this.onPlaceMarker(imageCoord);
    }
  }

  public onMouseDown(event: MouseEvent) {
    this.mouseDown = true;
    this.mouseDownMoved = false;

    const marker = this.raycastMarker(this.mouse.x, this.mouse.y);
    if (marker) {
      this.selectedMarker = marker;
    } else {
      this.selectedMarker = undefined;
    }
  }
  public onMouseUp(event: MouseEvent) {
    this.mouseDown = false;
    this.selectedMarker = undefined;
  }

  public createSelectionMesh(point: Vector2) {
    const m = new Selection();
    m.userData["marker"] = true;
    m.position.set(point.x, point.y, 0);
    m.color = new Color(0xff0000);
    this.scene.add(m);
    return m;
  }

  set aspect(viewAspect: number) {
    // console.log(this.imageAspect)
  }
}

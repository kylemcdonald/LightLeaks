import {
  Color,
  PerspectiveCamera,
  Scene,
  Vector3,
  WebGLRenderer,
  Vector2,
  Raycaster,
  Renderer,
  GridHelper,
  AxesHelper,
} from 'three';

import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls';
import { FlyControls } from 'three/examples/jsm/controls/FlyControls';
import { Model } from './model';
import { Selection } from './selection';

const canvas = document.getElementById('model-canvas')! as HTMLCanvasElement;

export class ModelView {

  onSelectVertex: (pos: Vector3|null) => void = ()=>{};

  public readonly renderer  = new WebGLRenderer({
    antialias: true,
    canvas,
  });

  public readonly scene = new Scene();
  public readonly camera = new PerspectiveCamera(
    60,
    canvas?.parentElement!.offsetWidth / canvas?.parentElement!.offsetHeight,
    0.1,
    10000
  );

  public seletedVertex: Vector3 | null = null;

  public controls: OrbitControls;

  private brick: Model;

  private mouse: Vector2 = new Vector2();
  private raycaster = new Raycaster();

  private selectorMesh = new Selection();

  constructor() {
    this.renderer.setClearColor(new Color('rgb(50,50,50)'));


    this.controls = new OrbitControls(this.camera, this.renderer.domElement);
    this.brick = new Model();
    this.scene.add(this.brick);
    this.scene.add(this.selectorMesh);

    this.scene.add(new AxesHelper(30))

    this.camera.position.set(2, 2, 2);
    this.camera.lookAt(new Vector3(0, 0, 0));

    this.controls.update();

    canvas.addEventListener(
      'mousemove',
      this.onMouseMove.bind(this),
      false
    );

    canvas.addEventListener("click", this.onClick.bind(this));

  }

  public render() {
    this.renderer.setSize(canvas?.parentElement!.offsetWidth, canvas?.parentElement!.offsetHeight);
    this.renderer.render(this.scene, this.camera);

    this.camera.aspect = canvas?.parentElement!.offsetWidth / canvas?.parentElement!.offsetHeight;
    this.camera.updateProjectionMatrix();
    
    this.controls.update();

    if (this.seletedVertex) {
      this.selectorMesh.position.copy(this.seletedVertex);
      this.selectorMesh.visible = true;
      this.selectorMesh.setSelected(true);
    } else {
      this.raycaster.setFromCamera(this.mouse, this.camera);
    
      const intersects = this.raycaster.intersectObjects(
        this.brick.children,
        true
      );

      const threshold = 50;
      // var intersects = rayCaster.intersectObject(mesh, true);
      if (intersects.length > 0) {
        // console.log(intersects[0])
        let distance;
        const intersect = intersects[0];
        const point = intersect.point;
        const object = intersect.object;
        let snap = null;
        const test = object.worldToLocal(point);
        const points = this.brick.verticesPositions;

        let curDist = -1;
        for (let i = 0; i < points.length; i++) {
          distance = points[i].distanceTo(test);
          if (distance > threshold) {
            continue;
          } else if (curDist === -1 || distance < curDist) {
            curDist = distance;
            snap = object.localToWorld(points[i].clone());
          }
        }
        if (snap) {
          this.selectorMesh.position.copy(snap);
          this.selectorMesh.visible = true;
          this.selectorMesh.setSelected(false);
        } else {
          this.selectorMesh.visible = false;
        }
      }
    }


    // localStorage.setItem('model-camera', 'Tom');
  }

  public createSelectionMesh(point: Vector3){
    const m = new Selection();;
    m.position.copy(point);
    m.color = new Color(0xff0000);
    this.scene.add(m);
    return m;
  }

  public onMouseMove(event: MouseEvent) {
    event.preventDefault();
    const x = (event.clientX / canvas.offsetWidth) * 2 - 1;
    const y = -(event.clientY / canvas.offsetHeight) * 2 + 1;

    this.mouse.x = x;
    this.mouse.y = y;
  }

  public onClick(event: MouseEvent) {
    if (this.selectorMesh.visible && !this.seletedVertex) {
      this.seletedVertex = this.selectorMesh.position.clone();
    } else {
      this.seletedVertex = null;
    }
    this.onSelectVertex(this.seletedVertex);
  }

  set aspect(aspect: number){
    
  }
}

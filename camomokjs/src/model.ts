import {
  Color,
  Scene,
  Mesh,
  MeshBasicMaterial,
  LoadingManager,
  ShaderMaterial,
  BufferGeometry,
  Points,
  Vector3,
  Box3,
} from "three";
import { ColladaLoader } from "three/examples/jsm/loaders/ColladaLoader";

type modeDefinition = "shaded" | "wireframe" | "xyzMap";

const xyzMapVertexShader = `
varying vec3 pos;

void main() {
  vec4 worldPosition = modelMatrix * vec4( position, 1.0 );
  pos = worldPosition.xyz;
  gl_Position = projectionMatrix * modelViewMatrix * vec4( position, 1.0 );
}`

const xyzMapFragShader = `
uniform vec3 zero;
uniform float range;

varying vec3 pos;

void main() {
  gl_FragColor = vec4((pos.xyz - zero) / range, 1.);
}
`
export class Model extends Scene {
  vertices: Points[] = [];
  verticesPositions: Vector3[] = [];
  bbox: Box3;

  private color: Color;
  private opacity: number;
  private _mode : modeDefinition;

  constructor(
    path: string,
    opacity: number = 0.2,
    color: Color = new Color("white"),
    mode: modeDefinition = "wireframe"
  ) {
    super();

    this.opacity = opacity;
    this.color = color;

    const loadingManager = new LoadingManager(() => {});
    const loader = new ColladaLoader(loadingManager);

    loader.load(path, (collada) => {
      this.add(collada.scene);
      // collada.scene.scale.set(1, 1, 1);//= 10;
      // collada.scene.rotation.set(0,0,0)
      // console.log(collada.scene)

      this.bbox = new Box3();
      this.bbox.setFromObject(collada.scene)

      collada.scene.traverse((child) => {
        // console.log(child.getBoundingBoxes())
        if (child instanceof Mesh) {
          const mesh = child;
          // mesh.material = material;
          // (mesh.geometry as Geometry).computeBoundingBox();
          // console.log((mesh.geometry as Geometry).boundingBox)

          if (mesh.geometry instanceof BufferGeometry) {
            // const geometry = mesh.geometry.clone();
            // const particles = new Points( geometry, vertexMaterial );
            for (let i = 0; i < mesh.geometry.attributes.position.count; i++) {
              // if(i < 100){
              //   console.log("#####")
              //   console.log(this.localToWorld(new Vector3(
              //     mesh.geometry.attributes.position.getX(i),
              //     mesh.geometry.attributes.position.getY(i),
              //     mesh.geometry.attributes.position.getZ(i))))
              //   console.log((new Vector3(
              //     mesh.geometry.attributes.position.getX(i),
              //     mesh.geometry.attributes.position.getY(i),
              //     mesh.geometry.attributes.position.getZ(i))))
              // }

              this.verticesPositions.push(
                this.localToWorld(
                  new Vector3(
                    mesh.geometry.attributes.position.getX(i),
                    mesh.geometry.attributes.position.getY(i),
                    mesh.geometry.attributes.position.getZ(i)
                  )
                )
              );
            }

            // collada.scene.add( particles );
          }
          // console.log(
        }

        // console.log(this.verticesPositions)
      });

      this.mode = mode;
    });
  }

  set mode(mode: modeDefinition) {
    this._mode = mode;
    let material;
    if (mode == "wireframe") {
      material = new MeshBasicMaterial({
        wireframe: true,
        color: this.color,
        opacity: this.opacity,
        transparent: true,
      });
    } else if (mode == "shaded") {
      material = new MeshBasicMaterial({
        wireframe: false,
        color: this.color,
        opacity: this.opacity,
        transparent: true,
      });
    } else if(mode == "xyzMap"){
      let range = Math.max(
        Math.abs(this.bbox.min.x),
        Math.abs(this.bbox.min.y),
        Math.abs(this.bbox.min.z),
        Math.abs(this.bbox.max.x),
        Math.abs(this.bbox.max.y),
        Math.abs(this.bbox.max.z),
      )
      // console.log(this.bbox)
      material = new ShaderMaterial({
        uniforms: {
          range: { value: range },
					zero: { value: new Vector3(0,0,0) }
        },
        vertexShader: xyzMapVertexShader,
        fragmentShader: xyzMapFragShader
      })
    }

    this.traverse((child) => {
      if (child.type === "Mesh") {
        const mesh = child as Mesh;
        mesh.material = material;
      }
    });
  }

  get mode(){
    return this._mode;
  }
}

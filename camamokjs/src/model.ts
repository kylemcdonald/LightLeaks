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
  PointsMaterial,
  PerspectiveCamera,
  Vector2,
  Material,
  DoubleSide,
} from "three";
import { ColladaLoader } from "three/examples/jsm/loaders/ColladaLoader";

type modeDefinition = "shaded" | "wireframe" | "xyzMap" | "xray";

const xyzMapVertexShader = `
varying vec3 pos;

void main() {
  vec4 worldPosition = modelMatrix * vec4( position, 1.0 );
  pos = worldPosition.xyz;
  gl_Position = projectionMatrix * modelViewMatrix * vec4( position, 1.0 );
}`;

const xyzMapFragShader = `
uniform vec3 zero;
uniform float range;

varying vec3 pos;

void main() {
  gl_FragColor = vec4((pos.xyz - zero) / range, 1.);
}
`;
export class Model extends Scene {
  // vertices: Points[] = [];
  private verticesPositions: {
    pos: Vector3;
    mesh: Mesh[];
  }[] = [];
  private bbox: Box3;

  private color: Color;
  private opacity: number;
  private _mode: modeDefinition;

  constructor(
    path: string,
    opacity: number = 0.2,
    color: Color = new Color("white"),
    mode: modeDefinition = "wireframe"
  ) {
    super();

    this.opacity = opacity;
    this.color = color;

    var vertexMaterial = new PointsMaterial({
      color: color,
      size: 3,
      sizeAttenuation: false,
    });

    const loadingManager = new LoadingManager(() => { });
    const loader = new ColladaLoader(loadingManager);

    loader.load(path, (collada) => {
      this.add(collada.scene);

      this.bbox = new Box3();
      this.bbox.setFromObject(collada.scene);

      let index = 0;
      collada.scene.traverse((child) => {
        if (child instanceof Mesh) {
          index++;

          const mesh = child;

          if (mesh.geometry instanceof BufferGeometry) {
            const geometry = mesh.geometry.clone();
            const particles = new Points(geometry, vertexMaterial);
            for (let i = 0; i < mesh.geometry.attributes.position.count; i++) {
              const p = mesh.localToWorld(
                new Vector3(
                  mesh.geometry.attributes.position.getX(i),
                  mesh.geometry.attributes.position.getY(i),
                  mesh.geometry.attributes.position.getZ(i)
                )
              );

              // Filter out duplicate vertex positions
              const epsilon = 0.001;
              const found = this.verticesPositions.find(
                (v) => v.pos.distanceToSquared(p) < epsilon
              );
              if (!found) {
                this.verticesPositions.push({ pos: p, mesh: [mesh] });
              } else {
                found.mesh.push(mesh);
              }
            }

            // collada.scene.add(particles);
          }
        }
      });

      this.mode = mode;
    });
  }

  findClosestVertex(pos: Vector2, camera: PerspectiveCamera) {
    let pos3 = new Vector3(pos.x, pos.y, 0);
    let closestPoint = new Vector3();
    let closestDist = -1;
    const projectVertex = new Vector3();
    let closestIndex = -1;

    for (const [i, v] of this.verticesPositions.entries()) {
      const projectedPoint = projectVertex.copy(v.pos).project(camera);
      projectedPoint.z = 0;
      const dist = pos3.distanceToSquared(projectedPoint);
      if (closestDist == -1 || closestDist > dist) {
        closestDist = dist;
        closestPoint.copy(v.pos);
        closestIndex = i;
      }
    }

    return {
      vertex: closestPoint,
      dist: Math.sqrt(closestDist),
      index: closestIndex,
    };
  }

  highlightVertexIndex(index: number) {
    if (index == -1) return;
    const vertex = this.verticesPositions[index];

    this.traverse((child) => {
      if (child instanceof Mesh) {
        (child.material as Material).opacity = this.opacity;
      }
    });

    for (const mesh of vertex.mesh) {
      (mesh.material as Material).opacity = this.opacity * 2;
    }
  }

  set mode(mode: modeDefinition) {
    this._mode = mode;
    let material;
    if (mode == "wireframe") {
      material =
        new MeshBasicMaterial({
          wireframe: true,
          color: this.color,
          opacity: this.opacity,
          transparent: true,
        });
    } else if (mode == "xray") {
      material = [
        new MeshBasicMaterial({
          wireframe: true,
          color: this.color,
          opacity: this.opacity,
          transparent: true,
        }),
        new MeshBasicMaterial({
          wireframe: false,
          color: new Color('rgb(0,0,0)'),
          opacity: 0.3,
          transparent: true,
          depthWrite: false,
          side: DoubleSide
        }),
      ];
    } else if (mode == "shaded") {
      material = new MeshBasicMaterial({
        wireframe: false,
        color: this.color,
        opacity: this.opacity,
        transparent: true,
      });
    } else if (mode == "xyzMap") {
      let range = Math.max(
        Math.abs(this.bbox.min.x),
        Math.abs(this.bbox.min.y),
        Math.abs(this.bbox.min.z),
        Math.abs(this.bbox.max.x),
        Math.abs(this.bbox.max.y),
        Math.abs(this.bbox.max.z)
      );
      console.log(this.bbox, range)
      material = new ShaderMaterial({
        uniforms: {
          range: { value: range },
          zero: { value: new Vector3(this.bbox.min.x, this.bbox.min.y, this.bbox.min.z) },
        },
        vertexShader: xyzMapVertexShader,
        fragmentShader: xyzMapFragShader,
      });
    }

    this.traverse((child) => {
      if (child.type === "Mesh" || child.type === "LineSegments") {
        const mesh = child as Mesh;
        mesh.material = material;
        // mesh.material = material.clone();
      }
    });
  }

  get mode() {
    return this._mode;
  }
}

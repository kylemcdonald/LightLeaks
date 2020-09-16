import {
  BoxGeometry,
  Color,
  Scene,
  Mesh,
  MeshBasicMaterial,
  LoadingManager,
  ShaderMaterial,
  TextureLoader,
  BufferGeometry,
  Points,
  BufferAttribute,
  PointsMaterial,
  Vector3
} from "three";
import { ColladaLoader } from "three/examples/jsm/loaders/ColladaLoader";

// const model = require("../../assets/lan_model.dae");
const model = require("../../assets/model.dae");
export class Model extends Scene {

  vertices: Points[] = [];
  verticesPositions: Vector3[] =[];

  constructor(opacity:number=0.2, color: Color=new Color("white")) {
    super();

    const material = new MeshBasicMaterial({
      wireframe: true,
      color,
      opacity,
      transparent: true,
    });
    // const material = new MeshBasicMaterial({
    //   wireframe: false,
    //   color,
    //   opacity: 0.5,
    //   transparent: true,
    // });

    // const sprite = new TextureLoader().load( require("../../assets/disc.png").default);
    // const vertexMaterial = new PointsMaterial({color: new Color("rgb(255,255,255)"), size: 10, map:sprite, sizeAttenuation: false, alphaTest: 0.5, transparent: true, depthTest:true});

    const loadingManager = new LoadingManager(() => {});
    const loader = new ColladaLoader(loadingManager);

    loader.load(model.default, (collada) => {
      this.add(collada.scene);
      collada.scene.traverse((child) => {
        if (child.type === 'Mesh') {
          const mesh = child as Mesh;
          mesh.material = material;

          if(mesh.geometry instanceof BufferGeometry){
            // const geometry = mesh.geometry.clone();
            // const particles = new Points( geometry, vertexMaterial );
            for(let i=0; i<mesh.geometry.attributes.position.count; i++){
              this.verticesPositions.push(this.localToWorld(new Vector3(
                mesh.geometry.attributes.position.getX(i),
                mesh.geometry.attributes.position.getY(i),
                mesh.geometry.attributes.position.getZ(i)))
              );
            }
            
            // collada.scene.add( particles );
          }
          // console.log(
        }
      });
    });
  }
}

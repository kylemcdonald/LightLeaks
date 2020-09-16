import { TextureLoader, PointsMaterial, Color, Mesh, Points, Geometry, Scene, SphereGeometry, MeshBasicMaterial, Material, Vector3, BufferGeometry, Float32BufferAttribute } from "three";


const sprite = new TextureLoader().load( require("../../assets/disc.png").default);


// const geometry = new SphereGeometry( 0.1, 16, 16 );
const geometry = new BufferGeometry();
geometry.setAttribute('position',  new Float32BufferAttribute( [0,0,0], 3 ) );

export class Selection extends Mesh {
  private sphereMaterial: PointsMaterial;
  
  constructor(){
    super();
    //  new Points( new Geometry(), vertexMaterial);

    // this.sphereMaterial = new MeshBasicMaterial( {color: 0xffff00} );

    this.sphereMaterial = new PointsMaterial({color: new Color("rgb(255,255,255)"), size: 20, map:sprite, sizeAttenuation: false, alphaTest: 0.5, transparent: true, depthTest:false});

    const sphere = new Points( geometry, this.sphereMaterial);
    this.add( sphere );

    this.setSelected(false)
  }

  setSelected(set:boolean){
    if(set){
      this.color = new Color(0x55ff00);
    } else {
      this.color = new Color(0xffff00);
    }
  }

  set color(color: Color){
    this.sphereMaterial.color = color;
  }
}
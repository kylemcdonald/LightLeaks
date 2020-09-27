import {
  TextureLoader,
  PointsMaterial,
  Color,
  Mesh,
  Points,
  BufferGeometry,
  Float32BufferAttribute,
} from "three";

const sprite = new TextureLoader().load("/circle_hole.png");

// const geometry = new SphereGeometry( 0.1, 16, 16 );
const geometry = new BufferGeometry();
geometry.setAttribute("position", new Float32BufferAttribute([0, 0, 0], 3));

export class MarkerMesh extends Mesh {
  private sphereMaterial: PointsMaterial;

  constructor(color=new Color(0xffff00), opacity=1) {
    super();
    this.sphereMaterial = new PointsMaterial({
      color,
      size: 25,
      map: sprite,
      sizeAttenuation: false,
      // alphaTest: 0.5,
      transparent: true,
      depthTest: false,
      opacity
    });
    // this.sphereMaterial.depthTest = false;
    const sphere = new Points(geometry, this.sphereMaterial);
    sphere.renderOrder = 1000;
    this.add(sphere);

    // this.setSelected(false);
  }

  // setSelected(set: boolean) {
  //   if (set) {
  //     this.color = new Color(0x55ff00);
  //   } else {
  //     this.color = new Color(0xffff00);
  //   }
  // }

  set color(color: Color) {
    this.sphereMaterial.color = color;
  }
}

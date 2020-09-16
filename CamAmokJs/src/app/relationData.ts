/*  tslint:disable:max-classes-per-file */

import { Vector2, Vector3, Geometry, Mesh } from "three";

export class RelationDataElement {
  public modelPoint: Vector3;
  // public imagePoint: Vector2;

  public modelMesh: Mesh | undefined;
  public imageMesh: Mesh | undefined;

  get imagePoint(){
    const ret = new Vector2();
    if(this.imageMesh){
      ret.set(this.imageMesh.position.x, this.imageMesh.position.y);
    }
    return ret;
  }

  constructor(modelPoint: Vector3) {
    this.modelPoint = modelPoint;
    // this.imagePoint = imagePoint;
    // console.log(imagePoint)
  }
}

// export class RelationData {
//   private data: RelationDataElement[] = [];

//   constructor() {}

//   addPoint(modelPoint: Vector3, imagePoint: Vector2) {
//     const d = new RelationDataElement(modelPoint, imagePoint);

//     this.data.push(d);

//     return d;
//   }
// }

/* tslint:disable:no-bitwise */

import { RelationDataElement } from "./relationData";
import { Vector2, Matrix4, Vector3, Quaternion, Matrix3 } from "three";

const cv = require("../opencv4.4.0/opencv.js");
// const cv = require('../opencv4.2.0/opencv.js');

let loaded = false;

cv["onRuntimeInitialized"] = () => {
  loaded = true;
};

export async function waitForLoad() {
  if (loaded) {
    return Promise.resolve();
  }

  return new Promise((resolve, reject) => {
    cv["onRuntimeInitialized"] = () => {
      loaded = true;
      resolve();
    };
  });
}

/**
 * Convert rotation and translation matrixes from openCv
 * to a three.Matrix4
 * from https://github.com/kylemcdonald/ofxCv/blob/master/libs/ofxCv/src/Helpers.cpp#L10
 * @param rotation
 * @param translation
 */
function makeMatrix(rotation: any, translation: any) {
  const rot3x3 = new cv.Mat();
  cv.Rodrigues(rotation, rot3x3);

  const rm = rot3x3.data64F;
  const tm = translation.data64F;

  console.log(tm)
  
  const rMatrix = new Matrix4();
  // rMatrix.set(
  //   rm[0], rm[3],  rm[6],  0*tm[0],
  //   rm[1], rm[4],  rm[7],  0*tm[1],
  //   rm[2], rm[5],  rm[8],  0*tm[2],
  //   0,
  //   0,
  //   0,
  //   1.0
  // );

  rMatrix.set(
    rm[0], rm[1],  rm[2],  0*tm[0],
    rm[3], rm[4],  rm[5],  0*tm[1],
    rm[6], rm[7],  rm[8],  0*tm[2],
    0,
    0,
    0,
    1.0
  );

  const tMatrix = new Matrix4();
  tMatrix.setPosition(tm[0],tm[1],tm[2])
  // tMatrix.setPosition(-tm[0],-tm[1],-tm[2])

  // const iMatrix = new Matrix4();
  // iMatrix.identity();
  // iMatrix.elements[5] = -1;
  // iMatrix.elements[10] = -1;

  // console.log(iMatrix);
  
  // const matrix = rMatrix.multiply(tMatrix);
  const matrix = tMatrix.multiply(rMatrix);


  rot3x3.delete();
  return matrix;
}

export function calibrateCamera(
  data: RelationDataElement[],
  imageSize: Vector2
) {
  // console.log("Calbrate camera");

  let modelPoints = [];
  let imagePoints = [];

  for (let d of data) {
    modelPoints.push(d.modelPoint.x);
    modelPoints.push(d.modelPoint.y);
    modelPoints.push(d.modelPoint.z);

    imagePoints.push(d.imagePoint.x);
    imagePoints.push(d.imagePoint.y);
  }

  modelPoints = cv.matFromArray(data.length, 3, cv.CV_32F, modelPoints);
  imagePoints = cv.matFromArray(data.length, 2, cv.CV_32F, imagePoints);
  const imagePointsArr = new cv.MatVector();
  const objectPointsArr = new cv.MatVector();

  imagePointsArr.push_back(imagePoints);
  objectPointsArr.push_back(modelPoints);
  const imageSizeCv = new cv.Size(imageSize.x, imageSize.y);

  const f = 1.39626 * imageSize.x;
  // const fovx = 107.0/360.0*2.0 * Math.PI;
  // const f = imageSize.width / 2 * Math.atan(fovx/2)

  // console.log("f",f)
  const intr = cv.matFromArray(3, 3, cv.CV_64F, [
    f,
    0,
    imageSize.x / 2,
    0,
    f,
    imageSize.y / 2,
    0,
    0,
    1,
  ]);

  const dist = new cv.Mat();
  // const intr = new cv.Mat();
  const rvecs = new cv.MatVector();
  const tvecs = new cv.MatVector();
  const stdDeviationsIntrinsics = new cv.Mat();
  const stdDeviationsExtrinsics = new cv.Mat();

  const perViewErrors = new cv.Mat();
  const flag =
    cv.CALIB_USE_INTRINSIC_GUESS |
    // cv.CALIB_FIX_PRINCIPAL_POINT |
    cv.CALIB_FIX_ASPECT_RATIO |
    cv.CALIB_FIX_K1 |
    cv.CALIB_FIX_K2 |
    cv.CALIB_FIX_K3 |
    cv.CALIB_ZERO_TANGENT_DIST;

  cv.calibrateCameraExtended(
    objectPointsArr,
    imagePointsArr,
    imageSizeCv,
    intr,
    dist,
    rvecs,
    tvecs,
    stdDeviationsIntrinsics,
    stdDeviationsExtrinsics,
    perViewErrors,
    flag
  );

  const intrData = intr.data64F;
  const distData = dist.data64F;

  const matrix = makeMatrix(rvecs.get(0), tvecs.get(0));

  intr.delete();
  dist.delete();
  stdDeviationsIntrinsics.delete();
  stdDeviationsExtrinsics.delete();
  rvecs.delete();
  tvecs.delete();
  perViewErrors.delete();

  // console.log("intrData",intrData);

  const cameraMatrix = new Matrix3();
  cameraMatrix.fromArray(intrData);

  return {
    matrix,
    cameraMatrix,
    distCoeffs: distData,
  };
}

/** JS implementation of native openCV function not packed in the js library
 * https://github.com/opencv/opencv/blob/master/modules/calib3d/src/calibration.cpp#L3821
 */
export function calibrationMatrixValues(
  cameraMatrix: Matrix3,
  imageSize: Vector2
) {
  const k: number[] = [];
  cameraMatrix.toArray(k);
// console.log(k)
  const K = (col: number, row: number) => {
    return k[col * 3 + row];
  };

  /* Calculate pixel aspect ratio. */
  const aspectRatio = K(1, 1) / K(0, 0);
  
  /* Calculate number of pixel per realworld unit. */
  const mx = 1.0;
  const my = aspectRatio;

  /* Calculate fovx and fovy. */
  let fovx =
    Math.atan2(K(0, 2), K(0, 0)) +
    Math.atan2(imageSize.width - K(0, 2), K(0, 0));
  let fovy =
    Math.atan2(K(1, 2), K(1, 1)) +
    Math.atan2(imageSize.height - K(1, 2), K(1, 1));
  fovx *= 180.0 / Math.PI;
  fovy *= 180.0 / Math.PI;

  /* Calculate focal length. */
  const focalLength = K(0, 0) / mx;
  /* Calculate principle point. */
  const principalPoint = new Vector2(K(0, 2) / mx, K(1, 2) / my);

  const fx = K(0, 0);
  const fy = K(1, 1);

  return {
    fovx,
    fovy,
    aspectRatio,
    focalLength,
    principalPoint,
    fx,
    fy,
  };
}

export function makeProjectionMatrix(
  cameraMatrix: Matrix3,
  imageSize: Vector2,
  nearDist = 0.1,
  farDist = 10000
) {
  const { principalPoint, fx, fy, fovx, fovy} = calibrationMatrixValues(
    cameraMatrix,
    imageSize
  );
  const w = imageSize.x;
  const h = imageSize.y;
  const cx = principalPoint.x;
  const cy = principalPoint.y;

  const matrix = new Matrix4();
  // flipped these compared to oF:
  matrix.makePerspective(
    (nearDist * (w - cx)) / fx,
    (nearDist * -cx) / fx,
    (nearDist * (cy - h)) / fy,
    (nearDist * cy) / fy,
    nearDist,
    farDist
  );
  return matrix;
}

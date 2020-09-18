/* tslint:disable:no-bitwise */

// import { RelationDataElement } from "./relationData";
import { Vector2, Matrix4, Vector3, Quaternion, Matrix3 } from "three";


// Copied correct constant values from c++.
// Values in cv.CALIB_* have wrong values
const CV_CALIB_USE_INTRINSIC_GUESS =  1;
const CV_CALIB_FIX_ASPECT_RATIO =     2;
const CV_CALIB_FIX_PRINCIPAL_POINT =  4;
const CV_CALIB_ZERO_TANGENT_DIST =    8;
const CV_CALIB_FIX_FOCAL_LENGTH = 16;
const CV_CALIB_FIX_K1 =  32;
const CV_CALIB_FIX_K2 =  64;
const CV_CALIB_FIX_K3 =  128;
const CV_CALIB_FIX_K4 =  2048;
const CV_CALIB_FIX_K5 =  4096;
const CV_CALIB_FIX_K6 =  8192;
const CV_CALIB_RATIONAL_MODEL = 16384;

declare const cv: any;

// import cv from "./opencv4.4.0/opencv";
// import "./opencv4.4.0/opencv";
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

  let matrix = new Matrix4();
  matrix.set(
    rm[0], rm[1],  rm[2],  tm[0],
    rm[3], rm[4],  rm[5],  tm[1],
    rm[6], rm[7],  rm[8],  tm[2],
    0, 0, 0, 1.0
  );

  // const invertMatrix = new Matrix4();
  // // invertMatrix.makeScale(-1,-1,-1);
  // matrix = invertMatrix.multiply(matrix)
  
  // const invertzAxisMatrix = new Matrix4();
  // invertzAxisMatrix.makeScale(1,1,1);
  // matrix = matrix.multiply(invertzAxisMatrix)
  
  rot3x3.delete();
  return matrix;
}

export function calibrateCamera(
  _modelPoints: Vector3[],
  _imagePoints: Vector2[],
  imageSize: Vector2
) {
  // console.log("Calbrate camera");

  let modelPoints = [];
  let imagePoints = [];

  for (let d of _modelPoints) {
    modelPoints.push(d.x);
    modelPoints.push(d.y);
    modelPoints.push(d.z);
  }
  for (let d of _imagePoints) {
    imagePoints.push(d.x);
    imagePoints.push(d.y);
  }

  modelPoints = cv.matFromArray(_modelPoints.length, 3, cv.CV_32F, modelPoints);
  imagePoints = cv.matFromArray(_imagePoints.length, 2, cv.CV_32F, imagePoints);
  const imagePointsArr = new cv.MatVector();
  const objectPointsArr = new cv.MatVector();

  imagePointsArr.push_back(imagePoints);
  objectPointsArr.push_back(modelPoints);
  const imageSizeCv = new cv.Size(imageSize.x, imageSize.y);

  // console.log("modelPoints", modelPoints.data32F)

  // const f = 1.39626 * imageSize.x;
  const fovx = 107.0/360.0*2.0 * Math.PI;
  const f = imageSize.width / 2 * Math.atan(fovx/2)

  // console.log("f",f)
  const intr = cv.matFromArray(3, 3, cv.CV_64F, [
    f, 0, imageSize.x / 2,
    0, f, imageSize.y / 2,
    0, 0, 1,
  ]);

  const dist = new cv.Mat();
  const rvecs = new cv.MatVector();
  const tvecs = new cv.MatVector();
  const stdDeviationsIntrinsics = new cv.Mat();
  const stdDeviationsExtrinsics = new cv.Mat();

  const perViewErrors = new cv.Mat();

  const flag =
    CV_CALIB_USE_INTRINSIC_GUESS +
    CV_CALIB_FIX_PRINCIPAL_POINT +
    CV_CALIB_FIX_ASPECT_RATIO +
    CV_CALIB_FIX_K1 +
    CV_CALIB_FIX_K2 +
    CV_CALIB_FIX_K3 +
    CV_CALIB_ZERO_TANGENT_DIST;
  // const flag = 235;

  // console.log(cv.CALIB_FIX_K3)
  // console.log("######## INPUT ");
  // console.log("objectPointsArr", objectPointsArr.get(0).data32F);
  // console.log("imagePointsArr", imagePointsArr.get(0).data32F);
  // console.log("imageSizeCv", imageSizeCv);
  // console.log("intr", intr.data64F);
  // console.log("dist", dist.data64F);
  // console.log("flag", flag);


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

  // console.log("#### OUTPUT")
  // console.log("tvecs",tvecs.get(0).data64F);
  // console.log("rvecs",rvecs.get(0).data64F);
  // console.log("intr",intr.data64F);
  console.log("stdDeviationsIntrinsics",stdDeviationsIntrinsics.data64F)
  console.log("stdDeviationsIntrinsics", stdDeviationsExtrinsics.data64F)
  console.log("perViewErrors", perViewErrors.data64F)

  const matrix = makeMatrix(rvecs.get(0), tvecs.get(0));
  const cameraMatrix = new Matrix3();
  cameraMatrix.fromArray(intrData);

  intr.delete();
  dist.delete();
  stdDeviationsIntrinsics.delete();
  stdDeviationsExtrinsics.delete();
  rvecs.delete();
  tvecs.delete();
  perViewErrors.delete();

  // console.log("intrData",intrData);


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
console.log("Camera Matrix", k)
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

  // console.log("focalLength",focalLength)
  // console.log("principalPoint", principalPoint);
  // console.log("fovx", fovx);
  // console.log("fovy", fovy);

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

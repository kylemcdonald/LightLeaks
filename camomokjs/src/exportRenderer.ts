import { PerspectiveCamera, WebGLRenderer, LinearFilter, NearestFilter, RGBAFormat, FloatType, WebGLRenderTarget, Scene, Vector2, Matrix4 } from "three";
import { makeProjectionMatrix } from "./cv";

export function renderSceneToArray(width, height, model, calibratedModelViewMatrix, cameraMatrix, imageSize) {
    const scene = new Scene();
    scene.add(model);

    const camera = new PerspectiveCamera();
    camera.matrixAutoUpdate = false;
    camera.matrix.getInverse(calibratedModelViewMatrix);
    camera.updateMatrixWorld(true);
    let projMatrix = makeProjectionMatrix(
      cameraMatrix,
      imageSize
    );

    // Flip Y
    const m = new Matrix4;
    m.makeScale(1,-1,1);
    projMatrix = projMatrix.multiply(m);

    camera.projectionMatrix.copy(projMatrix);
    camera.projectionMatrixInverse.getInverse(
      camera.projectionMatrix
    );

    const rtTexture = new WebGLRenderTarget(width, height, {
      minFilter: LinearFilter,
      magFilter: NearestFilter,
      format: RGBAFormat,
      type: FloatType,
    });

    const renderer = new WebGLRenderer();

  

    renderer.setRenderTarget( rtTexture );
    renderer.setSize( width, height );
    renderer.setViewport(0,0, width, height);
    renderer.render(scene, camera);

    const read = new Float32Array( width * height * 4 ); 
    renderer.readRenderTargetPixels( rtTexture, 0,0, width, height, read );
    scene.remove(model);

    return read;
    
  }

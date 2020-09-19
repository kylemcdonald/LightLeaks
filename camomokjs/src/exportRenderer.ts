import { PerspectiveCamera, WebGLRenderer, LinearFilter, NearestFilter, RGBAFormat, FloatType, WebGLRenderTarget, Scene, Vector2 } from "three";
import { makeProjectionMatrix } from "./cv";

export function renderSceneToArray(width, height, model, calibratedModelViewMatrix, cameraMatrix) {
    const scene = new Scene();
    scene.add(model);

    const camera = new PerspectiveCamera();
    camera.matrixAutoUpdate = false;
    camera.matrix.getInverse(calibratedModelViewMatrix);
    camera.updateMatrixWorld(true);
    const projMatrix = makeProjectionMatrix(
      cameraMatrix,
      new Vector2(width, height)
    );
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
    renderer.render(scene, camera);

    var read = new Float32Array( width * height * 4 ); 
    renderer.readRenderTargetPixels( rtTexture, 0,0, width, height, read );
    
    scene.remove(model);
    
  }

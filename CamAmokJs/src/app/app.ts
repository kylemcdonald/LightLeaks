import {
  Color,
  PerspectiveCamera,
  Scene,
  Vector3,
  WebGLRenderer,
  Vector2,
  Raycaster,
  Matrix4,
  Matrix,
  Matrix3,
  Quaternion,
  CameraHelper,
} from "three";

import { ModelView } from "./modelView";
import { ImageView } from "./imageView";
import { RelationDataElement } from "./relationData";
import * as cv from "./cv";

// cv.load();

export class App {
  private relationData: RelationDataElement[] = [];

  private modelView: ModelView;
  private imageView: ImageView;

  constructor() {
    this.modelView = new ModelView();
    this.imageView = new ImageView();

    const cameraHelper = new CameraHelper(this.imageView.callibratedCamera);
    cameraHelper.scale.set(100,100,100);
    this.modelView.scene.add(cameraHelper);

    this.modelView.onSelectVertex = (pos) => {
      this.imageView.placeMarkerOnClick = pos ? true : false;
      console.log("Select", pos);
    };

    this.imageView.onPlaceMarker = (imagePoint) => {
      if (this.modelView.seletedVertex) {
        this.imageView.placeMarkerOnClick = false;
        console.log("Place", imagePoint);

        const p = new RelationDataElement(this.modelView.seletedVertex);
        this.relationData.push(p);

        p.modelMesh = this.modelView.createSelectionMesh(
          this.modelView.seletedVertex
        );

        p.imageMesh = this.imageView.createSelectionMesh(
          new Vector2(imagePoint.x, imagePoint.y)
        );

        this.modelView.seletedVertex = null;

        this.exportJsonData();
        this.calibrate();
      }
    };

    this.imageView.onMoveMarker = (marker) => {
      this.calibrate();
    };

    document.addEventListener("keypress", (e) => {
      if (e.keyCode == 115) {
        //s
        this.exportJsonData();
      }
    });

    this.render();

    // this.importJsonData([{"imagePoint":{"x":734.4000000000001,"y":3126.600000000001},"modelPoint":{"x":9.370000158691406,"y":6.89999976196289,"z":1.53210772112786e-15}},{"imagePoint":{"x":1312.2000000000003,"y":907.2000000000003},"modelPoint":{"x":7.50000034790039,"y":1.6653346141871395e-15,"z":-7.50000034790039}},{"imagePoint":{"x":4039.2000000000007,"y":3056.4000000000005},"modelPoint":{"x":-3.9649998413085936,"y":6.899999761962891,"z":-2.0999999191284164}},{"imagePoint":{"x":3915.000000000001,"y":1306.8},"modelPoint":{"x":-0.10499999837875366,"y":1.6653346141871395e-15,"z":-7.50000034790039}},{"imagePoint":{"x":1895.4000000000003,"y":1668.6000000000004},"modelPoint":{"x":4.850000085449219,"y":-7.482903436695307e-16,"z":3.3700001129150388}}])
    // this.importJsonData([
    //   {"imagePoint":{"x":10,"y":10},"modelPoint":{"x":0,"y":0,"z":0}},
    //   {"imagePoint":{"x":5000,"y":10},"modelPoint":{"x":10,"y":0,"z":0}},
    //   {"imagePoint":{"x":10,"y":3000},"modelPoint":{"x":0,"y":10,"z":0}},
    //   {"imagePoint":{"x":5000,"y":3000},"modelPoint":{"x":10,"y":10,"z":0}},
    //   {"imagePoint":{"x":103,"y":103},"modelPoint":{"x":0,"y":0,"z":10}},
    //   {"imagePoint":{"x":4103,"y":103},"modelPoint":{"x":9,"y":0,"z":10}}
    // ])

    // this.importJsonData([{"imagePoint":{"x":734.4000000000001,"y":3126.600000000001},"modelPoint":{"x":9.370000158691406,"y":6.89999976196289,"z":1.53210772112786e-15}},{"imagePoint":{"x":1312.2000000000003,"y":907.2000000000003},"modelPoint":{"x":7.50000034790039,"y":1.6653346141871395e-15,"z":-7.50000034790039}},{"imagePoint":{"x":4039.2000000000007,"y":3056.4000000000005},"modelPoint":{"x":-3.9649998413085936,"y":6.899999761962891,"z":-2.0999999191284164}},{"imagePoint":{"x":3915.000000000001,"y":1306.8},"modelPoint":{"x":-0.10499999837875366,"y":1.6653346141871395e-15,"z":-7.50000034790039}},{"imagePoint":{"x":1895.4000000000003,"y":1668.6000000000004},"modelPoint":{"x":4.850000085449219,"y":-7.482903436695307e-16,"z":3.3700001129150388}},{"imagePoint":{"x":1549.8000000000004,"y":2889.0000000000005},"modelPoint":{"x":5.61999998474121,"y":6.899999761962889,"z":4.710000103759767}},{"imagePoint":{"x":3585.600000000001,"y":340.1999999999998},"modelPoint":{"x":3.750000173950195,"y":2.4980018352221618e-15,"z":-11.250000134277343}}])

    // this.importJsonData([{"imagePoint":{"x":2129.1428571428564,"y":2431.542857142857},"modelPoint":{"x":4.455000164794922,"y":1.4139800583927528e-15,"z":-6.368000064086914}},{"imagePoint":{"x":2733.9428571428566,"y":2524.1142857142854},"modelPoint":{"x":4.667000015258789,"y":2.0528023806634306e-15,"z":-9.245000036621093}},{"imagePoint":{"x":1753.9392857142852,"y":3140.268749999999},"modelPoint":{"x":-1.8799999755859373,"y":1.959999937438966,"z":-5.779999908447265}},{"imagePoint":{"x":2567.3207734285716,"y":1044.4194077142852},"modelPoint":{"x":11.08799994506836,"y":1.2772005824365598e-15,"z":-5.752000067138671}},{"imagePoint":{"x":3703.8502144285712,"y":1622.2347957857132},"modelPoint":{"x":10.395000384521484,"y":2.06279429437262e-15,"z":-9.289999615478516}},{"imagePoint":{"x":608.9675800957039,"y":2754.0635175904},"modelPoint":{"x":0.2759999895095825,"y":1.0600000274658203,"z":-0.1680000046730039}},{"imagePoint":{"x":3506.737120234653,"y":3275.2650851944745},"modelPoint":{"x":1.314999966430664,"y":2.590000048828128,"z":-14.725000427246092}}]        )
   
  //  const d = [
  //   {imagePoint: { x: 979.0, y: 625.0 }, modelPoint: { x: -179.13387 , y: 180.1181     ,z: 0.  } },
  //   {imagePoint: { x: 1136.0, y:  801.0 }, modelPoint: { x: -255.74438 , y: 257.1639     ,z: 0.  } },
  //   {imagePoint: { x: 584.0, y: 538.0 }, modelPoint: { x:   0.   , y:  180.1181   ,z: 0.  } },
  //   {imagePoint: { x: 872.0, y: 502.0 }, modelPoint: { x: -5.905512 , y: 5.905512  ,z: 0.  } },
  //   {imagePoint: { x: 464.0, y: 462.0 }, modelPoint: { x: 336.61417 , y:  43.307087   ,z: 0.  } },
  //   {imagePoint: { x: 514.0, y: 451.0 }, modelPoint: { x: 336.61417  , y:  5.905512   ,z: 0.  } },
  //   {imagePoint: { x: 314.0, y: 814.0 }, modelPoint: { x: -179.13387 , y: 360.2362     ,z: 0.  } },
  //   {imagePoint: { x: 1249.0, y:  543.0 }, modelPoint: { x: -179.13387  , y:  0.         ,z: 0.  } },
  //   {imagePoint: { x: 109.0, y: 607.0 }, modelPoint: { x:  -5.905512 , y:354.33072    ,z: 0.  } },
  //   {imagePoint: { x: 328.0, y: 429.0 }, modelPoint: { x: 713.77954  , y: -5.905512   ,z: 0.  } },
  //   {imagePoint: { x: 664.0, y: 424.0 }, modelPoint: { x:  602.3622, y: -360.2362    ,z: 0.  } },
  //  ]

  // -180, 180, 0
  // -5, 354, 0

// c++
   const cpp = [{"imagePoint": {"x": 979.0, "y": 625.0}, "modelPoint": {"x": -179.1338653564453, "y": 180.11810302734375, "z": 0.0}}, {"imagePoint": {"x": 1136.0, "y": 801.0}, "modelPoint": {"x": -255.744384765625, "y": 257.1639099121094, "z": 0.0}}, {"imagePoint": {"x": 584.0, "y": 538.0}, "modelPoint": {"x": 0.0, "y": 180.11810302734375, "z": 0.0}}, {"imagePoint": {"x": 872.0, "y": 502.0}, "modelPoint": {"x": -5.905511856079102, "y": 5.905511856079102, "z": 0.0}}, {"imagePoint": {"x": 464.0, "y": 462.0}, "modelPoint": {"x": 336.6141662597656, "y": 43.30708694458008, "z": 0.0}}, {"imagePoint": {"x": 514.0, "y": 451.0}, "modelPoint": {"x": 336.6141662597656, "y": 5.905511856079102, "z": 0.0}}, {"imagePoint": {"x": 314.0, "y": 814.0}, "modelPoint": {"x": -179.1338653564453, "y": 360.2362060546875, "z": 0.0}}, {"imagePoint": {"x": 1249.0, "y": 543.0}, "modelPoint": {"x": -179.1338653564453, "y": 0.0, "z": 0.0}}, {"imagePoint": {"x": 109.0, "y": 607.0}, "modelPoint": {"x": -5.905511856079102, "y": 354.3307189941406, "z": 0.0}}, {"imagePoint": {"x": 328.0, "y": 429.0}, "modelPoint": {"x": 713.779541015625, "y": -5.905511856079102, "z": 0.0}}, {"imagePoint": {"x": 664.0, "y": 424.0}, "modelPoint": {"x": 602.3621826171875, "y": -360.2362060546875, "z": 0.0}}];
   const js = [{"imagePoint":{"x":862.2148760330579,"y":364.1652892561985},"modelPoint":{"x":-0.15000000114440917,"y":3.330669099286458e-17,"z":-0.15000000114440917}},{"imagePoint":{"x":464.1322314049587,"y":407.0082644628101},"modelPoint":{"x":8.549999822998046,"y":0.0600000016689303,"z":-1.100000008392334}},{"imagePoint":{"x":465.91735537190084,"y":530.1818181818182},"modelPoint":{"x":8.549999822998046,"y":2.450000067138672,"z":-1.1000000083923336}},{"imagePoint":{"x":596.2314049586777,"y":449.8512396694216},"modelPoint":{"x":18.150000671386717,"y":-1.9984015026011486e-15,"z":9.000000262451172}},{"imagePoint":{"x":57.12396694214874,"y":251.70247933884303},"modelPoint":{"x":-0.15000000114440917,"y":2.0650147770136932e-15,"z":-9.299999780273437}},{"imagePoint":{"x":978.4013429752065,"y":241.8285123966943},"modelPoint":{"x":-4.55000018005371,"y":1.0158540268744366e-15,"z":-4.574999816894531}}];
 // jonas yml
  //  const d = [{"imagePoint": {"x": 982.0, "y": 624.0}, "modelPoint": {"x": -179.1338653564453, "y": 180.11810302734375, "z": 0.0}}, {"imagePoint": {"x": 2.0, "y": 322.0}, "modelPoint": {"x": 349.4094543457031, "y": 329.7243957519531, "z": 96.45669555664062}}, {"imagePoint": {"x": 466.0, "y": 328.0}, "modelPoint": {"x": 361.220458984375, "y": 30.511810302734375, "z": 96.45669555664062}}, {"imagePoint": {"x": 869.0, "y": 500.0}, "modelPoint": {"x": -5.905511856079102, "y": 5.905511856079102, "z": 0.0}}, {"imagePoint": {"x": 470.0, "y": 458.0}, "modelPoint": {"x": 336.6141662597656, "y": 43.30708694458008, "z": 2.3622047901153564}}, {"imagePoint": {"x": 597.0, "y": 416.0}, "modelPoint": {"x": 714.5669555664062, "y": -354.3307189941406, "z": 0.0}}];

  for(const row of cpp){
    row.imagePoint.y = 864 - row.imagePoint.y;
  }

    setTimeout(
      () =>
        // this.importJsonData(),
        this.importJsonData(cpp),
      1000
    );
  }

  public render() {
    this.modelView.render();
    this.imageView.render();
    requestAnimationFrame(() => this.render());
  }

  exportJsonData() {
    const d = [];
    for (const r of this.relationData) {
      d.push({
        imagePoint: r.imagePoint,
        modelPoint: r.modelPoint,
      });
    }

    console.log(JSON.stringify(d));
  }

  async importJsonData(data: any[]) {
    this.relationData = [];

    for (const o of data) {
      const p = new RelationDataElement(o.modelPoint);
      this.relationData.push(p);

      p.modelMesh = this.modelView.createSelectionMesh(o.modelPoint);

      p.imageMesh = this.imageView.createSelectionMesh(
        new Vector2(o.imagePoint.x, o.imagePoint.y)
      );
    }

    // await cv.waitForLoad();
    this.calibrate();

    // const { matrix, cameraMatrix } = cv.calibrateCamera(
    //   this.relationData,
    //   new Vector2(this.imageView.imageWidth, this.imageView.imageHeight)
    // );
    // this.updateCalibrateCameraResult(matrix, cameraMatrix);
  }

  private async calibrate() {
    await cv.waitForLoad();

    const { matrix, cameraMatrix } = cv.calibrateCamera(
      this.relationData,
      new Vector2(this.imageView.imageWidth, this.imageView.imageHeight)
    );
    // this.updateCalibrateCameraResult(matrix, cameraMatrix);

    this.imageView.setCalibratedMatrix(matrix, cameraMatrix);
  }
}

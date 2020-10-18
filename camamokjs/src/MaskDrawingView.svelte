<script lang="ts">
  import { onMount } from "svelte";
  import * as PIXI from "pixi.js";
  import { Viewport } from "pixi-viewport";
  import { onDestroy } from "svelte/internal";

  export let src: string = "";
  export let saveEndpoint: string;
  export let jsonPath: string;

  let app: PIXI.Application;
  let appOffscreen: PIXI.Application;
  let container: PIXI.Container;
  let polygonGraphics: PIXI.Graphics;
  let polygonGraphicsOffscreen: PIXI.Graphics;
  let lineGraphics: PIXI.Graphics;
  let viewport: Viewport;
  let width;
  let height;
  let errorMsg = "";
  let curMouseWorldPos: PIXI.Point;

  onMount(() => {
    var elem = document.getElementById("canvas-container");

    app = new PIXI.Application({
      width: 600,
      height: 600,
      // forceCanvas: true,
      transparent: true,
      // autoResize: true,
    });

    appOffscreen = new PIXI.Application({
      width: 600,
      height: 600,
      forceCanvas: true,
    });

    elem.appendChild(app.view);

    // Listen for window resize events
    window.addEventListener("resize", resize);

    const parent = document.getElementById("container");

    viewport = new Viewport({
      screenWidth: parent.clientWidth,
      screenHeight: parent.clientHeight,
      worldWidth: 1000,
      worldHeight: 1000,

      interaction: app.renderer.plugins.interaction, // the interaction module is important for wheel to work properly when renderer.view is placed or scaled
    });

    app.stage.addChild(viewport);

    // activate plugins
    viewport
      .drag({
        mouseButtons: "right",
      })
      .pinch()
      .wheel();

    container = new PIXI.Container();
    viewport.addChild(container);

    polygonGraphics = new PIXI.Graphics();
    polygonGraphicsOffscreen = new PIXI.Graphics();
    appOffscreen.stage.addChild(polygonGraphicsOffscreen);
    polygonGraphics.blendMode = PIXI.BLEND_MODES.ADD;

    viewport.addChild(polygonGraphics);

    lineGraphics = new PIXI.Graphics();
    viewport.addChild(lineGraphics);

    viewport.on("clicked", click);

    resize();

    window.addEventListener("wheel", onwheel, { passive: false });
    window.addEventListener("contextmenu", oncontextmenu, false);
    document.addEventListener("keydown", onkeydown);
    app.view.addEventListener("mousemove", onmousemove);
  });

  onDestroy(() => {
    window.removeEventListener("contextmenu", oncontextmenu);
    window.removeEventListener("wheel", onwheel);
    document.removeEventListener("keydown", onkeydown);
    app.view.removeEventListener("mousemove", onmousemove);
  });

  let onmousemove = (evt) => {
    const worldPoint = viewport.toWorld(evt.offsetX, evt.offsetY);
    curMouseWorldPos = worldPoint;
    if (curPolygon) {
      curPolygon.points[curPolygon.points.length - 2] = worldPoint.x;
      curPolygon.points[curPolygon.points.length - 1] = worldPoint.y;
    } 
    updateGraphics();

  };
  let oncontextmenu = (evt) => evt.preventDefault();

  let onwheel = (evt) => evt.preventDefault();

  let onkeydown = (ev) => {
    if (
      (window.navigator.platform.match("Mac") ? ev.metaKey : ev.ctrlKey) &&
      ev.key == "z"
    ) {
      ev.preventDefault();
      if (curPolygon) {
        if (curPolygon.points.length > 2) {
          curPolygon.points.pop();
          curPolygon.points.pop();
        }
      } else if (polygons.length > 0) {
        polygons.pop();
      }
      updateGraphics();
    }

    if (!curPolygon && (ev.key == "Backspace" || ev.key == "Delete")) {
      for(const p of polygons){
        if(curMouseWorldPos && p.contains(curMouseWorldPos.x, curMouseWorldPos.y)){
          polygons.splice(polygons.indexOf(p), 1);
          updateGraphics();
        }
      } 
    }

    if (ev.key == "Backspace" || ev.key == "Delete" || ev.key == "Escape") {
      if (curPolygon) {
        polygons.pop();
        curPolygon = undefined;
        updateGraphics();
      } 
    }

    if (
      (window.navigator.platform.match("Mac") ? ev.metaKey : ev.ctrlKey) &&
      ev.key == "s"
    ) {
      ev.preventDefault();
      save();
    }
  };

  export function reset() {
    console.log("Reset")
    polygons = [];
    updateGraphics();
  }

  export async function save() {
    const texture = appOffscreen.renderer.generateTexture(
      polygonGraphicsOffscreen,
      PIXI.SCALE_MODES.LINEAR,
      1,
      new PIXI.Rectangle(0, 0, width, height)
    );
    const extract = new PIXI.Extract(appOffscreen.renderer);
    const img = extract.base64(texture, "image/jpeg");

    const b64toBlob = (base64, type = "image/jpeg") =>
      fetch(`${base64}`).then((res) => res.blob());

    const polygonData = polygons.map((p) => p.points);

    const formData = new FormData();
    formData.append("json", JSON.stringify(polygonData));
    formData.append("image", new Blob([await b64toBlob(img)]));
    await fetch(saveEndpoint, {
      method: "POST",
      body: formData,
    });
  }

  // Resize function window
  function resize() {
    const parent = document.getElementById("container");
    app.renderer.resize(parent.clientWidth, parent.clientHeight);
    viewport.resize(parent.clientWidth, parent.clientHeight);
  }

  $: {
    if (container) {
      container.removeChildren();

      let loader = new PIXI.Loader();
      loader.add(src);

      loader.onComplete.add(() => {
        const texture = loader.resources[src].texture;
        width = texture.width;
        height = texture.height;
        errorMsg = ``
        if(texture.width == 1){
          errorMsg = `Could not load image at ${src}`
        }
        

        const img = PIXI.Sprite.from(texture);
        container.addChild(img);

        const parent = document.getElementById("container");
        const scale = height / parent.clientHeight;
        viewport.setZoom(1 / scale);

        // appOffscreen.stage.width = width
        // appOffscreen.stage.height = height

        updateGraphics();
      });
      loader.load();
    }
  }

  $: {
    if (jsonPath) {
      loadMaskFromJson(jsonPath);
    }
  }

  async function loadMaskFromJson(jsonPath) {
    return fetch(jsonPath)
      .then((d) => d.json())
      .then((d: number[][]) => {
        polygons = [];
        for (const p of d) {
          polygons.push(new PIXI.Polygon(p));
        }
        updateGraphics();
      })
      .catch((e) => {
        console.error(e)
        reset();
      });
  }

  let polygons: PIXI.Polygon[] = [];
  let curPolygon: undefined | PIXI.Polygon = undefined;

  let lastClick: number;
  function click(data) {
    const t = Date.now() / 1000;
    if (Math.abs(lastClick - t) < 0.25) {
      finishDrawing();
      return;
    }
    lastClick = t;

    if (!curPolygon) {
      const polygon = new PIXI.Polygon();
      polygons.push(polygon);
      // polygonsContainer.addChild(polygon);
      curPolygon = polygon;
      curPolygon.points.push(data.world.x);
      curPolygon.points.push(data.world.y);
    } else if (curPolygon.points.length > 0) {
      const dx = Math.abs(curPolygon.points[0] - data.world.x);
      const dy = Math.abs(curPolygon.points[1] - data.world.y);
      let dist = Math.sqrt(dx * dx + dy * dy);

      if (dist < 10) {
        finishDrawing();
        return;
      }
    }

    curPolygon.points.push(data.world.x);
    curPolygon.points.push(data.world.y);

    updateGraphics();
  }

  function finishDrawing() {
    // console.log("finish")
    curPolygon.points.push(curPolygon.points[0]);
    curPolygon.points.push(curPolygon.points[1]);
    curPolygon = undefined;
    updateGraphics();
  }

  function updateGraphics() {
    polygonGraphics.clear();
    // polygonGraphics.beginFill(0xffffff);
    // polygonGraphics.drawRect(0, 0, width, height);
    polygonGraphicsOffscreen.clear();
    polygonGraphicsOffscreen.beginFill(0xffffff);
    polygonGraphicsOffscreen.drawRect(0, 0, width, height);
    lineGraphics.clear();

    polygonGraphics.beginFill(0xaa0000);
    polygonGraphics.alpha = 0.8;
    polygonGraphicsOffscreen.beginFill(0x000000);
    for (const p of polygons) {
      polygonGraphics.drawPolygon(p);
      polygonGraphicsOffscreen.drawPolygon(p);
    }
    polygonGraphicsOffscreen.endFill();
    polygonGraphics.endFill();

    
    for (const p of polygons) {
      let hovering = false;
      if(curMouseWorldPos && p.contains(curMouseWorldPos.x, curMouseWorldPos.y)){
        hovering = true; 
      }
      if (p == curPolygon || hovering) {
        lineGraphics.lineStyle(4, 0xff0000);
      } else {
        lineGraphics.lineStyle(0, 0x333333);
      }

      lineGraphics.moveTo(p.points[0], p.points[1]);
      for (let i = 2; i < p.points.length; i += 2) {
        lineGraphics.lineTo(p.points[i], p.points[i + 1]);
      }
    }
  }
</script>

<style>
  #container {
    height: 100%;
    width: 100%;
  }

  #canvas-container {
    position: absolute;
    left: 0;
    top: 0;
  }
  #error {
    position: absolute;
    top: 40px;
    left: 40px;
    color: white;
  }
</style>

<div id="container">
  <!-- <img  id="image" {src}> -->
  <div id="canvas-container" />
  <!-- <canvas></canvas> -->
  <div id="error">{errorMsg}</div>
</div>

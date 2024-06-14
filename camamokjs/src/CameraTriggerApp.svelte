<script lang="ts">
  import { get } from "svelte/store";
  import { writable } from "svelte-local-storage-store";

  import { onDestroy, onMount } from "svelte";
  import * as ccapi from "./ccapi";
  import moment from "moment";
  import * as ExifReader from "exifreader";
  import { CompressedTextureLoader } from "three";

  let connected = false;
  let connecting = true;

  let proCamSampleConnected = false;
  let curPattern;
  let numPatterns;
  let patternName;
  let batteryStatus: ccapi.DeviceStatusBatteryResponse;
  let lensStatus: ccapi.DeviceStatusLensResponse;
  let previewSrc = "";
  let numScheduledTransfer = 0;

  let addedContentPromise = [];

  let runInfo = "";

  let deviceInfo: ccapi.DeviceInformationResponse;

  const quality = "medium_fine";
  // const continuousSpeed = "cont_super_hi";
  const continuousSpeed = "highspeed";

  export const preferences = writable("camtriggerpreferences_v3", {
    cameraUrl: "http://192.168.1.2:8080",
    proCamScanUrl: "http://host.docker.internal:8000", // should be host.docker.internal if running inside docker
    shutterSpeed: 0,
    captureDuration: 0,
  });

  function sleep(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
  }

  function Interval(fn, duration, ...args) {
    const _this = this;
    _this.stopping = false;
    this.baseline = undefined;

    this.run = function (flag) {
      if (_this.stopping) return;
      if (_this.baseline === undefined) {
        _this.baseline = new Date().getTime() - duration;
      }
      if (flag) {
        fn(...args);
      }
      const end = new Date().getTime();
      _this.baseline += duration;

      let nextTick = duration - (end - _this.baseline);
      if (nextTick < 0) {
        nextTick = 0;
      }

      console.log(nextTick);
      _this.timer = setTimeout(function () {
        _this.run(true);
      }, nextTick);
    };

    this.stop = function () {
      console.log("Stop ", _this.timer);
      clearTimeout(_this.timer);
      _this.stopping = true;
    };
  }

  async function connectCamera() {
    await ccapi
      .connectCamera(get(preferences).cameraUrl)
      .then(() => {
        connected = true;
        connecting = false;
      })
      .catch((err) => {
        connecting = false;
        console.error(err);
      });

    // Turn of the display
    // await ccapi.shootingLiveView("small", "off");

    if (connected) {
      ccapi.getDeviceInformation().then((res) => (deviceInfo = res));
      ccapi.getDeviceStatusBattery().then((res) => (batteryStatus = res));
      ccapi.getDeviceStatusLens().then((res) => (lensStatus = res));
    }
  }

  async function takePicture() {
    let retry = 10;

    while (retry-- > 0) {
      const response = await ccapi.shootingControlShutterButton(false);
      if (response.message) {
        console.warn("Problem taking photo", response.message);
        await sleep(1000);
        console.log("Retrying ", retry);
      } else {
        break;
      }
    }
    if (retry <= 0) {
      throw new Error("Could not take photo");
    }
    console.log("Wait for photo name");

    console.log(addedContentPromise);
    if (addedContentPromise.length == 0) {
      poll();
    }
    const path = await new Promise((res, rej) => {
      addedContentPromise.push(res);
    });
    return path;
  }

  async function takePreview() {
    const path = await takePicture();
    setTimeout(() => {
      previewSrc = path;
    }, 500);
  }

  async function proCamScapApi(method: string) {
    return await fetch(
      `/proxy?url=${get(preferences).proCamScanUrl}${method}`
    ).then((res) => res.text());
  }

  async function connectProCamSample() {
    await getPattern();
    proCamSampleConnected = true;
  }

  async function start() {
    await ccapi.shootingLiveView("small", "off");
    await ccapi.shootingSettingsStillImageQuality(quality);

    // await resetPattern();
    const scanName = `scan-${moment().format("MMDDTHH-mm-ss")}`;
    runInfo = `Capturing ${scanName}`;

    const paths = [];

    console.time("Scan");

    // for (let i = 0; i < 3; i++) {
    for (let i = 0; i < numPatterns; i++) {
      console.log("Set pattern", i);
      await setPattern(i);

      // Take photo
      console.log("Take photo");
      await takePicture().then((path) => {
        console.log("Photo taken:", path);
        paths.push([path, `${scanName}/cameraImages/${patternName}.jpg`]);
      });

      // await sleep(900);
      // List photo for download
    }
    // await Promise.all(promises);

    await setColor(0, 1, 0);
    await sleep(2000);
    await setPattern(0);
    await ccapi.shootingLiveView("medium", "on");

    // Download photos
    let downloaded = 1;

    runInfo = `Downloading ${downloaded}/${paths.length}`;

    console.log("Scan files:", paths);

    for (let [url, path] of paths) {
      console.log("Download ", url, "to", path);
      await fetch(`/downloadImageFromUrl?url=${url}&filename=${path}`)
        // await fetch(`/scheduleDownloadImageFromSD?url=${url}&filename=${path}`)
        .then(() => {
          runInfo = `Downloading ${++downloaded}/${paths.length}`;
        })
        .then(() => (previewSrc = `/SharedData/${path}`));
    }

    console.log("Done");
    runInfo = `Finished ${scanName}`;
    console.timeEnd("Scan");

    var msg = new SpeechSynthesisUtterance("Scan complete");
    window.speechSynthesis.speak(msg);

    // setTimeout(()=> runInfo = '', 3000);
  }

  // Experimental code that isnt finished:
  async function startHighspeed() {
    const settings = await ccapi.getShootingSettings();
    const tv = ccapi.parseTime(settings.tv.value);
    if (tv !== get(preferences).shutterSpeed) {
      alert("Shutter speed not meassured, click Test Duration");
      return;
    }

    const scanName = `scan-${moment().format("MMDDTHH-mm-ss")}`;
    runInfo = `Capturing ${scanName} in highspeed`;

    // Prepare
    await ccapi.shootingSettingsStillImageQuality(quality);
    await ccapi.shootingSettingsDrive(continuousSpeed);
    await ccapi.polling(false);

    // Lets go 🤞 hope the timing matches
    setColor(1, 0, 0);

    await ccapi.shootingControlShutterButtonManual("full_press", false);

    const intervalDuration = get(preferences).captureDuration;
    let i = 0;
    var s = +new Date();

    // const interval = setInterval(async () => {
    const interval = new Interval(async () => {
      // console.log(
      //   (new Date() - s) % intervalDuration,
      //   i - 1,
      //   Math.round((new Date() - s) / intervalDuration)
      // );

      // const _numPatterns = numPatterns;
      const _numPatterns = 10;
      runInfo = `${i}/${_numPatterns * 2 + 4}`;
      if (i == 0) {
        setColor(1, 0, 0);
      } else if (i == 1 || i == 2) {
        setColor(0, 1, 0);
      } else if (i == 3) {
        setColor(1, 0, 0);
      } else if (i < (_numPatterns + 1) * 2 + 4) {
        // } else if(i<10*2+4){
        setPattern(Math.floor((i - 4) / 2));
        console.log(i, Math.floor((i - 4) / 2));
      } else if (i < (_numPatterns + 1) * 2 + 5) {
        setColor(255, 0, 0);
      } else if (i < (_numPatterns + 1) * 2 + 7) {
        setColor(0, 255, 0);
      } else if (i < (_numPatterns + 1) * 2 + 8) {
        setColor(255, 0, 0);
      } else {
        interval.stop();

        console.log("Done");
        await ccapi.shootingControlShutterButtonManual("release", false);
        await sleep(5000);
        await ccapi.shootingSettingsDrive("single");

        const pollResponse = await ccapi.polling(false);
        const numPhotos = pollResponse.addedcontents.length;
        console.log("Took " + numPhotos);

        // Download first 4 images and last 4 images
        runInfo = `Downloading first and last photos`;
        let paths = [];
        for (let u = 0; u < 4; u++) {
          const l = pollResponse.addedcontents.length;
          paths.push([
            pollResponse.addedcontents[u],
            `${scanName}/checkImages/first${u}.jpg`,
          ]);
          paths.push([
            pollResponse.addedcontents[l - u - 1],
            `${scanName}/checkImages/last${u}.jpg`,
          ]);
        }

        for (let [url, path] of paths) {
          console.log("Download ", url, "to", path);
          await fetch(
            `/scheduleDownloadImageFromSD?url=${url}&filename=${path}`
          ).then(() => (previewSrc = `/SharedData/${path}`));
        }
      }
      i++;
    }, intervalDuration);

    interval.run();
  }

  async function testDuration() {
    const settings = await ccapi.getShootingSettings();
    const tv = ccapi.parseTime(settings.tv.value);
    const duration = Math.max(10000, tv * 3);
    await ccapi.polling(false);

    console.log("Shutter speed: ", tv);

    await ccapi.shootingSettingsStillImageQuality("small2");
    await ccapi.shootingSettingsDrive(continuousSpeed);
    await sleep(1000);

    console.log("Press");
    await ccapi.shootingControlShutterButtonManual("full_press", false);

    await sleep(duration);

    console.log("Release");
    await ccapi.shootingControlShutterButtonManual("release", false);

    await sleep(1000);
    await ccapi.shootingSettingsStillImageQuality(quality);
    await ccapi.shootingSettingsDrive("single");

    await sleep(2000);
    const pollResponse = await ccapi.polling(false);
    const numPhotos = pollResponse.addedcontents.length;
    console.log(
      "Took " +
        numPhotos +
        " photos over " +
        duration +
        "ms with shutter speed of " +
        tv +
        "ms"
    );

    console.log("Downloading exif of first and last image");
    const timing1 = await fetch(
      `/getExifFromUrl?url=${pollResponse.addedcontents[0]}&filename=first.jpg`
    )
      .then((res) => res.json())
      .then(
        (res) => res.DateTime.description + "." + res.SubSecTime.description
      );

    const timing2 = await fetch(
      `/getExifFromUrl?url=${
        pollResponse.addedcontents[numPhotos - 1]
      }&filename=last.jpg`
    )
      .then((res) => res.json())
      .then(
        (res) => res.DateTime.description + "." + res.SubSecTime.description
      );

    console.log(timing1, timing2);

    const diff = moment(timing2, "YYYY:MM:DD hh:mm:ss.SS").diff(
      moment(timing1, "YYYY:MM:DD hh:mm:ss.SS")
    );
    console.log("Total diff is " + diff + "ms");
    console.log("Diff per photo is " + diff / (numPhotos - 1) + "ms");
    preferences.update((pref) => {
      return {
        ...pref,
        shutterSpeed: tv,
        captureDuration: diff / (numPhotos - 1),
      };
    });
  }

  async function getPattern() {
    const curPatternStr = await proCamScapApi("/actions/currentPattern");
    [curPattern, numPatterns] = curPatternStr
      .split("/")
      .map((s) => parseInt(s));
  }

  async function setPattern(pattern: number) {
    if (pattern >= numPatterns) {
      pattern = 0;
    }
    if (pattern < 0) {
      pattern = numPatterns - 1;
    }
    patternName = await proCamScapApi("/actions/pattern/" + pattern);
    await getPattern();
  }

  async function setColor(r: number, g: number, b: number) {
    await proCamScapApi("/actions/color/" + [r, g, b].join(","));
  }

  async function pollScheduledTransfer() {
    const scheduled = await fetch(
      "/SharedData/_scheduledDownload.json"
    ).then((res) => res.json());
    numScheduledTransfer = scheduled.length;
  }

  setInterval(pollScheduledTransfer, 5000);

  async function poll() {
    console.log("Poll");
    if (connected) {
      const pollResponse = await ccapi.polling(false).catch((e) => {
        console.error(e);
      });

      if (
        pollResponse &&
        pollResponse.addedcontents &&
        addedContentPromise.length > 0
      ) {
        console.log(pollResponse.addedcontents);
        pollResponse.addedcontents.forEach((path) => {
          if (addedContentPromise.length > 0) {
            addedContentPromise.shift()(path);
          }
        });
        // addedContentPromise(pollResponse.addedcontents);
        // addedContentPromise = undefined;
      }

      if (addedContentPromise.length > 0) {
        setTimeout(() => poll(), 100);
      }
      return pollResponse;
    }
  }

  onMount(async () => {
    connectCamera();
    connectProCamSample();
  });

  onDestroy(() => {
    connected = false;
  });
</script>

<div class="panel">
  <div class="panel-row">
    <div class="box" id="device-info">
      <div>
        <h3>Camera</h3>
      </div>
      <div>
        <input
          type="text"
          bind:value={$preferences.cameraUrl}
          style="width: 300px"
        />
      </div>
      <div>
        <b>Status:</b>
        {connected ? 'Connected' : connecting ? 'Connecting...' : 'Not connected'}
      </div>
      {#if connected}
        <div><b>Camera:</b> {deviceInfo?.productname}</div>

        <div><b>Battery:</b> {batteryStatus?.level}</div>

        <div><b>Lens:</b> {lensStatus?.name}</div>

        <div><button on:click={takePreview}>Take Preview</button></div>
        <div>
          <b>Display:</b>
          <button on:click={() => ccapi.shootingLiveView('small', 'off')}>Turn
            off display</button>
          <button on:click={() => ccapi.shootingLiveView('medium', 'on')}>Turn
            on display</button>
        </div>
      {/if}
    </div>

    <div class="box" id="pattern-info">
      <div>
        <h3>Computer</h3>
      </div>
      <div>
        <input
          type="text"
          bind:value={$preferences.proCamScanUrl}
          style="width: 300px"
        />
      </div>
      <div>
        <b>Status:</b>
        {proCamSampleConnected ? 'Connected' : 'Not connected'}
      </div>
      {#if proCamSampleConnected}
        <div><b>Pattern:</b> {curPattern} / {numPatterns} ({patternName})</div>
        <div>
          <button on:click={() => setPattern(curPattern + 1)}>+</button>
          <button on:click={() => setPattern(curPattern - 1)}>-</button>
        </div>
        <button on:click={() => setColor(1.0, 0, 0)}>Red</button>
        <button on:click={() => setColor(0, 0, 1.0)}>Blue</button>
      {/if}
    </div>

    <div class="box">
      {#if proCamSampleConnected}
        <div>
          <button on:click={() => start()}>Start Calibration</button>
          {runInfo}
          {numScheduledTransfer} images need to be transfered
        </div>

        <div>Experimental stuff</div>
        <div>
          <button on:click={() => startHighspeed()}>Start bulk (experimental)</button>
          {runInfo}
        </div>
        <div>
          <button on:click={() => testDuration()}>Test Duration!</button>
          {#if $preferences.shutterSpeed}
            {$preferences.shutterSpeed}ms + {$preferences.captureDuration - $preferences.shutterSpeed}ms
          {/if}
        </div>
      {/if}
    </div>
  </div>
  <div class="panel-row">
    <div class="box">
      <img style="max-width: 100%" src={previewSrc} alt="preview" />
    </div>
  </div>
</div>

<style>
  .panel-row {
    display: flex;
    align-items: stretch;
    flex-direction: row;
    overflow: hidden;
  }

  .panel {
    position: relative;
  }

  .box {
    margin: 20px;
  }
  .box div {
    padding: 5px 10px;
  }
</style>

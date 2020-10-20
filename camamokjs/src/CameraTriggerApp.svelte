<script lang="ts">
  import { get } from "svelte/store";
  import { writable } from "svelte-local-storage-store";

  import { onDestroy, onMount } from "svelte";
  import * as ccapi from "./ccapi";
  import moment from "moment";

  let connected = false;
  let connecting = true;

  let proCamSampleConnected = false;
  let curPattern;
  let numPatterns;
  let patternName;
  let batteryStatus: ccapi.DeviceStatusBatteryResponse;
  let lensStatus: ccapi.DeviceStatusLensResponse;
  let previewSrc = "";

  let addedContentPromise = [];

  let runInfo = "";

  let deviceInfo: ccapi.DeviceInformationResponse;

  export const preferences = writable("camtriggerpreferences_v3", {
    cameraUrl: "http://192.168.1.2:8080",
    proCamScanUrl: "http://lightleaks.local:8000",
  });

  function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
  }

  async function connectCamera() {
    await ccapi.connectCamera(get(preferences).cameraUrl).catch((err) => {
      connecting = false;
      console.error(err);
    });
    connected = true;
    connecting = false;

    // Turn of the display
    // await ccapi.shootingLiveView("small", "off");

    if (connected) {
      ccapi.getDeviceInformation().then((res) => (deviceInfo = res));
      ccapi.getDeviceStatusBattery().then((res) => (batteryStatus = res));
      ccapi.getDeviceStatusLens().then((res) => (lensStatus = res));
    }
  }

  async function takePicture() {
    let retry =  3;

    while(retry-- > 0){
      const response = await ccapi.shootingControlShutterButton(false);
      if(response.message){
        console.warn("Problem taking photo", response.message);
        await sleep(1000);
        console.log("Retrying ",retry)
      } else {
        break;
      }
    }
    if(retry <= 0){
      throw  new Error("Could not take photo");
    }
    console.log("Wait for photo name");

    console.log(addedContentPromise)
    if(addedContentPromise.length == 0){
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
      await takePicture().then( path =>{
        console.log("Photo taken:", path);
        paths.push([path, `${scanName}/cameraImages/${patternName}.jpg`]);
      })

      // await sleep(900);
      // List photo for download
    }
    // await Promise.all(promises);
    await setPattern(0);
    await ccapi.shootingLiveView("medium", "on");
    
    // Download photos
    let downloaded = 1;
    
    runInfo = `Downloading ${downloaded}/${paths.length}`;
    for(let [url, path] of paths){
      console.log("Download ",url, 'to', path)
      await fetch(`/downloadImageFromUrl?url=${url}&filename=${path}`).then(() =>{
          runInfo = `Downloading ${++downloaded}/${paths.length}`;
      }).then(()=> previewSrc = `/SharedData/${path}`)
    }

    console.log("Done");
    runInfo = `Finished ${scanName}`;
    console.timeEnd("Scan")

    var msg = new SpeechSynthesisUtterance('Scan complete');
    window.speechSynthesis.speak(msg);
    
    // setTimeout(()=> runInfo = '', 3000);
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

  async function poll() {
    console.log("Poll");
    if (connected) {
      const pollResponse = await ccapi.polling(false).catch((e) => {
        console.error(e);
      });

      if (pollResponse && pollResponse.addedcontents && addedContentPromise.length > 0) {
        console.log(pollResponse.addedcontents)
        pollResponse.addedcontents.forEach( path => {
          if(addedContentPromise.length > 0){
            addedContentPromise.shift()(path)
          }
        })
        // addedContentPromise(pollResponse.addedcontents);
        // addedContentPromise = undefined;
      } 

      if(addedContentPromise.length > 0){
        setTimeout(() => poll(), 100);
      }
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
          style="width: 300px" />
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
          style="width: 300px" />
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
      {/if}
    </div>

    <div class="box">
      {#if proCamSampleConnected}
        <div><button on:click={() => start()}>Start!</button> {runInfo}</div>
      {/if}
    </div>
  </div>
  <div class="panel-row">
    <div class="box"><img style="max-width: 100%" src={previewSrc} /></div>
  </div>
</div>

<script lang="ts">
  import CameraMaskApp from "./CameraMaskApp.svelte";
  import CameraTriggerApp from "./CameraTriggerApp.svelte";
  import MappingApp from "./MappingApp.svelte";
  import ProjectorMaskApp from "./ProjectorMaskApp.svelte";

  import { get } from "svelte/store";
  import { writable } from "svelte-local-storage-store";

  let mappingApp;
  let cameraMaskApp;
  let projectoMaskApp;
  let cameraTriggerApp;

  export const preferences = writable("preferences", {
    mode: "camtrigger",
  });

  function reset() {
    const mode = get(preferences).mode;
    if (mode == "mapping") {
      mappingApp.reset();
    } else if (mode == "cammask") {
      cameraMaskApp.reset();
    } else if (mode == "projmask") {
      projectoMaskApp.reset();
    }
  }
  function save() {
    const mode = get(preferences).mode;
    if (mode == "mapping") {
      mappingApp.saveCalibration();
    } else if (mode == "cammask") {
      cameraMaskApp.save();
    } else if (mode == "projmask") {
      projectoMaskApp.save();
    }
  }
</script>

<style>
  #topbar {
    width: 100%;
    border-bottom: 1px solid gray;
    /* height: 72px; */
    box-sizing: content-box;
    display: flex;
    justify-content: space-between;
    flex-wrap: wrap;
  }

  main {
    display: flex;
    flex-direction: column;
    align-items: stretch;
    height: 100%;
    background-color: #202020;
    color: white;
  }

  #savebutton {
    margin-right: 20px;
    margin-top: 9px;
  }

  h1 {
    font-size: 20px;
    font-weight: 100;
    margin-left: 10px;
  }

  .topbar-tabs {
    display: flex;
    /* flex-basis: 100%; */
    width: 100%;
  }

  .topbar-tabs p {
    padding: 6px;
    margin: 0;
    cursor: pointer;
  }

  .topbar-tabs p.active {
    background-color: cadetblue;
  }
</style>

<main>
  <div id="topbar">
    <h1>Light Leaks | Camamok</h1>
    <div class="rightbuttons">
      <button id="resetbutton" on:click={() => reset()}>Reset</button>
      <button id="savebutton" on:click={() => save()}>Save</button>
    </div>
    <div class="topbar-tabs">
      <p
        class:active={$preferences.mode == 'camtrigger'}
        on:click={() => ($preferences.mode = 'camtrigger')}>
        Camera Shooting
      </p>
      <p class:active={$preferences.mode == 'cammask'} on:click={() => ($preferences.mode = 'cammask')}>
        Camera Mask
      </p>
      <p class:active={$preferences.mode == 'projmask'} on:click={() => ($preferences.mode = 'projmask')}>
        Projector Mask
      </p>
      <p class:active={$preferences.mode == 'mapping'} on:click={() => ($preferences.mode = 'mapping')}>
        Camera Mapping
      </p>
    </div>
  </div>

  {#if $preferences.mode == 'mapping'}
    <MappingApp bind:this={mappingApp} />
  {:else if $preferences.mode == 'cammask'}
    <CameraMaskApp bind:this={cameraMaskApp} />
  {:else if $preferences.mode == 'projmask'}
    <ProjectorMaskApp bind:this={projectoMaskApp} />
  {:else if $preferences.mode == 'camtrigger'}
    <CameraTriggerApp bind:this={cameraTriggerApp} />
  {/if}
</main>

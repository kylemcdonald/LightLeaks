<script lang="ts">
  import CameraMaskApp from "./CameraMaskApp.svelte";
import MappingApp from "./MappingApp.svelte";
import ProjectorMaskApp from "./ProjectorMaskApp.svelte";

  let mappingApp;
  let cameraMaskApp;
  let projectoMaskApp;

  let mode : 'mapping' | 'cammask' | 'projmask' = 'cammask';

  function reset(){
    if(mode == 'mapping'){
      mappingApp.reset();
    } else if (mode == 'cammask'){
      cameraMaskApp.reset();
    } else if (mode == 'projmask'){
      projectoMaskApp.reset();
    }

  }
   function save(){
    if(mode == 'mapping'){
      mappingApp.saveCalibration();
    } else if (mode == 'cammask'){
      cameraMaskApp.save();
    } else if (mode == 'projmask'){
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

  .topbar-tabs  p   {
    padding: 6px;
    margin:0;
    cursor: pointer;
  }

  .topbar-tabs  p.active {
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
      <p class:active={mode == 'cammask'} on:click={()=>mode = 'cammask'}>Camera Mask</p>
      <p class:active={mode == 'projmask'} on:click={()=>mode = 'projmask'}>Projector Mask</p>
      <p class:active={mode == 'mapping'} on:click={()=>mode = 'mapping'}>Camera Mapping</p>
    </div>
  </div>

  {#if mode == 'mapping'}
    <MappingApp bind:this={mappingApp}/>
  {:else if mode == 'cammask'}
    <CameraMaskApp bind:this={cameraMaskApp}></CameraMaskApp>
  {:else if mode == 'projmask'}
    <ProjectorMaskApp bind:this={projectoMaskApp}></ProjectorMaskApp>
  {/if}
</main>

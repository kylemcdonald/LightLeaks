<script lang="ts">
  import { onMount } from "svelte";
  import MaskDrawingView from "./MaskDrawingView.svelte";

  let scans: string[];
  let maskDrawingView: MaskDrawingView;
  let processedScans: string[] = [];

  onMount(async () => {
    scans = await fetch("/scans").then((res) => res.json());

    const promises = scans.map( scan => {
      return fetch(
          `/status/SharedData/${scan}/processedScan/proConfidence.jpg`
        ).then((res) => res.json());
    })

    const statuses = (await Promise.all(promises));
    for(let i=0; i<scans.length;i++){
      if(statuses[i].exists){
        processedScans.push(scans[i])
      }
    }
    processedScans = [...processedScans]
  });

  export function reset(){
    maskDrawingView.reset();
  }
  export function save(){
    maskDrawingView.save();
  }
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
</style>

<div class="panel-row" style="flex:1; ">
  <div class="panel" style="    overflow: scroll;  flex-shrink: 0;">
    <!-- <ScanList
      bind:this={scanListComponent}
      fileToCheckStatusOf="/processedScan/proConfidence.jpg"
      {scans}
      {loadedScan}
      on:loadscan={(ev) => loadScan(ev.detail)} /> -->
      Loaded scans: <br>
      {#each processedScans as s}
        {s}<br>
      {/each}
  </div>
  <div class="panel" style="flex:1">
    {#if processedScans.length > 0}
    <MaskDrawingView
      bind:this={maskDrawingView}
      jsonPath={`/SharedData/mask.json`}
      saveEndpoint={`/saveProjectorMask/`}
      src={processedScans.map( scan => `/SharedData/${scan}/processedScan/proConfidence.jpg`)} />
    {/if}
  </div>
</div>

<script lang="ts">
  import { onMount } from "svelte";
  import MaskDrawingView from "./MaskDrawingView.svelte";
  import ScanList from "./ScanList.svelte";

  let scanListComponent: ScanList;
  let scans: string[];
  let loadedScan: string;
  let maskDrawingView: MaskDrawingView;

  async function loadScan(name: string) {
    loadedScan = name;
    // await loadCalibration(name);
  }

  onMount(async () => {
    scans = await fetch("/scans").then((res) => res.json());
    scans = scans.filter( s => !s.startsWith('_'))
    loadScan(scans[0]);
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
    <ScanList
      bind:this={scanListComponent}
      fileToCheckStatusOf="/camamok/mask.json"
      {scans}
      {loadedScan}
      on:loadscan={(ev) => loadScan(ev.detail)} />
  </div>
  <div class="panel" style="flex:1">
    {#if loadedScan}
    <MaskDrawingView
      bind:this={maskDrawingView}
      jsonPath={`/SharedData/${loadedScan}/camamok/mask.json`}
      saveEndpoint={`/saveMask/${loadedScan}`}
      src={`/SharedData/${loadedScan}/cameraImages/vertical/inverse/10.jpg`} />
    {/if}
  </div>
</div>

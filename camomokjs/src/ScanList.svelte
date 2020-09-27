<script lang="ts">
import { createEventDispatcher, onMount } from "svelte";

  import type { Vector2, Vector3 } from "three";

  export let scans:string[];
  export let loadedScan: string;

  let scanStatus = {};

  $: scans && loadStatus();

  onMount(async ()=>{
  })

  export async function loadStatus(){
    if(!scans) return;
    for(const scan of scans){
      try {
        const scanJson = await fetch(`/SharedData/${scan}/camamok/camamok.json`).then(res=>res.json())
        scanStatus[scan] = true;
      } catch(e){
        scanStatus[scan] = false;
      }
    }
  }

  const dispatch = createEventDispatcher<{
    loadscan: string;
  }>();
  
</script>

<style>
  #view {
    font-size: 0.7em;
    padding: 10px;
  }

  #table-wrapper {
    display: block;
    overflow: scroll;
    
  }

  table {
    background-color: #3c3c3c;
    border-collapse: collapse;
  }

  td {
    padding: 2px;
    padding: 2px 7px;
  }

  .border {
    border-right: 1px solid #ccc;
  }

  td i {
    font-size: 12px;
  }

  td:last-child {
    padding-right: 20px;
  }

  a {
    color: white;
  }
  a:hover {
    color: red;
  }

  td:hover{
    background-color: #666;
    cursor: pointer;
  }
  .loaded {
    background-color: cadetblue;
  }
</style>

<div id="view">
  <div id="table-wrapper">
    <table>
      {#if scans}          
        {#each scans as scan}
          <tr on:click={()=>dispatch('loadscan',scan)} class:loaded={loadedScan==scan}>
            <td>{scan} {scanStatus[scan] == true ? '✔':''}</td>
          </tr>
        {/each}
      {/if}
    </table>
  </div>
</div>

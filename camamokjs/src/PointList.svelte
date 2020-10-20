<script lang="ts">
  import type { Vector2, Vector3 } from "three";

  export let objectPoints: Vector3[];
  export let imagePoints: Vector2[];

  export let highlightedIndex=-1;
  
</script>

<style>
  #view {
    font-size: 0.7em;
    padding: 10px;
  }

  #table-wrapper {
    display: block;
    overflow: scroll;
    width:300px;
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

  .highlighted{
    background-color: #666;
  }
</style>

<div id="view">
  <div id="table-wrapper">
    <table>
      <tr>
        <th class="border">#</th>
        <th class="border" colspan="3">object point</th>
        <th class="border" colspan="2">image point</th>
      </tr>
      {#each objectPoints as objectPoint, i}
        <tr 
        on:mouseover={()=>highlightedIndex=i}

        on:mouseleave={()=>highlightedIndex=-1}       
        
        class:highlighted={highlightedIndex == i}
        >
          <td class="border">{i}</td>
          <td>{objectPoint.x.toFixed(1)}</td>
          <td>{objectPoint.y.toFixed(1)}</td>
          <td class="border">{objectPoint.z.toFixed(1)}</td>
          <td>{imagePoints[i].x.toFixed(0)}</td>
          <td class="border">{imagePoints[i].y.toFixed(0)}</td>
          <td><a href="#" on:click={()=>{
            objectPoints.splice(highlightedIndex,1)
            imagePoints.splice(highlightedIndex,1)
            objectPoints = [...objectPoints]
            imagePoints = [...imagePoints]
            highlightedIndex = -1;
          }}><i class="material-icons">clear</i></a></td>
        </tr>
      {/each}
    </table>
  </div>
</div>

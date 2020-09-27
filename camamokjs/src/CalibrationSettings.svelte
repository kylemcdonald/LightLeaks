<script lang="ts">
  import type { Vector2 } from "three";

  export let calibrationFlags;
  export let errorValue;
  export let imageSize: Vector2;
  export let focalLength: number;
  export let principalPoint: Vector2;
  export let fov: Vector2;
  export let aspectRatio: number;
</script>

<style>
  #calibration-settings {
    font-size: 0.7em;
    padding: 10px;
  }

  .cols {
    display: flex;
    flex-direction: column;
  }
  .col {
    margin-right: 30px;
  }
</style>

<div id="calibration-settings">
  <div class="cols">
    <div class="col">
      <b>Calibration Flags</b>
      {#each Object.keys(calibrationFlags) as key}
        <div>
          <input
            type="checkbox"
            id={key}
            bind:checked={calibrationFlags[key]} />
          <label style="display: inline-block;" for={key}>{key}</label>
        </div>
      {/each}
    </div>
    <div class="col">
      <div><b>Calibration stats</b></div>
      <div>Image Size: {imageSize.width} x {imageSize.height}</div>
      <div>
        Error: {errorValue == -1 ? 'Not calibrated' : errorValue.toFixed(1)}
      </div>

      {#if errorValue != -1}
        <div>Focal Length: {focalLength.toFixed(1)}</div>
        <div>Field of View: h {fov.x.toFixed(1)}° - v {fov.y.toFixed(1)}°</div>
        <div>
          Principal Point: {principalPoint.x.toFixed(0)} x {principalPoint.y.toFixed(0)}
          ({((100 * principalPoint.x) / imageSize.width).toFixed(1)}% x {((100 * principalPoint.y) / imageSize.height).toFixed(1)}%)
        </div>
        <div>Pixel Aspect Ratio: {aspectRatio.toFixed(2)}</div>
      {/if}
    </div>
  </div>
</div>

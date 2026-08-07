# Every-rock audit of the calibration pipeline

A pass over every stage of the pipeline - capture app, patterns,
decoding, map building, and the manual calibration path - looking for
computer-vision problems and unexploited ideas. Items marked [fixed]
are already addressed in the new model-free solver
(`../src/autoCalibrateCore.py` + experiment runners); everything else
is an open improvement.

## Capture (0-ProCamSample)

1. **BUG - long-run gray codes exist but are never projected.**
   `codes/lrgc/` contains generated long-run gray code tables and the
   client app dutifully loads `codes/<codeType>/<bits>.png` as a
   texture - but `bin/data/shader.frag` never samples that texture; it
   computes plain binary-reflected gray code analytically
   (`grayCode(x)` in the shader). The `codeType` setting is dead.
   LRGC bounds the minimum stripe run length, which directly reduces
   decode errors from projector/camera defocus - the experiment was
   set up years ago and silently never ran. Fix: sample the code
   texture in the shader (one line) or delete the illusion.

2. **Finest stripe levels are captured but are physically hopeless.**
   The app projects every level down to 1-2px stripes
   (`ceil(log2(w))` levels). At typical projector defocus + camera
   resample + JPEG, the last 1-2 levels decode as noise yet the
   decoder trusts them as LSBs. Skipping them saves 8 photos per scan
   AND removes the dominant per-pixel code noise; sub-pixel
   interpolation (below) more than recovers the resolution.

3. **Pattern-settle margin is a single `bufferTime = 100ms`.**
   Projectors with internal frame delay (scalers, warping units - and
   we know warped projectors were used at TodaysArt) can exceed this,
   mixing adjacent patterns into one exposure. Cheap insurance: raise
   to 300ms+, or verify once per install by capturing a
   known-alternating pattern.

4. **Camera settings recorded in settings.json: ISO 5000,
   in-camera noise reduction ON.** Sensor noise lands directly on
   fine-bit margins and NR smears stripe edges. Static scene + tripod
   => ISO 100-400, longer exposure, NR off, RAW or max-quality JPEG
   (archived JPEGs show visible block quantization).

5. **Single exposure per pattern.** Ball glints saturate while
   far-wall leak dots sit near the noise floor in the same frame. The
   360-degree content (dots all around the room) is exactly what is
   underexposed. Bracketing 2 exposures and merging before decode is
   the single biggest coverage lever for a new capture.

6. **`// check this isn't off-by-one` in shader.frag** - the y-flip
   has never been verified. Worth a one-time test pattern check;
   a half-pixel bias here shifts every decoded y coordinate.

## Decoding (python/src/processScansCommand.py)

7. **One global confidence per camera pixel** (mean over ALL patterns,
   line 203, with magic constants /5). A pixel that cleanly decodes 9
   of 10 bits but flips one is indistinguishable from a solid decode.
   Per-bit margins `|normal - inverse|` enable both honest confidence
   (min margin) and repair: coarse bits localize a pixel to a stripe
   band, neighborhood consistency resolves ambiguous fine bits.

8. **Fixed 301px high-pass hurts the coarsest bits.** Stripes wider
   than the kernel lose their own signal on large smooth surfaces -
   the most important bits weakened where walls are most decodable.
   `normal - inverse` already cancels static ambient; the high-pass
   only helps with scatter that changes between the pair. Make the
   filter radius proportional to stripe width, or decode each bit
   both ways and keep the higher-margin result.

9. **No sub-pixel refinement.** Decoded coordinates are integers; the
   difference image crosses zero sub-pixel at each stripe boundary.
   Interpolating the zero-crossing gives ~0.1px projector coords -
   a direct multiplier on triangulation accuracy, free.

10. **No code-space cleanup.** `packed_h/packed_v` should be locally
    smooth except at depth edges; an edge-preserving median repairs
    single-bit errors before they become confidently-wrong pixels.

11. **`imread().mean(axis=2)` for gray conversion.** Equal-weight RGB
    doubles down on the noisy blue channel at high ISO. Weighted luma
    or the green channel alone is cleaner through the Bayer mosaic.

12. **Winner-take-all proMap (line 295) discards multi-modality.**
    [partially fixed downstream] A projector pixel's light has two
    real answers in this artwork (ball glint + leak landing spot);
    keeping the top-2 modes per pixel preserves both for downstream
    logic, which the new solver's mode-aware fusion could then use
    directly instead of re-deriving modes from pairs.

13. **`overflow_fix` in buildXyzMapCommand.py maps out-of-range codes
    to pixel [0,0]** - garbage silently accumulates in the corner
    projector pixel instead of being dropped.

## Manual calibration path (camamokjs)

14. **BUG - focal initialization formula in `cv.ts`:**
    `f = (w/2) * atan(fov/2)` - that is `atan` where `tan` belongs
    and a multiply where a divide belongs. At the hardcoded fov=107
    the two wrong operations nearly cancel (2163 vs the correct
    2131), which is why nobody noticed. CALIB_USE_INTRINSIC_GUESS
    refines past it, but at other fovs this seeds calibration badly.

15. **fov hardcoded at 107 degrees**; EXIF focal from the loaded
    reference image would seed every calibration correctly.

## Structural (addressed by the model-free solver this branch adds)

- [fixed] no cross-scan consistency checking existed at all - scans
  could disagree wildly (LAN's two sessions) and only manual curation
  caught it; the solver measures it.
- [fixed] triangulation, shared-intrinsics bundle adjustment,
  distortion modeling, epipolar verification, dense multi-pair fusion,
  speckle cleanup, mesh generation, mesh-fill, auto gauge fixing.
- [open] registering 360-degree far-side cameras whose only shared
  content is view-dependent glints: full-resolution essential
  matrices recover their relative poses (1,100-2,600 inliers); scale
  transfer is the remaining piece, or a mini-camamok click UI against
  the reconstructed mesh.

## Suggested order of attack

1. Re-decode TodaysArt's raw cameraImages with items 7-11 and measure
   the delta in confident coverage + triangulation noise (the archive
   has the raw JPEGs; Llum's does not).
2. Fix the two bugs (1, 14) and the corner-pixel sink (13).
3. Next capture: items 2-6 (costs nothing, mostly removes work) plus
   the capture rules in README.md.

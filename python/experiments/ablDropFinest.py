"""drop_finest ablation, isolated. The two finest gray-code levels are
~1-4 projector px -- below the camera/defocus resolution limit. Letting
their (noise) margins into the per-pixel min-margin confidence drags
every pixel toward the floor. Skip them (+ subpixel pooling) to recover
honest confidence. Toggle: drop_finest = 0 vs 2, mode C, same scan."""
import os
os.environ["OPENCV_IO_ENABLE_OPENEXR"] = "1"
import sys, numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from diag_decoder import decode_axis_variant, load_axis

RAW = os.path.join(HERE, 'lan_raw', 'SharedData', 'scan-1831')
nv, iv = load_axis(RAW, 'vertical')
nh, ih = load_axis(RAW, 'horizontal')

# decode_axis_variant hardcodes drop_finest=2 via its `use`; re-implement
# the toggle by trimming levels fed in for the =0 case is wrong (levels
# are MSB-first), so call with an explicit drop_finest param.
def conf_for_drop(drop):
    _, mx = decode_axis_variant(nv, iv, 'C', drop_finest=drop)
    _, my = decode_axis_variant(nh, ih, 'C', drop_finest=drop)
    conf = np.minimum(mx, my) / 50.0
    return conf

for drop in (0, 2):
    conf = conf_for_drop(drop)
    print(f"drop_finest={drop}: confident>0.05 {100*(conf>0.05).mean():.1f}%  "
          f"median conf {np.median(conf):.3f}  "
          f"p75 {np.percentile(conf,75):.3f}")

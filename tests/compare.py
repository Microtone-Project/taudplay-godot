#!/usr/bin/env python3
"""Compare a capture from tests/godot against the taudplay CLI's raw render.

    compare.py GODOT.f32 CLI.f32

Both are interleaved stereo float32. The capture starts with however much
bus silence came before the stream did, found from the first sound in each.
Godot fades every new playback in across the mix step it starts in (512
frames), so that step is skipped; everything after it must be identical,
bit for bit, up to the end of the CLI render.
"""
import sys
import numpy as np

g = np.fromfile(sys.argv[1], dtype="<f4").reshape(-1, 2)
c = np.fromfile(sys.argv[2], dtype="<f4").reshape(-1, 2)

first_c = int(np.flatnonzero(np.any(c != 0, axis=1))[0])
first_g = int(np.flatnonzero(np.any(g != 0, axis=1))[0])
lag = first_g - first_c
if lag < 0:
    sys.exit("capture starts before the song does")
MIX_STEP = 512
skip = (lag // MIX_STEP + 1) * MIX_STEP - lag
n = min(len(g) - lag, len(c)) - skip
a, b = g[lag + skip:lag + skip + n], c[skip:skip + n]
diff = np.flatnonzero(np.any(a.view(np.uint32) != b.view(np.uint32), axis=1))
print(f"stream starts {lag} frames into the capture; {n} frames compared after Godot's {skip}-frame fade-in")
if len(diff):
    i = int(diff[0])
    print(f"DIFFER: {len(diff)} frames, first at {skip + i}: godot {a[i]} cli {b[i]}, max |d| {np.abs(a - b).max():.3g}")
    sys.exit(1)
print("identical")

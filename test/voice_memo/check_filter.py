# /// script
# requires-python = ">=3.11"
# dependencies = ["scipy==1.18.1"]
# ///
"""Compare the actual firmware float32 filter to the owner's SciPy SOS preset."""
import json
from pathlib import Path
import sys
import numpy as np
from scipy import signal

preset = json.loads(Path(sys.argv[3]).read_text())
sos = np.array(preset["sos"])
assert np.all(sos[:, 3] == 1)
for coefficients in (sos, sos.astype(np.float32).astype(float)):
    poles = signal.sos2zpk(coefficients)[1]
    assert np.max(np.abs(poles)) < 1
x, actual = (np.fromfile(path, dtype="<f4") for path in sys.argv[1:3])
expected = signal.sosfilt(sos, x.astype(float))
error = np.abs(actual - expected)
assert np.all(np.isfinite(actual))
assert error.max() < 2.0, f"Excessive float32 error: {error.max()} PCM units"
print(f"{preset['sample_rate']} Hz filter vs SciPy float64: max error {error.max():.6f}, RMS {np.sqrt(np.mean(error**2)):.6f} PCM units")

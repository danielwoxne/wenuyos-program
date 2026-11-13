# SPDX-License-Identifier: GPL-3.0-or-later
"""Render a real WenuyGFX CPU frame without an EGL window and check its PPM."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary, project = sys.argv[1:3]
env = dict(os.environ, WENUYOS_PERF="low", WENUYOS_GFX="soft")
with tempfile.TemporaryDirectory(prefix="wenuy-gfx-notes-") as folder:
    output = Path(folder) / "map.ppm"
    subprocess.run([binary, project, str(output), "--low"], env=env,
                   timeout=30, check=True)
    with output.open("rb") as image:
        assert image.readline() == b"P6\n"
        assert image.readline() == b"320 180\n"
        assert image.readline() == b"255\n"
        pixels = image.read()
        assert len(pixels) == 320 * 180 * 3
        assert any(pixels), "3D map is entirely black"
    # Never silently overwrite someone else's output.
    result = subprocess.run([binary, project, str(output), "--low"], env=env,
                            timeout=30, capture_output=True)
    assert result.returncode != 0
print("PASS: WenuyGFX 0.9 CPU note-map frame, low profile, no overwrite")

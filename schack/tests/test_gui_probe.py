#!/usr/bin/env python3
"""Real GTK4/Wayland geometry regression for Wenuy Schack.

Starts an isolated labwc headless session with software rendering, then asks
Schack to select/move e2-e4 and toggle the chess school while checking actual
GtkWidget bounds. Never joins a network, changes host configuration or writes
outside a disposable home. Uses only Python standard library.
"""
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time


def main():
    if len(sys.argv) != 2 or not Path(sys.argv[1]).is_file():
        raise SystemExit("usage: test_gui_probe.py /path/to/schack")
    app = str(Path(sys.argv[1]).resolve())
    if not shutil.which("labwc"):
        raise SystemExit("labwc missing; cannot run GUI integration probe")

    with tempfile.TemporaryDirectory(prefix="schack-ui-probe-") as tmp:
        root = Path(tmp)
        run = root / "run"
        run.mkdir(mode=0o700)
        cfg = root / "labwc"
        cfg.mkdir()
        (cfg / "autostart").write_text("", encoding="utf-8")
        env = os.environ.copy()
        env.update({
            "HOME": tmp,
            "XDG_DATA_HOME": str(root / "data"),
            "XDG_CONFIG_HOME": str(root / "config"),
            "XDG_RUNTIME_DIR": str(run),
            "WLR_BACKENDS": "headless",
            "WLR_RENDERER": "pixman",
            "WLR_HEADLESS_OUTPUTS": "1",
            "WLR_LIBINPUT_NO_DEVICES": "1",
            "WENUYOS_PERF": "low",
            "WENUYOS_GFX": "soft",
            "GSK_RENDERER": "cairo",
            "LIBGL_ALWAYS_SOFTWARE": "1",
            "GDK_BACKEND": "wayland",
        })
        env.pop("WAYLAND_DISPLAY", None)
        env.pop("DISPLAY", None)
        # The SDL/video compositor is launched without any user autostart.
        compositor = subprocess.Popen(
            ["labwc", "-C", str(cfg)], env=env,
            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
            start_new_session=True,
        )
        try:
            socket = None
            for _ in range(100):
                sockets = list(run.glob("wayland-*"))
                sockets = [p for p in sockets if not p.name.endswith(".lock")]
                if sockets:
                    socket = sockets[0]
                    break
                if compositor.poll() is not None:
                    raise RuntimeError("labwc exited before opening Wayland socket")
                time.sleep(0.05)
            if socket is None:
                raise RuntimeError("labwc headless did not create a Wayland socket")
            env["WAYLAND_DISPLAY"] = socket.name
            # Probe both a 1024x600-style window and an even smaller layout.
            for size in ("normal", "small"):
                cmd = [app, "--ui-probe-small" if size == "small" else "--ui-probe"]
                try:
                    result = subprocess.run(
                        cmd, env=env, cwd=tmp, capture_output=True, text=True,
                        timeout=16, check=False,
                    )
                except subprocess.TimeoutExpired as exc:
                    raise RuntimeError(f"{size}: Schack GUI probe timed out") from exc
                if result.returncode != 0 or "board bounds stable" not in result.stdout:
                    raise RuntimeError(
                        f"{size}: GUI probe failed with {result.returncode}\n"
                        f"stdout: {result.stdout[-1200:]}\n"
                        f"stderr: {result.stderr[-2000:]}"
                    )
                print(f"GUI {size}: board stable across selection, e2-e4, school toggle")
        finally:
            try:
                os.killpg(compositor.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                compositor.communicate(timeout=3)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(compositor.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                compositor.communicate()


if __name__ == "__main__":
    main()

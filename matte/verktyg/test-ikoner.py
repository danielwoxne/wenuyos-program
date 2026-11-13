#!/usr/bin/env python3
"""Regression för Matte: reservikoner i build/, riktiga ikoner bevaras.

Kan köras med bara Python 3: python3 verktyg/test-ikoner.py /sökväg/till/matte
Arbetar enbart i en tillfällig katalog, utan användardata eller nätverk.
"""
from pathlib import Path
import os
import runpy
import subprocess
import sys
import tempfile


def kolla(villkor: bool, meddelande: str) -> None:
    if not villkor:
        raise AssertionError(meddelande)


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("Användning: test-ikoner.py MATTE-KÄLLMAPP")
    repo = Path(sys.argv[1]).resolve()
    generator = repo / "verktyg" / "generera-saknade-ikoner.py"
    expandera = repo / "verktyg" / "expandera.py"
    kolla(generator.is_file() and expandera.is_file(), "saknar ikon-/innehållsverktyg")
    kopiera_ikoner = runpy.run_path(str(expandera))["kopiera_ikoner"]

    with tempfile.TemporaryDirectory(prefix="matte-icon-test-") as td:
        t = Path(td)
        kallmapp = t / "kallkod"
        ikonmapp = kallmapp / "innehall" / "data" / "ikoner"
        kortmapp = kallmapp / "innehall" / "data" / "kort"
        ikonmapp.mkdir(parents=True)
        kortmapp.mkdir(parents=True)
        (kallmapp / "src").mkdir()
        (kallmapp / "src" / "Fonster.cpp").write_text("// Testfil\n", encoding="utf-8")
        (kortmapp / "prov.toml").write_text('ikon = "test_fallback"\n', encoding="utf-8")
        original = b"verklig-originalikon"
        (ikonmapp / "apple.png").write_bytes(original)
        byggmapp = t / "bygg" / "reservikoner"

        subprocess.run([sys.executable, str(generator), str(kallmapp), str(byggmapp)],
                       check=True, capture_output=True, text=True, timeout=65)
        kolla((ikonmapp / "apple.png").read_bytes() == original,
              "generatorn ändrade den verkliga ikonen")
        kolla(not (ikonmapp / "test_fallback.png").exists(),
              "generatorn skrev en reservikon i källkodsmappen")
        reserv = byggmapp / "test_fallback.png"
        kolla(reserv.is_file() and reserv.read_bytes().startswith(b"\x89PNG\r\n\x1a\n"),
              "reservikonen genererades inte till byggmappen")

        ut = t / "bygg" / "delade-data"
        kopiera_ikoner(kallmapp / "innehall", ut, byggmapp)
        kolla((ut / "ikoner" / "apple.png").read_bytes() == original,
              "bokstavsreserv ersatte den verkliga äppelikonen")
        kolla((ut / "ikoner" / "test_fallback.png").read_bytes() == reserv.read_bytes(),
              "saknad ikon fylldes inte av reserven")
        # Även läroplanskontrollen måste se samma katalog, trots att källan är orörd.
        tidigare = os.environ.get("MATTE_IKONER_DIR")
        try:
            os.environ["MATTE_IKONER_DIR"] = str((ut / "ikoner").resolve())
            mod = runpy.run_path(str(repo / "innehall" / "matte" / "ikoner.py"))
            kolla("test_fallback" in mod["alla"](), "ikonvalideringen hittar inte byggikonen")
            kolla(mod["sokvag"]("apple").read_bytes() == original,
                  "ikonvalideringen hittar inte originalikonen")
        finally:
            if tidigare is None:
                os.environ.pop("MATTE_IKONER_DIR", None)
            else:
                os.environ["MATTE_IKONER_DIR"] = tidigare
        # Om originalkatalogen helt saknas måste byggningen ändå kunna slutföras.
        tom = t / "utan-original" / "innehall"
        kopiera_ikoner(tom, t / "bygg" / "tom-original", byggmapp)
        kolla((t / "bygg" / "tom-original" / "ikoner" / "test_fallback.png").exists(),
              "saknad originalmapp får inte stoppa byggningen")
        kolla(not (tom / "data" / "ikoner").exists(),
              "kopieringen skapade något i källkodsmappen")
        # Om ett äkta bildpaket dyker upp vid nästa bygge får reserven aldrig vinna.
        kopiera_ikoner(kallmapp / "innehall", ut, byggmapp)
        kolla((ut / "ikoner" / "apple.png").read_bytes() == original,
              "originalet byttes ut vid ombyggning")
    print("OK: reservikoner ligger i byggmappen, original prioriteras, källkod orörd")


if __name__ == "__main__":
    main()

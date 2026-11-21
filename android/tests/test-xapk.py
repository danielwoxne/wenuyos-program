#!/usr/bin/env python3
"""Prov: Android-appar i flera delar (wenuyos_xapk.py) – .xapk, .apks, uppackad mapp, ARM-delar, OBB, och att
ingenting kan skrivas utanför mappen (../ i zip-arkivet). Allt i en tillfällig mapp, inget nätverk, ingen Android."""
import json, os, subprocess, sys, tempfile, zipfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))   # programmappen android/
sys.path.insert(0, os.path.join(ROOT, "lib"))
import wenuyos_xapk as X  # noqa: E402

ok = fails = 0


def check(cond, what):
    global ok, fails
    if cond:
        ok += 1
    else:
        fails += 1
        print("FEL:", what)


MAN = {"package_name": "com.innersloth.spacemafia", "name": "Among Us", "split_apks": [
    {"file": "com.innersloth.spacemafia.apk", "id": "base"}, {"file": "config.arm64_v8a.apk", "id": "config.arm64_v8a"},
    {"file": "UnityDataAssetPack.apk", "id": "UnityDataAssetPack"}],
    "expansions": [{"file": "Android/obb/com.innersloth.spacemafia/main.12.com.innersloth.spacemafia.obb", "install_location": "EXTERNAL_STORAGE"}]}

with tempfile.TemporaryDirectory() as t:
    # 1. en .xapk som APKPure gör den
    xp = os.path.join(t, "among.xapk")
    with zipfile.ZipFile(xp, "w") as z:
        z.writestr("manifest.json", json.dumps(MAN))
        z.writestr("icon.png", b"png")
        for n in ("UnityDataAssetPack.apk", "config.arm64_v8a.apk", "com.innersloth.spacemafia.apk"):
            z.writestr(n, b"apk " + n.encode())
        z.writestr("Android/obb/com.innersloth.spacemafia/main.12.com.innersloth.spacemafia.obb", b"obb")
    out = os.path.join(t, "ut1")
    p = X.prepare(xp, out)
    check(p["package"] == "com.innersloth.spacemafia" and p["name"] == "Among Us", "paket och namn ur manifest.json")
    check(p["apks"][0] == "com.innersloth.spacemafia.apk" and len(p["apks"]) == 3, "basen först, alla tre delarna")
    check(p["abis"] == ["arm64_v8a"] and p["needs_arm"], "bara ARM-delar: ARM-stöd behövs")
    check(p["obb"] == [["main.12.com.innersloth.spacemafia.obb", "com.innersloth.spacemafia"]], "OBB-filen följer med")
    check(sorted(os.listdir(out)) == sorted(p["apks"] + ["main.12.com.innersloth.spacemafia.obb"]), "bara delarna i mappen (inte icon.png)")

    # 2. samma, uppackad i Filer (som Daniels mapp): Installera på basen ska ta hela mappen
    d = os.path.join(t, "com.innersloth.spacemafia_2026.11.3.xapk (2)")
    os.makedirs(d)
    with zipfile.ZipFile(xp) as z:
        z.extractall(d)
    check(X.is_split_dir(d), "uppackad mapp känns igen som en delad app")
    r = subprocess.run([sys.executable, os.path.join(ROOT, "lib/wenuyos_xapk.py"), "split",
                        os.path.join(d, "com.innersloth.spacemafia.apk")], capture_output=True, text=True)
    check(r.stdout.strip() == d, "split <bas.apk> pekar på mappen")
    p2 = X.prepare(d, os.path.join(t, "ut2"))
    check(p2["apks"] == p["apks"] and p2["needs_arm"], "mappen ger samma plan")
    check(p2["obb"] == [["main.12.com.innersloth.spacemafia.obb", "com.innersloth.spacemafia"]], "OBB ur mappen Android/obb")

    # 3. en vanlig .apk ensam i Hämtade filer är ingen delad app
    solo = os.path.join(t, "solo")
    os.makedirs(solo)
    open(os.path.join(solo, "app.apk"), "wb").write(b"x")
    open(os.path.join(solo, "annan.apk"), "wb").write(b"x")
    check(not X.is_split_dir(solo), "två fristående .apk utan manifest/config är ingen delad app")

    # 4. .apks (SAI) utan manifest: split_config.<abi>.apk, x86_64 finns → inget ARM-stöd behövs
    ap = os.path.join(t, "x.apks")
    with zipfile.ZipFile(ap, "w") as z:
        for n in ("base.apk", "split_config.x86_64.apk", "split_config.arm64_v8a.apk", "split_config.sv.apk"):
            z.writestr(n, b"a")
    p4 = X.prepare(ap, os.path.join(t, "ut4"))
    check(len(p4["apks"]) == 4 and p4["abis"] == ["arm64_v8a", "x86_64"] and not p4["needs_arm"], "x86-del finns: ingen översättare")

    # 5. elakt arkiv: ../ och absoluta sökvägar skrivs aldrig
    ev = os.path.join(t, "elak.xapk")
    with zipfile.ZipFile(ev, "w") as z:
        z.writestr("manifest.json", json.dumps({"package_name": "a.b", "split_apks": [{"file": "../../utanfor.apk"}, {"file": "base.apk"}],
                                                "expansions": [{"file": "../../../x.obb"}]}))
        z.writestr("base.apk", b"a")
        z.writestr("../../utanfor.apk", b"a")
    p5 = X.prepare(ev, os.path.join(t, "ut5"))
    check(p5["apks"] == ["base.apk"] and p5["obb"] == [], "../ i arkivet ignoreras")
    check(not os.path.exists(os.path.join(t, "utanfor.apk")) and os.listdir(os.path.join(t, "ut5")) == ["base.apk"], "inget skrivs utanför")
    check(X.safe_name("../a.apk") == "" and X.safe_name("a;rm.apk") == "" and X.safe_name("config.arm64_v8a.apk") == "config.arm64_v8a.apk", "filnamnsregler")
    check(X.plan_from(["base.apk"], {"package_name": "inte ett paket; rm"})["package"] == "", "konstigt paketnamn används inte")

    # 6. inte ett arkiv, och ett arkiv utan .apk
    bad = os.path.join(t, "fel.xapk")
    open(bad, "w").write("hej")
    r = subprocess.run([sys.executable, os.path.join(ROOT, "lib/wenuyos_xapk.py"), "prepare", bad, os.path.join(t, "ut6")],
                       capture_output=True, text=True)
    check(r.returncode == 1 and "error" in json.loads(r.stdout), "inte en .xapk: fel, ingen krasch")
    tom = os.path.join(t, "tom.xapk")
    with zipfile.ZipFile(tom, "w") as z:
        z.writestr("manifest.json", "{}")
    r = subprocess.run([sys.executable, os.path.join(ROOT, "lib/wenuyos_xapk.py"), "prepare", tom, os.path.join(t, "ut7")],
                       capture_output=True, text=True)
    check(r.returncode == 1, "arkiv utan .apk: fel")
    check(X.read_manifest("inte json") == {} and X.read_manifest("[1]") == {}, "trasig manifest.json")

    # 7. OBB i en uppackad mapp räknas in i taken (antal filer, storlek) – ChatGPT:s granskning av #100
    big = os.path.join(t, "manga-obb")
    os.makedirs(os.path.join(big, "Android/obb/com.exempel.spel"))
    open(os.path.join(big, "manifest.json"), "w").write(json.dumps({"package_name": "com.exempel.spel"}))
    for n in ("com.exempel.spel.apk", "config.x86_64.apk"):
        open(os.path.join(big, n), "wb").write(b"apk")
    for i in range(X.MAX_FILES):
        open(os.path.join(big, f"Android/obb/com.exempel.spel/patch.{i}.com.exempel.spel.obb"), "wb").write(b"o")
    try:
        X.prepare(big, os.path.join(t, "ut8")); check(False, "för många OBB-filer ska stoppas")
    except X.Bad:
        check(True, "")
    old_total = X.MAX_TOTAL
    X.MAX_TOTAL = 10
    try:
        few = os.path.join(t, "stor-obb")
        os.makedirs(os.path.join(few, "Android/obb/com.exempel.spel"))
        open(os.path.join(few, "manifest.json"), "w").write(json.dumps({"package_name": "com.exempel.spel"}))
        for n in ("com.exempel.spel.apk", "config.x86_64.apk"):
            open(os.path.join(few, n), "wb").write(b"apk")
        open(os.path.join(few, "Android/obb/com.exempel.spel/main.1.com.exempel.spel.obb"), "wb").write(b"0123456789")
        try:
            X.prepare(few, os.path.join(t, "ut9")); check(False, "OBB över storlekstaket ska stoppas")
        except X.Bad:
            check(True, "")
    finally:
        X.MAX_TOTAL = old_total

    # 8. administratörsdelen (wenuyos_android_split.py, i systemet) provas i New-wenuyos (tools/check/test-android-root.py)

print(f"xapk: {ok} godkända, {fails} fel")
sys.exit(1 if fails else 0)

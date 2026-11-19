#!/usr/bin/env python3
"""Android-appar i flera delar (Daniel 13 nov): .xapk, .apks, .apkm – eller en uppackad mapp med manifest.json och
flera .apk (basen + config.arm64_v8a.apk + tillgångspaket). waydroid app install tar bara en .apk; här tas delarna
fram och läggs i en mapp som wenuyos-android-root installerar på en gång (pm install med alla delar).

  wenuyos_xapk.py split <fil.apk>          är .apk:n en del av en delad app? (skriver mappen, annars inget)
  wenuyos_xapk.py prepare <källa> <mapp>   packa upp/kopiera delarna till <mapp>, skriv en JSON-plan

Planen: {"package", "name", "apks": [filnamn], "obb": [[filnamn, paket]], "abis": [...], "needs_arm": bool}
Bara logik och filer – inga nätverksanrop, inget som kräver administratör. Provas av tools/check/test-xapk.py.
"""
import json
import os
import re
import shutil
import sys
import zipfile

MAX_FILES = 64                     # fler delar än så: inte en app
MAX_TOTAL = 8 * 1024 ** 3          # 8 GB uppackat (spel med OBB kan vara stora, men inte större)
NAME_OK = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._ ()+-]{0,150}\.(apk|obb)$")
PKG_OK = re.compile(r"^[A-Za-z][A-Za-z0-9_]*(\.[A-Za-z0-9_]+)+$")
ARM = {"arm64_v8a", "armeabi_v7a", "armeabi"}
X86 = {"x86", "x86_64"}


class Bad(Exception):
    pass


def safe_name(n):
    """Bara ett filnamn – aldrig en sökväg (../, /) eller konstiga tecken."""
    if not isinstance(n, str) or "/" in n or "\\" in n or ".." in n:
        return ""
    return n if NAME_OK.match(n) else ""


def abis_of(apks):
    """config.arm64_v8a.apk, split_config.x86_64.apk → {"arm64_v8a", "x86_64"}"""
    out = set()
    for a in apks:
        m = re.search(r"config\.([a-z0-9_]+)\.apk$", a)
        if m and m.group(1) in ARM | X86:
            out.add(m.group(1))
    return sorted(out)


def needs_arm(abis):
    """Bara ARM-delar (ingen x86): appen behöver ARM-översättaren på en vanlig dator."""
    return bool(abis) and not any(a in X86 for a in abis)


def read_manifest(text):
    try:
        m = json.loads(text)
    except (ValueError, TypeError):
        return {}
    return m if isinstance(m, dict) else {}


def plan_from(names, manifest):
    """names = filerna (bara namn); manifest = manifest.json (kan vara tomt)."""
    pkg = str(manifest.get("package_name") or "")
    if pkg and not PKG_OK.match(pkg):
        pkg = ""
    apks = sorted(n for n in names if "/" not in n.strip("/") and n.lower().endswith(".apk") and safe_name(n))   # delarna ligger överst
    # manifestets ordning: basen först (pm vill ha basen med, ordningen spelar annars ingen roll)
    split = [s.get("file") for s in manifest.get("split_apks") or [] if isinstance(s, dict)]
    split = [safe_name(s) for s in split if isinstance(s, str) and safe_name(s) in apks]
    if split:
        apks = split + [a for a in apks if a not in split]
    if pkg and f"{pkg}.apk" in apks:
        apks.remove(f"{pkg}.apk")
        apks.insert(0, f"{pkg}.apk")
    obb = []
    for e in manifest.get("expansions") or []:
        if not isinstance(e, dict):
            continue
        f = e.get("file") if isinstance(e.get("file"), str) else ""
        n = safe_name(os.path.basename(f))
        if n and n.lower().endswith(".obb") and pkg and f.replace("\\", "/").lstrip("/") in names_full(names):
            obb.append([f, pkg])
    abis = abis_of(apks)
    return {"package": pkg, "name": str(manifest.get("name") or pkg or (apks[0][:-4] if apks else "")), "apks": apks,
            "obb": obb, "abis": abis, "needs_arm": needs_arm(abis)}


def names_full(names):
    return set(n.replace("\\", "/").lstrip("/") for n in names)


def is_split_dir(d):
    """En mapp med manifest.json och flera .apk, eller flera .apk där någon är en config-del."""
    try:
        files = os.listdir(d)
    except OSError:
        return False
    apks = [f for f in files if f.lower().endswith(".apk")]
    if len(apks) < 2:
        return False
    return "manifest.json" in files or any(re.search(r"(^|[._])config\.", a) for a in apks) or any(a.startswith("split_") for a in apks)


def prepare(src, out):
    """Lägg delarna i out (en tom mapp som skapas) och returnera planen."""
    if os.path.exists(out):
        shutil.rmtree(out)
    os.makedirs(out, mode=0o700)
    total = 0
    if os.path.isdir(src):
        names = []
        for f in sorted(os.listdir(src)):
            p = os.path.join(src, f)
            if os.path.islink(p) or not os.path.isfile(p):
                continue
            if f == "manifest.json" or safe_name(f):
                names.append(f)
        if len(names) > MAX_FILES:
            raise Bad("för många filer")
        manifest = {}
        if "manifest.json" in names:
            with open(os.path.join(src, "manifest.json"), encoding="utf-8", errors="replace") as fh:
                manifest = read_manifest(fh.read(256 * 1024))
        for f in names:
            if f == "manifest.json":
                continue
            total += os.path.getsize(os.path.join(src, f))
            if total > MAX_TOTAL:
                raise Bad("för stort")
            shutil.copyfile(os.path.join(src, f), os.path.join(out, f))
        plan = plan_from([n for n in names if n != "manifest.json"], manifest)
        # OBB i en mapp: Android/obb/<paket>/main.….obb bredvid (uppackad .xapk)
        # … och de räknas in i samma tak (antal filer, total storlek) som .apk-delarna
        plan["obb"] = []
        count = len(names)
        for root, dirs, files in os.walk(os.path.join(src, "Android", "obb"), followlinks=False):
            dirs[:] = [d for d in dirs if not os.path.islink(os.path.join(root, d))]
            for f in sorted(files):
                p = os.path.join(root, f)
                n = safe_name(f)
                if os.path.islink(p) or not os.path.isfile(p):
                    continue
                if n and n.endswith(".obb") and plan["package"] and os.path.basename(root) == plan["package"]:
                    count += 1
                    if count > MAX_FILES:
                        raise Bad("för många filer")
                    total += os.path.getsize(p)
                    if total > MAX_TOTAL:
                        raise Bad("för stort")
                    shutil.copyfile(p, os.path.join(out, n))
                    plan["obb"].append([n, plan["package"]])
        return plan
    if not zipfile.is_zipfile(src):
        raise Bad("inte en .xapk")
    with zipfile.ZipFile(src) as z:
        infos = [i for i in z.infolist() if not i.is_dir()]
        if len(infos) > MAX_FILES:
            raise Bad("för många filer")
        if sum(i.file_size for i in infos) > MAX_TOTAL:
            raise Bad("för stort")
        manifest = {}
        if "manifest.json" in z.namelist():
            manifest = read_manifest(z.read("manifest.json")[:256 * 1024].decode("utf-8", "replace"))
        names = [i.filename for i in infos]
        plan = plan_from(names, manifest)
        for a in plan["apks"]:
            with z.open(a) as fi, open(os.path.join(out, a), "wb") as fo:
                shutil.copyfileobj(fi, fo)
        obb = []
        for f, pkg in plan["obb"]:
            n = safe_name(os.path.basename(f))
            with z.open(f.lstrip("/")) as fi, open(os.path.join(out, n), "wb") as fo:
                shutil.copyfileobj(fi, fo)
            obb.append([n, pkg])
        plan["obb"] = obb
    return plan


def main(argv):
    if len(argv) >= 2 and argv[0] == "split":
        d = os.path.dirname(os.path.abspath(argv[1]))
        if is_split_dir(d):
            print(d)
        return 0
    if len(argv) >= 3 and argv[0] == "prepare":
        try:
            plan = prepare(argv[1], argv[2])
        except (Bad, OSError, zipfile.BadZipFile, KeyError) as e:
            print(json.dumps({"error": str(e)}))
            return 1
        if not plan["apks"]:
            print(json.dumps({"error": "inga .apk"}))
            return 1
        print(json.dumps(plan, ensure_ascii=False))
        return 0
    print(__doc__.strip().splitlines()[0])
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

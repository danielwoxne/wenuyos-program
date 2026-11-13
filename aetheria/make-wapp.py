#!/usr/bin/env python3
# WenuyOS: gör en .wapp (färdigt program att dubbelklicka på) – utan WenuyOS, bara Python 3.8+ (standardbiblioteket).
# Samma format som `wenuyos app pack` på en WenuyOS-dator: en zip med wenuyos-app.toml, programmets filer och
# SHA256SUMS (kontrollsummor; WenuyOS vägrar installera en .wapp där något har ändrats). Se docs/PROGRAM-FOR-WENUYOS.md.
#
#   python3 make-wapp.py MAPP [-o fil.wapp]
#
# MAPP innehåller det färdiga programmet, som det ska ligga på datorn (~/Program/<Namn>/):
#   wenuyos-app.toml            name, version … och  exec = "bin/programmet"  för ett byggt program (C++/Rust);
#                               requires = körpaketen (byggs skriptet i Debian läggs bibliotekens paket till själv)
#   bin/programmet              byggt för Debian 13 amd64 (eller main.py / main.js / game.ini – då behövs ingen exec)
#   data/<id>.desktop           namnet i Start (MÅSTE finnas)
#   data/<id>.svg               ikonen
#   share/ …                    programmets egna data (hittas från programfilen: <bin>/../share)
import hashlib
import json
import os
import re
import stat
import sys
import zipfile

MANIFEST, SUMS = "wenuyos-app.toml", "SHA256SUMS"
SKIP = {".git", "build", "target", "node_modules", "__pycache__", ".wenuyos"}


def fail(msg):
    sys.exit("make-wapp: " + msg)


def read_manifest(path):
    text = open(path, encoding="utf-8").read()
    try:
        import tomllib   # Python 3.11+
        return tomllib.loads(text)
    except ImportError:   # äldre Python: enkla rader  nyckel = "värde" / [lista] / tal / true
        m = {}
        for line in text.splitlines():
            mm = re.match(r'\s*([A-Za-z_][\w]*)\s*=\s*(.+?)\s*(#.*)?$', line)
            if mm:
                v = mm.group(2)
                try:
                    m[mm.group(1)] = json.loads(v)   # "text", [listor], tal, true/false skrivs likadant i JSON
                except ValueError:
                    m[mm.group(1)] = v.strip('"')
        return m


BUILD_TOOLS = {"cmake", "make", "g++", "gcc", "pkgconf", "pkg-config", "meson", "ninja-build", "cargo", "rustc", "build-essential"}
BASE = {"libc6", "libstdc++6", "libgcc-s1", "libglib2.0-0t64", "libgtk-4-1", "libadwaita-1-0", "libcairo2", "libpango-1.0-0",
        "libpangocairo-1.0-0", "libgdk-pixbuf-2.0-0", "libgraphene-1.0-0", "libharfbuzz0b", "libsqlite3-0"}   # finns alltid på WenuyOS


def runtime_packages(exe, have):
    """Bara på Debian/Ubuntu: vilka paket kommer programmets bibliotek från (ldd + dpkg -S)? Annars tomt."""
    import shutil
    import subprocess
    if not (shutil.which("ldd") and shutil.which("dpkg")):
        return set()
    found = dict(re.findall(r"^\s*(\S+)\s+=>\s+(/\S+)", subprocess.run(["ldd", exe], capture_output=True, text=True).stdout, re.M))
    # bara de bibliotek programmet själv använder (DT_NEEDED) – inte alla som GTK i sin tur drar in
    needed = re.findall(r"\(NEEDED\)\s+Shared library: \[(.+?)\]", subprocess.run(["readelf", "-d", exe], capture_output=True, text=True).stdout) \
        if shutil.which("readelf") else list(found)
    libs = [found[n] for n in needed if n in found]
    pk = set()
    for lib in libs:
        r = subprocess.run(["dpkg", "-S", os.path.realpath(lib)], capture_output=True, text=True)
        if r.returncode != 0:   # usrmerge: /lib/x86_64-linux-gnu/… är registrerad som /usr/lib/…
            r = subprocess.run(["dpkg", "-S", lib.replace("/usr/lib/", "/lib/", 1) if lib.startswith("/usr/lib/") else "/usr" + lib], capture_output=True, text=True)
        if r.returncode == 0:
            pk.add(r.stdout.split(":")[0].strip())
    return {p for p in pk if p not in BASE and p not in have}


def toml(m):
    def val(v):
        if isinstance(v, bool):
            return "true" if v else "false"
        if isinstance(v, int):
            return str(v)
        if isinstance(v, list):
            return "[" + ", ".join(val(x) for x in v) + "]"
        return json.dumps(str(v), ensure_ascii=False)
    return "# WenuyOS-program (.wapp) – skrivet av make-wapp.py\n" + "".join(f"{k} = {val(v)}\n" for k, v in m.items() if v not in (None, "", []))


def main():
    args = sys.argv[1:]
    out = None
    if "-o" in args:
        i = args.index("-o")
        out = args[i + 1]
        del args[i:i + 2]
    if len(args) != 1 or not os.path.isdir(args[0]):
        fail("användning: make-wapp.py MAPP [-o fil.wapp]")
    src = os.path.abspath(args[0])
    mf = os.path.join(src, MANIFEST)
    if not os.path.isfile(mf):
        fail(f"{MANIFEST} saknas i {src}")
    m = read_manifest(mf)
    desk = sorted(f for f in os.listdir(os.path.join(src, "data")) if f.endswith(".desktop")) if os.path.isdir(os.path.join(src, "data")) else []
    if not desk:
        fail("data/<id>.desktop saknas (namnet och ikonen i Start)")
    m.setdefault("id", desk[0][:-8])
    for k in ("name", "version"):
        if not m.get(k):
            fail(f"{MANIFEST}: {k} saknas")
    exe = m.get("exec")
    if exe:   # ett byggt program: filen måste finnas, vara en Linux-programfil (x86-64) och ligga i mappen
        p = os.path.join(src, exe)
        if ".." in exe.split("/") or exe.startswith("/") or not os.path.isfile(p):
            fail(f"exec = {exe!r} finns inte i mappen")
        head = open(p, "rb").read(20)
        if head[:4] == b"\x7fELF":
            if head[18:20] != b"\x3e\x00":
                fail(f"{exe} är inte byggd för amd64 (x86-64)")
            m.setdefault("arch", "amd64")
        os.chmod(p, os.stat(p).st_mode | 0o755)
        # requires i ett färdigt program = körpaketen: byggpaketen (-dev, kompilatorer) bort, bibliotekens paket till
        m.pop("build_requires", None)
        req = [x for x in (m.get("requires") or []) if not x.endswith("-dev") and x not in BUILD_TOOLS]
        added = sorted(runtime_packages(p, set(req)))
        m["requires"] = req + added
        if added:
            print(f"make-wapp: requires += {json.dumps(added)} (biblioteken programmet använder)", file=sys.stderr)
    elif not any(os.path.isfile(os.path.join(src, f)) for f in ("main.py", "main.js", "game.ini")):
        fail("ingen exec i wenuyos-app.toml och ingen main.py, main.js eller game.ini")
    m.setdefault("arch", "any")
    out = out or os.path.join(os.path.dirname(src), re.sub(r"[^\w.-]+", "-", str(m["name"])).strip("-").lower() + "-" + str(m["version"]) + ".wapp")
    files = []
    for root, dirs, fs in os.walk(src):
        dirs[:] = sorted(d for d in dirs if d not in SKIP and not d.startswith("."))
        for f in sorted(fs):
            p = os.path.join(root, f)
            rel = os.path.relpath(p, src).replace(os.sep, "/")
            if rel in (MANIFEST, SUMS, "SHA256SUMS.sig", "signerare.pub") or f.endswith(".wapp") or not os.path.exists(p):
                continue
            files.append((rel, p))
    manifest = toml(m).encode()
    sha = lambda b: hashlib.sha256(b).hexdigest()
    sums = "".join(f"{sha(open(p, 'rb').read())}  {rel}\n" for rel, p in files) + f"{sha(manifest)}  {MANIFEST}\n"
    tmp = out + ".tmp"
    with zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        z.writestr(MANIFEST, manifest)
        for rel, p in files:
            zi = zipfile.ZipInfo.from_file(p, rel)
            zi.external_attr = ((os.stat(p).st_mode & 0o755) | stat.S_IFREG) << 16   # aldrig setuid
            zi.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(zi, open(p, "rb").read())
        z.writestr(SUMS, sums)
    os.replace(tmp, out)
    print(out)


if __name__ == "__main__":
    main()

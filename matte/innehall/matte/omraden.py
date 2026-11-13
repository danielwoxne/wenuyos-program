"""`matte omraden <bild>` – sätt klickytor på en målning utan att räkna pixlar.

Skriver en fristående HTML-sida (öppnas i webbläsaren, fungerar utan nät):
  1. skriv vad nästa yta är ("gul", "mitten", "fat" …),
  2. dra en ruta över saken i bilden,
  3. upprepa – sidan visar hela tiden raden att klistra in i kortets uppgift:
     figur = { typ = "scen", bild = "…", bredd = 560, omraden = [...], namn = [...] }
Koordinaterna är andelar av bilden (0–1), så ytorna håller i alla storlekar.
"""
from __future__ import annotations

import base64
import html
import json
from pathlib import Path

from .kort import DATA


def sida(bild: Path) -> str:
    data = base64.b64encode(bild.read_bytes()).decode()
    mime = "image/png" if bild.suffix.lower() == ".png" else "image/jpeg"
    namn = json.dumps(bild.name)
    return f"""<!doctype html><html lang="sv"><head><meta charset="utf-8">
<title>Klickytor – {html.escape(bild.name)}</title>
<style>
 body {{ font-family: Inter, sans-serif; margin: 16px; background: #f4f4f2; color: #1f2328; }}
 #yta {{ position: relative; display: inline-block; max-width: 100%; user-select: none; cursor: crosshair; }}
 #yta img {{ display: block; max-width: 100%; }}
 .ruta {{ position: absolute; border: 3px solid #e8821c; background: rgba(232,130,28,.12); box-sizing: border-box; }}
 .ruta span {{ position: absolute; top: -20px; left: 0; background: #e8821c; color: #fff; font-size: 12px; padding: 1px 5px; }}
 .rad {{ display: flex; gap: 8px; align-items: center; margin: 10px 0; flex-wrap: wrap; }}
 input {{ font-size: 16px; padding: 4px 8px; border: 1px solid #3a3f44; }}
 button {{ font-size: 15px; padding: 4px 12px; border: 1px solid #3a3f44; background: #fff; cursor: pointer; }}
 button.huvud {{ background: #e8821c; color: #fff; border-color: #c86e14; }}
 pre {{ background: #fff; border: 1px solid #3a3f44; padding: 10px; white-space: pre-wrap; word-break: break-all; }}
</style></head><body>
<h2>Klickytor för {html.escape(bild.name)}</h2>
<div class="rad">Nästa yta heter: <input id="namn" value="sak" size="14">
 <button id="angra">Ångra senaste</button><button id="rensa">Börja om</button>
 <button class="huvud" id="kopiera">Kopiera raden</button></div>
<div>Dra en ruta över varje sak. Samma namn på flera ytor går bra (t.ex. alla gula brickor heter "gul").</div>
<div id="yta"><img id="bild" src="data:{mime};base64,{data}" draggable="false"></div>
<pre id="ut"></pre>
<script>
const bildnamn = {namn};
const yta = document.getElementById('yta'), bild = document.getElementById('bild');
let ytor = [], start = null, ny = null;
function rita() {{
  yta.querySelectorAll('.ruta').forEach(e => e.remove());
  ytor.forEach(o => {{
    const d = document.createElement('div'); d.className = 'ruta';
    d.style.left = (o.x*100)+'%'; d.style.top = (o.y*100)+'%';
    d.style.width = (o.b*100)+'%'; d.style.height = (o.h*100)+'%';
    d.innerHTML = '<span></span>'; d.firstChild.textContent = o.namn; yta.appendChild(d);
  }});
  const r = v => Math.round(v*10000)/10000;
  const om = ytor.map(o => '['+[r(o.x),r(o.y),r(o.b),r(o.h)].join(', ')+']').join(', ');
  const nm = ytor.map(o => JSON.stringify(o.namn)).join(', ');
  document.getElementById('ut').textContent =
    'figur = {{ typ = "scen", bild = "'+bildnamn+'", bredd = 560, omraden = ['+om+'], namn = ['+nm+'] }}';
}}
function pos(e) {{ const b = bild.getBoundingClientRect(); return [(e.clientX-b.left)/b.width, (e.clientY-b.top)/b.height]; }}
yta.addEventListener('pointerdown', e => {{ start = pos(e); ny = document.createElement('div'); ny.className = 'ruta'; yta.appendChild(ny); yta.setPointerCapture(e.pointerId); }});
yta.addEventListener('pointermove', e => {{ if (!start) return; const [x,y] = pos(e);
  ny.style.left = Math.min(x,start[0])*100+'%'; ny.style.top = Math.min(y,start[1])*100+'%';
  ny.style.width = Math.abs(x-start[0])*100+'%'; ny.style.height = Math.abs(y-start[1])*100+'%'; }});
yta.addEventListener('pointerup', e => {{ if (!start) return; const [x,y] = pos(e);
  const o = {{ x: Math.max(0,Math.min(x,start[0])), y: Math.max(0,Math.min(y,start[1])), b: Math.abs(x-start[0]), h: Math.abs(y-start[1]),
              namn: document.getElementById('namn').value.trim() || 'sak' }};
  start = null; ny.remove(); if (o.b > 0.01 && o.h > 0.01) ytor.push(o); rita(); }});
document.getElementById('angra').onclick = () => {{ ytor.pop(); rita(); }};
document.getElementById('rensa').onclick = () => {{ ytor = []; rita(); }};
document.getElementById('kopiera').onclick = () => navigator.clipboard && navigator.clipboard.writeText(document.getElementById('ut').textContent);
rita();
</script></body></html>"""


def hitta(namn: str) -> Path | None:
    p = Path(namn)
    if p.exists():
        return p
    q = DATA / "bilder" / namn
    return q if q.exists() else None

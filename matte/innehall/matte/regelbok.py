"""Regelboken som HTML – för utskrift, förhandsvisning och tester.

Samma kort som i fönstret, ritade med SvgDuk. `matte regelbok ut.html`
skriver hela boken (eller elevens egen, i den ordning korten mötts).
"""
from __future__ import annotations

import html

from . import tema
from .kort import DATA, Kort
from .rita import figur

STIL = f"""
body {{ font-family: {tema.TYPSNITT}; color: {tema.TEXT}; background: {tema.BAKGRUND}; margin: 0; padding: 24px; }}
.kort {{ background: {tema.KORT}; border: 1px solid {tema.LINJE}; max-width: 760px; margin: 0 auto 32px; padding: 0 0 20px; page-break-inside: avoid; }}
.kort h1 {{ font-size: 20px; margin: 0; padding: 12px 20px; border-bottom: 1px solid {tema.LINJE}; text-align: center; }}
.kort img.scen {{ display: block; width: 100%; height: auto; }}
.kort .bildtext {{ padding: 6px 20px 0; color: {tema.TEXT_SVAG}; font-size: 13px; font-style: italic; }}
.kort .sager {{ padding: 14px 20px 4px; font-size: 16px; }}
.kort .omrade {{ padding: 0 20px; color: {tema.TEXT_SVAG}; font-size: 12px; }}
table.rader {{ border-collapse: collapse; margin: 12px 20px; }}
table.rader td {{ padding: 6px 10px; vertical-align: middle; font-size: 17px; }}
table.rader td.skriver {{ max-width: 260px; font-variant-numeric: tabular-nums; }}
table.rader th {{ text-align: left; font-weight: 600; padding: 4px 10px; font-size: 14px; }}
table.former td {{ border: 1px solid {tema.LINJE}; text-align: center; min-width: 110px; }}
.galler {{ margin: 14px 20px 0; padding: 10px 14px; background: {tema.ACCENT_SVAG}; border-left: 4px solid {tema.ACCENT}; font-size: 15px; }}
.galler-inte {{ margin: 10px 20px 0; padding: 10px 14px; background: #fbe9e8; border-left: 4px solid {tema.ROD}; font-size: 15px; }}
.galler-inte s {{ color: {tema.TEXT_SVAG}; }}
.exempel {{ margin: 16px 20px 0; }}
.exempel h2 {{ font-size: 16px; margin: 0 0 6px; }}
.exempel ol {{ margin: 8px 0 0; padding-left: 22px; font-size: 15px; }}
.kalla {{ margin: 16px 20px 0; color: {tema.TEXT_SVAG}; font-size: 12px; }}
@media print {{ body {{ background: white; padding: 0; }} .kort {{ border: none; }} }}
"""


def kort_html(k: Kort) -> str:
    d = [f'<section class="kort" id="{html.escape(k.id)}">', f"<h1>{html.escape(k.titel)}</h1>",
         f'<div class="omrade">{html.escape(k.omrade_namn)}</div>']
    if k.bild:
        import base64
        bp = DATA / "bilder" / k.bild
        if bp.exists():
            data = base64.b64encode(bp.read_bytes()).decode()
            d.append(f'<img class="scen" src="data:image/jpeg;base64,{data}" alt="">')
            if k.bildtext:
                d.append(f'<div class="bildtext">{html.escape(k.bildtext)}</div>')
    if k.sager:
        d.append(f'<div class="sager">{html.escape(k.sager)}</div>')
    if k.rader:
        tre_former = all(len(r.skriver) == 3 for r in k.rader)
        klass = "rader former" if tre_former else "rader"
        d.append(f'<table class="{klass}">')
        if tre_former:
            d.append("<tr><th></th><th>Bråkform</th><th>Procentform</th><th>Decimalform</th></tr>")
        elif any(r.sager for r in k.rader):
            d.append("<tr><th>Man säger</th><th></th><th>Man skriver</th></tr>")
        for r in k.rader:
            d.append("<tr>")
            if not tre_former:
                d.append(f"<td>{html.escape(r.sager)}</td>")
            d.append("<td>" + (figur(r.figur).svg() if r.figur else "") + "</td>")
            if tre_former:
                d.extend(f'<td class="skriver">{html.escape(s)}</td>' for s in r.skriver)
            else:
                d.append('<td class="skriver">' + "<br>".join(html.escape(s) for s in r.skriver) + "</td>")
            d.append("</tr>")
            if r.kommentar:
                d.append(f'<tr><td colspan="4" style="color:{tema.TEXT_SVAG};font-size:13px">{html.escape(r.kommentar)}</td></tr>')
        d.append("</table>")
    for e in k.exempel:
        d.append('<div class="exempel">')
        if e.rubrik:
            d.append(f"<h2>{html.escape(e.rubrik)}</h2>")
        if e.text:
            d.append(f"<p>{html.escape(e.text)}</p>")
        if e.figur:
            d.append(figur(e.figur).svg())
        if e.steg:
            d.append("<ol>" + "".join(f"<li>{html.escape(s)}</li>" for s in e.steg) + "</ol>")
        d.append("</div>")
    if k.galler:
        d.append(f'<div class="galler"><b>Gäller:</b> {html.escape(k.galler)}</div>')
    if k.galler_inte:
        d.append(f'<div class="galler-inte"><b>Slutar gälla:</b> {html.escape(k.galler_inte)}</div>')
    if k.kalla:
        d.append(f'<div class="kalla">Bygger på: {html.escape(k.kalla)}</div>')
    d.append("</section>")
    return "\n".join(d)


def bok_html(kort: list[Kort], titel: str = "Regelboken") -> str:
    return (f'<!doctype html><html lang="sv"><head><meta charset="utf-8"><title>{html.escape(titel)}</title>'
            f"<style>{STIL}</style></head><body>\n" + "\n".join(kort_html(k) for k in kort) + "\n</body></html>")

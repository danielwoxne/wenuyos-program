// Matte – fönstret. WenuyOS fönstergrund (libwenuy): sidopanel med stadier, områden och kort;
// innehållet visar hem, ett regelkort, övningen, placeringstestet eller lärarens översikt.
//
// Färger: ytorna följer temat (ljust och mörkt) – inga fasta vita bakgrunder med temats textfärg.
// Tonade rutor (gäller, slutar gälla, lösning) är halvgenomskinliga och ärver textfärgen, och
// figurerna ritas på ett eget vitt papper, så att allt syns i båda lägena.
#include "Bedom.h"
#include "Data.h"
#include "Framsteg.h"
#include "Flytt.h"
#include "Lara.h"
#include "Logg.h"
#include "Papper.h"
#include "Rita.h"
#include "Svar.h"
#include "Tal.h"
#include "Utskrift.h"
#include "Uppl.h"
#include "BrakmodellGtk.h"
#include "LarandeUtforskaGtk.h"
#include "SteghjalpGtk.h"
#include "FramstegOversikt.h"
#include "HundrarutaGtk.h"
#include "LektionsytaGtk.h"
#include "Mattekarta.h"
#include "VisuellNiva.h"
#include "MattekartaGtk.h" /* MATTE13-UI-STEG7 */ /* MATTE13-UI-STEG6 */ /* MATTE13-UI-STEG5 */ /* MATTE13-UI-STEG4 */ /* MATTE13-UI-STEG3 */ /* MATTE13-UI-STEG2 */

#include <Wenuy.h>
#include <WenuyWidgets.h>
#include <WenuyIcon.h>

#include <algorithm>
#include <cctype>
#include <csignal>
#include <functional>
#include <memory>
#include <random>
#include <regex>
#include <set>

using namespace matte;
using wenuy::tr;

namespace {

const char* APP_ID = "se.wenuyos.app.matte";
const std::set<std::string> LEKLAGE_STADIER = {"forskola", "lgr22.1-3"};
const std::map<std::string, std::string> OMRADE_IKON = {
    {"tal", "kulram"}, {"algebra", "lika-med"}, {"geometri", "triangel"},
    {"sannolikhet", "tarning"}, {"samband", "linjediagram"}, {"problem", "gloidlampa"}};
const std::map<std::string, std::string> STADIUM_IKON = {
    {"forskola", "stapeltorn"}, {"lgr22.1-3", "kuber-gula"}, {"lgr22.4-6", "linjal"}, {"lgr22.7-9", "koordinatsystem"},
    {"gy25.1", "passare"}, {"gy25.2", "gradskiva"}, {"gy25.fortsattning", "raket"}};

// Stilen. Sidorna fyller fönstret som i Filer och Inställningar (inga svävande kort), rubrikerna följer
// systemets typografi och färgerna kommer från temat (@accent_*, @success_color, @error_color, @warning_color).
// Textstorlekarna skalas med zoomen (stilCss multiplicerar alla font-size).
const char* CSS = R"CSS(
.matte-yta { font-size: 15px; }
.matte-kort { padding: 4px 10px 28px; }
.matte-titel { font-weight: 800; font-size: 22px; padding: 16px 16px 6px; }
.matte-stadium { font-weight: 600; font-size: 13px; padding: 12px 6px 2px; }
.matte-sager { padding: 10px 16px 4px; font-size: 16px; }
.matte-rubrik { font-weight: 700; }
.matte-skriver { font-size: 17px; font-feature-settings: "tnum"; }
.matte-svag { opacity: 0.7; font-size: 13px; }
.matte-fraga { font-size: 21px; }
.matte-stor { font-size: 24px; font-weight: 800; }
.matte-galler { margin: 10px 16px 0; padding: 8px 12px; border-radius: 8px; background: alpha(@accent_bg_color, 0.12);
                border-left: 4px solid @accent_bg_color; }
.matte-galler-inte { margin: 8px 16px 0; padding: 8px 12px; border-radius: 8px; background: alpha(@error_color, 0.10);
                     border-left: 4px solid @error_color; }
.matte-minns, .matte-losning { margin: 8px 16px 0; padding: 8px 12px; border-radius: 8px; background: alpha(@accent_bg_color, 0.10);
                               border-left: 4px solid alpha(@accent_bg_color, 0.7); }
.matte-steg { margin: 0 16px; padding: 10px 14px; border-radius: 8px; border: 1px dashed alpha(currentColor, 0.4);
              font-size: 18px; font-feature-settings: "tnum"; background: alpha(currentColor, 0.03); }
.matte-exempel { margin: 14px 16px 0; padding: 10px 14px; border-radius: 10px; background: alpha(currentColor, 0.04); }
.ratt { color: @success_color; font-weight: 600; }
.fel { color: @error_color; }
entry.fel { outline: 2px solid @error_color; }
button.val { font-size: 19px; padding: 10px 22px; min-width: 80px; }
button.val.ratt { background: alpha(@success_color, 0.22); }
button.val.fel { background: alpha(@error_color, 0.18); }
.svarsfalt { font-size: 19px; }
.matte-fira { background: alpha(@accent_bg_color, 0.14); border: 1px solid alpha(@accent_bg_color, 0.6); border-radius: 12px;
              padding: 10px 16px; margin: 0 16px; }
.matte-fira-rubrik { font-weight: 800; font-size: 18px; }
button.matte-ny { box-shadow: inset 0 0 0 2px @accent_bg_color; }
.lek .matte-fraga { font-size: 30px; font-weight: 600; }
.lek .matte-kort button, .lek .matte-pass button { font-size: 20px; min-height: 52px; padding: 8px 22px; }
.lek .matte-kort button.circular { padding: 8px; }
.lek .svarsfalt { font-size: 28px; min-height: 56px; }
.lek .matte-sager { font-size: 20px; }
.lek .matte-titel { font-size: 26px; }
.lek .matte-steg { font-size: 26px; }
.lek .matte-svag { font-size: 16px; }
checkbutton.kryssa { padding: 6px 12px 6px 8px; border-radius: 10px; border: 1px solid alpha(currentColor, 0.15); }
checkbutton.kryssa.fel { background: alpha(@error_color, 0.16); }
checkbutton.kryssa.ratt { background: alpha(@success_color, 0.2); }
checkbutton.kryssa.facit { box-shadow: inset 0 0 0 3px @success_color; }
.kryssa-text { font-size: 19px; font-feature-settings: "tnum"; }
.lek .kryssa-text { font-size: 26px; }
.lek checkbutton.kryssa check { min-width: 26px; min-height: 26px; }
button.penna { padding: 4px; border-radius: 999px; min-width: 0; min-height: 0; }
button.penna:checked { box-shadow: inset 0 0 0 3px @accent_bg_color; }
.matte-teckning { padding: 6px; }
/* Fokusläget: menyn visar bara passet */
.matte-pass { padding: 18px 16px; }
.matte-siffror button { font-size: 22px; font-weight: 600; }
.matte-stjarnor { color: @warning_color; font-size: 20px; letter-spacing: 2px; }
.matte-pass-titel { font-weight: 800; font-size: 17px; }
.matte-prickar { font-size: 17px; letter-spacing: 2px; }
.matte-fragarad { margin: 14px 16px 4px; }
button.matte-fortsatt { padding: 14px 22px; font-size: 18px; font-weight: 700; }
.matte-hej { font-size: 30px; font-weight: 800; }
.matte-paus { padding: 30px 26px; }
.lek .matte-prickar { font-size: 24px; }
/* Lättläst (stöd vid dyslexi): större text, mer luft mellan bokstäver och rader, korta rader, varm bakgrund */
.lattlast .matte-kort label, .lattlast .matte-kort entry, .lattlast .matte-kort button { letter-spacing: 0.05em; }
.lattlast .matte-fraga { font-size: 26px; line-height: 1.7; }
.lattlast .matte-steg { font-size: 24px; line-height: 1.6; }
.lattlast .svarsfalt { font-size: 26px; min-height: 52px; }
.lattlast .matte-sager, .lattlast .matte-skriver { font-size: 20px; line-height: 1.6; }
.lattlast .matte-losning, .lattlast .matte-galler, .lattlast .matte-galler-inte, .lattlast .matte-minns,
.lattlast .matte-exempel { font-size: 19px; line-height: 1.6; }
.lattlast button.val { font-size: 24px; }
.lattlast .kryssa-text { font-size: 24px; }
.lattlast .ratt, .lattlast .fel { font-size: 20px; }
.lattlast .matte-yta { background: color-mix(in srgb, var(--window-bg-color) 88%, #f2dca6); }
/* Sidomenyn */
.matte-meny-rubrik { font-weight: 700; font-size: 12px; opacity: 0.6; margin: 14px 14px 4px; }
.matte-avatar { background: @accent_bg_color; color: @accent_fg_color; border-radius: 999px; min-width: 36px; min-height: 36px;
                font-weight: 700; font-size: 16px; }
button.matte-elevknapp { padding: 6px 10px; margin: 8px 8px 0; }
.matte-elevnamn { font-weight: 700; font-size: 15px; }
.matte-omrade-rad { font-weight: 600; font-size: 14px; }
.matte-kortrad { font-size: 14px; }
.matte-nasta { background: alpha(@accent_bg_color, 0.18); color: @accent_color; border-radius: 999px; padding: 1px 8px;
               font-size: 11px; font-weight: 700; }
.matte-stjarnor-liten { color: @warning_color; font-size: 11px; }
.matte-stadieval { margin: 0 10px 6px; }
.lek .matte-kortrad, .lek .matte-omrade-rad { font-size: 17px; }
.lattlast .matte-kortrad, .lattlast .matte-omrade-rad { font-size: 16px; letter-spacing: 0.03em; }
/* Kortsidan: verktygsraden, regeln, uppgiften, räknepappret */
.matte-verktyg { padding: 4px 8px; }
.matte-verktyg button { padding: 4px 10px; }
.matte-zoom { font-feature-settings: "tnum"; min-width: 44px; font-size: 13px; }
.matte-uppg-huvud { padding: 14px 16px 6px; }
.matte-uppg-nr { font-weight: 800; font-size: 20px; }
.matte-delning { margin: 14px 16px 0; }
.matte-sektion { font-weight: 700; font-size: 14px; opacity: 0.8; margin: 14px 16px 0; }
.matte-sektion-tips { font-size: 12px; opacity: 0.6; margin: 0 16px; }
entry.matte-lucka { font-size: 19px; font-feature-settings: "tnum"; }
.matte-steg-rad { min-height: 40px; }
.lattlast entry.matte-lucka { font-size: 24px; min-height: 48px; }
.lek entry.matte-lucka { font-size: 26px; min-height: 52px; }
.matte-papper-kolumn { padding: 12px 14px 14px 10px; }
.matte-papper-inne { margin: 4px 16px 0; }
.matte-papper-verktyg button { padding: 3px 9px; font-size: 13px; }
.matte-papper-verktyg button.matte-stall-upp { font-weight: 700; color: @accent_color; }
/* Hem och kartan: rutor i systemets kortstil (.card) */
.matte-ruta { padding: 16px 18px; }
.matte-ruta-rubrik { font-weight: 800; font-size: 16px; }
.matte-ruta-tal { font-weight: 800; font-size: 34px; font-feature-settings: "tnum"; }
.matte-kartruta { padding: 12px 14px; }
.matte-kartruta.klar { background: alpha(@success_color, 0.10); }
.matte-kartruta.pagar { box-shadow: inset 0 0 0 2px alpha(@accent_bg_color, 0.55); }
.matte-kartruta.nasta { background: alpha(@accent_bg_color, 0.12); box-shadow: inset 0 0 0 2px @accent_bg_color; }
.matte-chip { border-radius: 999px; padding: 1px 8px; font-size: 11px; font-weight: 700; }
.matte-chip.klar { background: alpha(@success_color, 0.2); color: @success_color; }
.matte-chip.pagar { background: alpha(@accent_bg_color, 0.16); color: @accent_color; }
.matte-chip.nasta { background: @accent_bg_color; color: @accent_fg_color; }
.matte-chip.ny { background: alpha(currentColor, 0.08); }
.matte-karta-omrade { font-weight: 800; font-size: 17px; margin: 18px 16px 6px; }

/* MATTE13-UI-STEG1: Modernare gränssnitt, anpassat efter WenuyOS-temat */
.matte13-window .matte13-sidebar {
    padding: 8px 4px;
    background: alpha(@view_bg_color, 0.85);
}
.matte13-sidebar .navigation-sidebar row {
    border-radius: 10px;
    margin: 2px 5px;
    min-height: 38px;
}
.matte13-sidebar .navigation-sidebar row:selected {
    background: alpha(@accent_bg_color, 0.18);
    color: @window_fg_color;
}
.matte13-window .matte13-hero {
    margin: 10px 16px 4px;
    padding: 22px 22px;
    border-radius: 20px;
    border: 1px solid alpha(@accent_bg_color, 0.25);
    background: alpha(@accent_bg_color, 0.11);
}
.matte13-hero-title { font-size: 25px; font-weight: 800; }
.matte13-hero-sub { font-size: 15px; color: alpha(@window_fg_color, 0.85); }
.matte13-hero button.suggested-action {
    min-height: 46px;
    padding: 8px 20px;
    border-radius: 12px;
    font-weight: 700;
}
.matte13-dashboard .matte-ruta {
    border-radius: 16px;
    border: 1px solid alpha(@window_fg_color, 0.09);
    padding: 18px 20px;
}
.matte13-dashboard .matte-ruta-rubrik { font-size: 17px; }
.matte13-dashboard .matte-ruta button { min-height: 42px; border-radius: 10px; }
.matte13-map .matte-kartruta {
    border-radius: 14px;
    min-height: 104px;
    border: 1px solid alpha(@window_fg_color, 0.12);
    padding: 16px;
}
.matte13-map .matte-kartruta.nasta {
    border: 2px solid @accent_bg_color;
}
.matte13-map .matte-kartruta.klar {
    border: 1px solid alpha(@success_color, 0.45);
}
.matte13-exercise { padding: 10px 6px 20px; }
.matte13-exercise .matte-fragarad {
    padding: 14px 16px;
    border-radius: 14px;
    border: 1px solid alpha(@window_fg_color, 0.1);
    background: alpha(@accent_bg_color, 0.07);
}
.matte13-exercise .matte-fraga { font-size: 24px; font-weight: 750; }
.matte13-exercise .matte-steg { border-radius: 12px; }
.matte13-exercise .matte-losning { border-radius: 12px; padding: 14px 16px; }
.matte13-exercise .matte-figur { padding: 6px; }
.matte13-exercise button.suggested-action { min-height: 46px; border-radius: 12px; }
.matte13-toolbar { padding: 6px 10px; }
.matte13-toolbar button { min-height: 34px; border-radius: 9px; }
.matte13-exercise button:focus-visible,
.matte13-map button:focus-visible,
.matte13-hero button:focus-visible {
    outline: 3px solid @accent_bg_color;
    outline-offset: 2px;
}
/* Lässtöd: ingen lågkontrasttext; eleven kan förstora utan att texten försvinner. */
.lattlast .matte13-hero-sub, .lattlast .matte-svag { opacity: 1; color: @window_fg_color; }
.lattlast .matte13-exercise .matte-fraga { font-size: 28px; }
.lattlast .matte13-hero-title { font-size: 28px; }
.lattlast .matte13-hero button { min-height: 52px; }


/* MATTE13-UI-STEG2: pedagogisk bråkmodell och passets framsteg. */
.matte13-exercise .matte13-progress {
    min-height: 7px;
    margin: 0 16px 6px;
    border-radius: 999px;
}
.matte13-exercise .matte13-progress trough,
.matte13-exercise .matte13-progress progress { border-radius: 999px; }
.matte13-brakpanel {
    margin: 6px 16px 8px;
    border: 1px solid alpha(@window_fg_color, 0.15);
    border-radius: 14px;
    padding: 12px;
    background: alpha(@accent_bg_color, 0.05);
}
.matte13-brakpanel:focus-visible { outline: 3px solid @accent_bg_color; }
.matte13-brakpanel spinbutton { min-height: 42px; min-width: 68px; }
.matte13-brakresultat { font-weight: 800; font-size: 22px; }
.lattlast .matte13-brakpanel spinbutton { min-height: 52px; }
.lattlast .matte13-brakresultat { font-size: 26px; }


/* MATTE13-UI-STEG3: Laborationsytor och successiva steg; temafärger från WenuyOS. */
.matte13-laboration, .matte13-steghjalp {
    margin: 8px 16px 6px;
    padding: 12px 14px;
    border: 1px solid alpha(@window_fg_color, 0.16);
    border-radius: 14px;
    background: alpha(@accent_bg_color, 0.045);
}
.matte13-laboration:focus-visible, .matte13-steghjalp:focus-visible {
    outline: 3px solid @accent_bg_color;
}
.matte13-laboration spinbutton, .matte13-laboration dropdown,
.matte13-laboration scale, .matte13-steghjalp button {
    min-height: 44px;
}
.matte13-labresultat { font-size: 18px; font-weight: 750; }
.matte13-hjalptext { font-size: 18px; line-height: 1.5; }
.lattlast .matte13-labresultat, .lattlast .matte13-hjalptext {
    font-size: 23px;
    line-height: 1.7;
}
.lattlast .matte13-steghjalp button, .lattlast .matte13-laboration spinbutton {
    min-height: 54px;
}


/* MATTE13-UI-STEG4: översikt med tydlig semantik och responsiva kort. */
.matte13-dashboard-progress {
    margin: 8px 16px 8px;
    padding: 16px 18px;
    border-radius: 16px;
    border: 1px solid alpha(@window_fg_color, 0.12);
    background: alpha(@view_bg_color, 0.94);
}
.matte13-progress-heading { font-size: 20px; font-weight: 800; }
.matte13-progress-stat {
    min-width: 138px;
    border-radius: 12px;
    border: 1px solid alpha(@window_fg_color, 0.09);
    padding: 12px 16px;
    background: alpha(@accent_bg_color, 0.05);
}
.matte13-progress-stat-num { font-size: 26px; font-weight: 800; }
.matte13-progress-stat-name { font-size: 14px; }
.matte13-progress-bar { min-height: 12px; }
.matte13-progress-actions button { min-height: 42px; border-radius: 10px; }
.lattlast .matte13-progress-heading { font-size: 26px; }
.lattlast .matte13-progress-stat-name { font-size: 18px; }
.lattlast .matte13-progress-actions button { min-height: 54px; }


/* MATTE13-UI-STEG5: hundraruta, temaanpassad yta och kontrast även vid lässtöd. */
.matte13-hundraruta {
    margin: 8px 16px 8px;
    padding: 12px 14px;
    border-radius: 14px;
    border: 1px solid alpha(@window_fg_color, 0.20);
    background: alpha(@accent_bg_color, 0.05);
}
.matte13-hundraruta-resultat { font-size: 18px; font-weight: 600; line-height: 1.5; }
.matte13-hundraruta spinbutton, .matte13-hundraruta button { min-height: 44px; }
.matte13-hundraruta:focus-visible { outline: 3px solid @accent_bg_color; }
.lattlast .matte13-hundraruta-resultat { font-size: 23px; line-height: 1.7; }
.lattlast .matte13-hundraruta button,
.lattlast .matte13-hundraruta spinbutton { min-height: 54px; }


/* MATTE13-UI-STEG6: samlad, lättläst och skärmstorleksoberoende arbetsyta. */
.matte13-lektionsyta {
  margin: 10px 16px 10px;
  padding: 16px;
  border-radius: 16px;
  border: 1px solid alpha(@window_fg_color, 0.18);
  background: alpha(@view_bg_color, 0.95);
}
.matte13-lektionsyta-rubrik { font-size: 21px; font-weight: 800; }
.matte13-lektionsyta-info { font-size: 15px; line-height: 1.5; opacity: 0.86; }
.matte13-lektionsyta-val { min-width: 180px; min-height: 44px; }
.matte13-lektionsyta .matte13-brakpanel,
.matte13-lektionsyta .matte13-hundraruta,
.matte13-lektionsyta .matte13-laboration,
.matte13-lektionsyta .matte13-steghjalp { margin: 0; border: none; box-shadow: none; }
.lattlast .matte13-lektionsyta-rubrik { font-size: 26px; }
.lattlast .matte13-lektionsyta-info { font-size: 20px; line-height: 1.7; }
.lattlast .matte13-lektionsyta-val { min-height: 54px; }


/* MATTE13-UI-STEG7: Läsbar, responsiv lärandekarta med synliga statusord. */
.matte13-kartaoversikt {
  margin: 16px 16px 8px;
  padding: 18px;
  border-radius: 18px;
  border: 1px solid alpha(@window_fg_color, 0.16);
  background: alpha(@view_bg_color, 0.96);
}
.matte13-kartaoversikt-titel { font-weight: 800; font-size: 23px; }
.matte13-kartaoversikt-beskrivning { font-size: 15px; line-height: 1.5; }
.matte13-kartaoversikt-stapel { min-height: 12px; }
.matte13-kartaoversikt-rad { margin-top: 4px; }
.matte13-kartastat { padding: 10px 16px; border-radius: 12px; min-width: 116px;
  border: 1px solid alpha(@window_fg_color, 0.12); background: alpha(@window_fg_color, 0.04); }
.matte13-kartastat.klar { background: alpha(@success_color, 0.10); }
.matte13-kartastat.pagar { background: alpha(@accent_bg_color, 0.10); }
.matte13-kartastat.repetition { background: alpha(@warning_color, 0.10); }
.matte13-kartastat-tal { font-weight: 800; font-size: 26px; font-feature-settings: "tnum"; }
.matte13-kartastat-rubrik { font-size: 14px; }
.matte13-kartafilter { margin: 12px 16px 12px; }
.matte13-kartafilter dropdown { min-height: 44px; min-width: 210px; }
button.matte13-vagkort { min-height: 124px; border-radius: 14px;
  border: 1px solid alpha(@window_fg_color, 0.16); }
button.matte13-vagkort:focus-visible { outline: 3px solid @accent_bg_color; }
.matte13-vagpunkt { font-size: 18px; color: @accent_color; }
.matte13-karta-tom { margin: 16px; padding: 22px; border-radius: 12px;
  background: alpha(@accent_bg_color, 0.08); }
.lattlast .matte13-kartaoversikt-titel { font-size: 28px; }
.lattlast .matte13-kartaoversikt-beskrivning { font-size: 20px; line-height: 1.6; }
.lattlast button.matte13-vagkort { min-height: 160px; }
.lattlast .matte13-kartafilter dropdown { min-height: 56px; }


/* MATTE13-UI-STEG8 */
/* Sammanhängande WenuyOS-stil med låg grafisk belastning. */
.matte13-hero {
  border-radius: 18px;
  border: 1px solid alpha(@accent_bg_color, 0.20);
  background: alpha(@accent_bg_color, 0.09);
  padding: 18px;
}
.matte13-hero-title { font-weight: 800; }
.matte-ruta {
  border-radius: 16px;
  border: 1px solid alpha(@window_fg_color, 0.13);
  background: alpha(@view_bg_color, 0.96);
  padding: 18px;
}
.matte-ruta-rubrik { font-size: 17px; font-weight: 750; }
.matte-uppg-huvud { border-bottom: 1px solid alpha(@window_fg_color, 0.10); padding-bottom: 12px; }
.matte-fragarad { margin-top: 18px; margin-bottom: 10px; }
.matte13-lektionsyta {
  margin: 14px 16px;
  padding: 18px;
  border-radius: 17px;
  border: 1px solid alpha(@window_fg_color, 0.14);
  background: alpha(@view_bg_color, 0.96);
}
.matte13-lektionsyta-rubrik { font-weight: 800; font-size: 20px; }
.matte13-lektionsyta-info { font-size: 15px; line-height: 1.5; }
.matte13-lektionsyta-val { min-height: 46px; min-width: 195px; }
.matte13-brakresultat, .matte13-hundraruta-resultat, .matte13-labresultat {
  font-size: 17px; font-weight: 700; padding: 8px 0;
}
.matte13-kartaoversikt {
  border-radius: 18px;
  border: 1px solid alpha(@window_fg_color, 0.14);
  background: alpha(@view_bg_color, 0.96);
}
button.matte13-vagkort {
  border-radius: 16px;
  border: 1px solid alpha(@window_fg_color, 0.17);
  background: alpha(@view_bg_color, 0.95);
}
button.matte13-vagkort:hover { background: alpha(@accent_bg_color, 0.07); }
button.matte13-vagkort:focus-visible { outline: 3px solid @accent_bg_color; }
.lattlast .matte13-lektionsyta-info { font-size: 19px; line-height: 1.7; }
.lattlast .matte13-lektionsyta-val { min-height: 56px; }
.lattlast .matte-ruta-rubrik { font-size: 20px; }

/* Kursanpassat uttryck: endast presentation, aldrig pedagogiskt innehåll.
   Färger är alltid systemets tema – inga nya hårdkodade UI-färger. */
.matte13-window.matte-visuell-lekfull .matte13-hero {
    border-radius: 20px; padding: 22px; background: alpha(@accent_bg_color, 0.12);
}
.matte13-window.matte-visuell-lugn .matte13-hero {
    border-radius: 12px; padding: 16px; background: alpha(@accent_bg_color, 0.05);
}
.matte13-window.matte-visuell-lugn .matte13-dashboard .matte-ruta {
    border-radius: 10px; padding: 14px 16px;
}
.matte13-window.matte-visuell-akademisk .matte13-hero {
    border-radius: 7px; padding: 12px 16px; background: transparent;
    border: 1px solid alpha(currentColor, 0.12);
}
.matte13-window.matte-visuell-akademisk .matte13-dashboard .matte-ruta {
    border-radius: 8px; padding: 12px 16px;
}
.matte13-window.matte-visuell-akademisk .matte13-exercise .matte-fragarad {
    border-radius: 7px; background: transparent;
}
.matte13-window.matte-visuell-akademisk .matte-fira {
    border-radius: 6px; background: alpha(@success_color, 0.06);
}
.matte13-window.matte-visuell-professionell .matte13-hero {
    border: 0; border-radius: 0; padding: 8px 16px; background: transparent;
}
.matte13-window.matte-visuell-professionell .matte13-dashboard .matte-ruta {
    border-radius: 5px; padding: 10px 14px;
}
.matte13-window.matte-visuell-professionell .matte13-map .matte-kartruta {
    border-radius: 5px; min-height: 84px; padding: 10px;
}
.matte13-window.matte-visuell-professionell .matte13-exercise .matte-fragarad {
    border-radius: 4px; background: transparent; padding: 10px 16px;
}
.matte13-window.matte-visuell-professionell .matte-fira {
    border-radius: 4px; background: transparent;
}
)CSS";

// Stilen med textstorlekarna i övningen gånger z. Menyn, verktygsraden och passpanelen behåller sin storlek.
std::string stilCss(double z) {
    static const std::regex re(R"(font-size:\s*(\d+)px)");
    static const char* fasta[] = {"matte-meny", "matte-avatar", "matte-elev", "matte-omrade-rad", "matte-kortrad", "matte-nasta",
                                  "matte-stjarnor-liten", "matte-stadieval", "matte-pass", "matte-verktyg", "matte-zoom"};
    std::string css = CSS, ut;
    size_t a = 0;
    while (a < css.size()) {   // regel för regel: "selektor { … }"
        size_t b = css.find('}', a);
        std::string regel = css.substr(a, b == std::string::npos ? std::string::npos : b - a + 1);
        a = b == std::string::npos ? css.size() : b + 1;
        std::string sel = regel.substr(0, regel.find('{'));
        bool fast = false;
        for (const char* f : fasta) fast |= sel.find(f) != std::string::npos;
        if (fast || z == 1.0) { ut += regel; continue; }
        size_t sist = 0;
        for (auto it = std::sregex_iterator(regel.begin(), regel.end(), re); it != std::sregex_iterator(); ++it) {
            ut += regel.substr(sist, it->position() - sist);
            ut += "font-size: " + std::to_string((int)std::lround(std::stoi((*it)[1]) * z)) + "px";
            sist = it->position() + it->length();
        }
        ut += regel.substr(sist);
    }
    return ut;
}

// ---- små byggstenar ----------------------------------------------------------------------------

GtkWidget* lbl(const std::string& text, const char* klass = nullptr, bool wrap = true) {
    GtkWidget* l = gtk_label_new(text.c_str());
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    if (wrap) { gtk_label_set_wrap(GTK_LABEL(l), TRUE); gtk_label_set_wrap_mode(GTK_LABEL(l), PANGO_WRAP_WORD_CHAR); }
    if (klass) gtk_widget_add_css_class(l, klass);
    return l;
}

GtkWidget* marg(GtkWidget* w, int start = 16, int topp = 0, int slut = 16) {
    gtk_widget_set_margin_start(w, start);
    gtk_widget_set_margin_end(w, slut);
    if (topp) gtk_widget_set_margin_top(w, topp);
    return w;
}

void klick(GtkWidget* knapp, std::function<void()> f) {
    auto* fp = new std::function<void()>(std::move(f));
    g_signal_connect_data(knapp, "clicked", CB(+[](GtkButton*, gpointer p) { (*static_cast<std::function<void()>*>(p))(); }), fp,
                          +[](gpointer p, GClosure*) { delete static_cast<std::function<void()>*>(p); }, (GConnectFlags)0);
}

GtkWidget* knapp(const std::string& text, bool huvud, std::function<void()> f, const char* ikon = nullptr) {
    GtkWidget* b = gtk_button_new_with_label(text.c_str());
    if (ikon) {
        GtkWidget* c = adw_button_content_new();
        adw_button_content_set_label(ADW_BUTTON_CONTENT(c), text.c_str());
        adw_button_content_set_icon_name(ADW_BUTTON_CONTENT(c), ikon);
        gtk_button_set_child(GTK_BUTTON(b), c);
    }
    if (huvud) gtk_widget_add_css_class(b, "suggested-action");
    klick(b, std::move(f));
    return b;
}

void tom(GtkWidget* box) {
    while (GtkWidget* c = gtk_widget_get_first_child(box)) gtk_box_remove(GTK_BOX(box), c);
}

GtkWidget* vbox(int mellan = 10) { return gtk_box_new(GTK_ORIENTATION_VERTICAL, mellan); }
GtkWidget* hbox(int mellan = 8) { return gtk_box_new(GTK_ORIENTATION_HORIZONTAL, mellan); }

GtkWidget* rull(GtkWidget* barn = nullptr) {
    GtkWidget* s = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(s), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(s, TRUE);
    if (barn) gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(s), barn);
    return s;
}

// Kortet centreras och blir inte bredare än det går att läsa
GtkWidget* kortyta() {
    GtkWidget* b = vbox(10);
    gtk_widget_add_css_class(b, "matte-kort");
    GtkWidget* klamp = adw_clamp_new();
    adw_clamp_set_maximum_size(ADW_CLAMP(klamp), 1240);
    adw_clamp_set_tightening_threshold(ADW_CLAMP(klamp), 1240);
    adw_clamp_set_child(ADW_CLAMP(klamp), b);
    g_object_set_data(G_OBJECT(klamp), "innehall", b);
    return klamp;
}
GtkWidget* inre(GtkWidget* klamp) { return static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(klamp), "innehall")); }


// ---- programmet ---------------------------------------------------------------------------------

struct Matte {
    struct Lasdel { GtkWidget* l; std::string text; bool fet; };
    Data data;
    Elev elev;
    std::mt19937 rnd{std::random_device{}()};
    GtkWidget* win = nullptr;
    GtkWidget* lista = nullptr;
    GtkWidget* stack = nullptr;
    GtkWidget *hemRull, *kortRull, *ovaRull, *placRull, *oversiktRull, *pausRull;
    GtkWidget* sidoLista = nullptr;          // sidopanelen: elev, navigering, kursen …
    GtkWidget* sidoPass = nullptr;           // … eller bara passet (fokusläget)
    GtkWidget *passIkon, *passTitel, *passPrickar, *passText, *passRegel, *passSluta;
    GtkWidget* ovaKlamp = nullptr;
    GtkWidget* ovaPassbar = nullptr;
    GtkWidget* ovaBrakModell = nullptr;
    GtkWidget* ovaTallinje = nullptr;
    GtkWidget* ovaGeometri = nullptr;
    GtkWidget* ovaSteghjalp = nullptr;
    GtkWidget* ovaHundraruta = nullptr;
    GtkWidget* ovaArbetsyta = nullptr;
    int passGjorda = 0, passRatt = 0, passMal = 10;
    std::vector<std::string> passKort;
    bool franPass = false;                   // kortet visas mitt i ett pass – "Tillbaka till uppgiften"
    const Kort* franKort = nullptr;       // "Dagens tre": ett kort per uppgift (tom = vanligt pass på ett kort)
    void borjaDagens();
    std::string stjarntext(int n) const { std::string r; for (int i = 0; i < 3; ++i) r += i < n ? "★" : "☆"; return r; }
    gint64 uppgiftBorjade = 0;               // för loggen: hur länge eleven arbetat med uppgiften
    bool uppgiftKlar = false;                // rätt svar eller sparad teckning – annars är det ett avbrott om man går
    Json uppgiftsinfo() const;               // kort, mall, typ, nivå, sekunder – till loggen
    void loggaAvbrott();
    // sidomenyn
    GtkWidget* elevKnapp = nullptr;          // vem som övar (byt elev, ny elev)
    GtkWidget* navLista = nullptr;           // Hem, Min regelbok, Matteorden
    GtkWidget* navLista2 = nullptr;          // Inställningar, För läraren
    GtkWidget* stadieVal = nullptr;          // kursen: ett stadium i taget
    GtkWidget* kursRull = nullptr;
    std::vector<std::string> stadieIds;      // stadierna i stadieVal
    std::string valtStadium;                 // "" = det eleven är på
    std::set<std::string> oppnaOmraden, stangdaOmraden;   // "stadium/område" – öppnade och stängda för hand
    void byggSidomeny();
    void fyllElevKnapp();
    void markeraNav(const std::string& vy);
    std::string stadiumFor(const std::string& kortId) const;
    // kortsidan: regeln och uppgiften på samma sida
    GtkWidget *kortSida = nullptr, *regelRull = nullptr, *regelPlats = nullptr, *regelInline = nullptr, *uppgRull = nullptr,
              *uppgKort = nullptr, *regelVy = nullptr;
    bool smal = true;                        // smalt: regeln ovanför uppgiften, brett: bredvid
    bool regelHel = true;                    // hela regeln eller bara kärnan (smalt fönster)
    bool passAktivt = false;                 // ett pass pågår på kortet – tillbaka till kortet = samma uppgift
    bool fokusLage = false;                  // menyn göms medan man övar
    bool rord = false;                       // eleven har börjat på uppgiften (skrivit något)
    std::vector<Lasdel>* regelLas = nullptr; // det som läses upp i regeln
    std::shared_ptr<std::vector<Lasdel>> regelLasDelar;
    void visaRegel();
    void tillUppgiften();                    // smalt fönster: rulla så att uppgiften syns
    void satFokus(bool pa);
    void visaInstallningar();
    void visaRegelbok();
    GtkWidget *installRull = nullptr, *regelbokRull = nullptr;
    // uträkningen
    GtkWidget *ovaRakna = nullptr, *ovaKladd = nullptr, *ovaSvarRubrik = nullptr, *ovaNr = nullptr, *ovaPrickar = nullptr, *ovaFokusKnapp = nullptr;
    std::vector<GtkWidget*> luckor;
    void kopplaRord(GtkWidget* w);
    bool fyllerFokus = false;
    // verktygsraden, layouten, räknepappret, zoom, helskärm
    GtkWidget *verktyg = nullptr, *vRegel = nullptr, *vPapper = nullptr, *vFokus = nullptr, *vZoom = nullptr, *vHel = nullptr, *vLedtrad = nullptr;
    GtkWidget *kortBin = nullptr, *yttre = nullptr, *inrePaned = nullptr, *papperKolumn = nullptr, *papperDel = nullptr, *papperRull = nullptr,
              *papper = nullptr, *papperStatus = nullptr;
    bool papperByte = false, papperFelVisat = false, papperAterupptaVidStart = false;
    bool sparaPapper();
    int layout = 0;                          // 0 en kolumn · 1 uppgift | papper · 2 regel | uppgift | papper · 3 som 2, större
    bool regelKolumn = true;                 // brett: regeln i egen kolumn (Regeln i verktygsraden)
    int papperVal = -1;                      // -1 som uppgiften vill, 0 av, 1 på (Räknepapper i verktygsraden)
    bool papperVisas = false;
    double autoZoom = 1.0;
    bool helskarm = false;
    bool synkarVerktyg = false;
    GtkCssProvider* stil = nullptr;
    void byggVerktyg();
    void ordnaLayout(bool nyLayout);
    void synkaVerktyg();
    void tillampaZoom(bool omrita);
    void satHelskarm(bool pa);
    void visaUppgift();
    void ledtrad();
    std::vector<GtkWidget*> stilbyte;        // (oanvänd)
    void visaKarta();
    GtkWidget* kartaRull = nullptr;
    int kartaFilter = 0; // sessionens filter, elevdata ändras inte
    bool rostSaknasVisad = false;
    std::vector<std::string> ordning;            // alla kort i läroplanens ordning
    const Kort* aktuellt = nullptr;
    std::set<std::string> nyaBeharskade;
    bool fyller = false;

    // övningen
    Uppgift uppgift;
    Svarsyta svarsyta;
    int forsok = 0;
    std::string ritfil;
    bool tranar = false;                  // en feltanke tränas (ledtråden står framme)
    int tvingaMall = -1;                  // testkroken: visa just den här mallen                   // teckningen i den här uppgiften (sparas om vid nytt Klar)
    void sparaTeckning();
    GtkWidget *ovaTitel, *ovaFira, *ovaFiraText, *ovaFraga, *ovaSteg, *ovaFokus, *ovaFigur, *ovaSvar, *ovaSvarIkon, *ovaSvarText,
              *ovaLosning, *ovaTillbaka, *ovaNasta, *ovaNiva;

    // placeringen
    std::vector<Placering> placeringar;
    size_t placI = 0;
    Uppgift placUppgift;
    Svarsyta placSvarsyta;
    GtkWidget *placOmrade, *placFraga, *placSteg, *placFigur, *placSvar, *placText;

    std::string ikon(const std::string& namn) const { return data.ikon(namn); }
    GtkWidget* ikonbild(const std::string& namn, int px) const {
        GtkWidget* i = gtk_image_new_from_file(ikon(namn).c_str());
        gtk_image_set_pixel_size(GTK_IMAGE(i), px);
        return i;
    }
    void toast(const std::string& t) { wenuy_app_window_toast(win, t.c_str()); }
    void titel(const std::string& t) { wenuy_app_window_set_content_title(win, t.c_str()); }
    void visa(const char* sida) {
        std::string s = sida;
        if (s != "kort") loggaAvbrott();
        sidopanel((s == "kort" && fokusLage) || s == "placering");
        if (verktyg) gtk_widget_set_visible(verktyg, s == "kort");
        if (helskarm && s != "kort") satHelskarm(false);
        gtk_stack_set_visible_child_name(GTK_STACK(stack), sida);
        wenuy_app_window_show_content(win);
    }
    void lasUpp(const std::string& t) {
        if (!las(t) && !rostSaknasVisad) { rostSaknasVisad = true; toast(tr("No voice installed (espeak-ng)", "Ingen röst installerad (espeak-ng)")); }
    }
    bool lasesUpp() const { return elev.leklage || elev.lattlast; }   // frågor och svar läses upp av sig själva
    void sidopanel(bool fokus);
    void uppdateraPass();
    void visaPaus();
    std::string fragaMarkup(const std::string& fraga) const { return textMarkup(fraga, -1, true); }
    // Texten som markup: ordlistans ord som länkar, frågemeningen i fetstil, ord nr `markera` markerat (uppläsning).
    // ord (ut): orden i den ordning de läses upp.
    std::string textMarkup(const std::string& text, int markera = -1, bool fetFraga = false, std::vector<std::string>* ord = nullptr) const;
    void sattText(GtkWidget* l, const std::string& text, bool fetFraga = false, int markera = -1);
    void lankad(GtkWidget* l);              // klick på ett ordlisteord öppnar förklaringen
    void lasUppUppgift();
    // Läser upp flera etiketter i följd och markerar ordet som läses i den etikett det står i
    bool lasMarkerat(const std::vector<Lasdel>& delar, const std::vector<std::string>& extra = {});
    GtkWidget* lasbar(const std::string& text, const char* klass, std::vector<Lasdel>& delar, bool fet = false);
    void sifferknappar(GtkWidget* forvaring);
    void visaOrd(int i);
    void visaOrdlista();
    void visaElevvy(const std::string& namn);
    void exportera(const std::string& namn);
    void importera();
    void importeraFil(const std::string& fil, bool ersatt);
    GtkWidget* elevRull = nullptr;
    AdwDialog* ordDialog = nullptr;
    GtkWidget* ordlistaRull = nullptr;

    void bygg(GtkApplication* app);
    void tillampaLeklage();
    void fyllLista();
    void tillampaVisuellStil(const std::string& stadium = "");
    void uppdateraStatus();
    void visaHem();
    void bytElev(const std::string& namn);
    void visaKort(const Kort& k, bool nyttPass = false);
    void byggOvavy();
    void borjaOva();
    void nyUppgift();
    void kolla(const std::vector<std::string>& svar);
    void fira(const Kort& k);
    void byggPlaceringsvy();
    void borjaPlacering();
    void placNasta();
    void placSvara(const std::vector<std::string>* svar);
    void placAvsluta(bool hoppade);
    void visaOversikt();
    void visaTeckning(const std::string& fil, const std::string& rubrik, const std::string& fraga);
    std::string senastePdf;
    void pdfKlar(const std::string& fil, const std::string& text);
    void skrivArbetsblad(const Kort& k);
    void skrivRegelbok(bool hela);
    std::string nastaKort() const;
};

Matte* M = nullptr;

// ---- start ----------------------------------------------------------------------------------------

void Matte::bygg(GtkApplication* app) {
    wenuy::loadStyle("");
    stil = gtk_css_provider_new();
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(stil), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
    std::string fel;
    if (!data.las(Data::hittaMapp(), &fel)) g_warning("Matte: kunde inte läsa innehållet: %s", fel.c_str());
    elev = Elev::ladda(senasteElev());

    win = wenuy_app_window_new_with_sidebar(app, "Matte", WENUY_SIDEBAR_PAGES);
    gtk_widget_add_css_class(win, "matte13-window"); /* MATTE13-UI-STEG1 */
    gtk_window_set_default_size(GTK_WINDOW(win), 1100, 760);
    if (const char* st = g_getenv("MATTE_STORLEK")) {   // skärmbilderna: t.ex. 1800x900
        int b = 0, h = 0;
        if (sscanf(st, "%dx%d", &b, &h) == 2 && b > 300 && h > 300) gtk_window_set_default_size(GTK_WINDOW(win), b, h);
    }
    adw_header_bar_pack_end(ADW_HEADER_BAR(wenuy_app_window_get_header_bar(win)), wenuy_window_actions_button());

    byggSidomeny();

    // Passpanelen (fokusläget): kortet, hur långt man kommit, och vägen ut
    sidoPass = GTK_WIDGET(g_object_ref_sink(vbox(10)));
    gtk_widget_add_css_class(sidoPass, "matte-pass");
    passIkon = hbox(0);
    passTitel = lbl("", "matte-pass-titel");
    passPrickar = lbl("", "matte-prickar");
    passText = lbl("", "matte-svag");
    passRegel = knapp(tr("Show the menu", "Visa menyn"), false, [] { M->satFokus(false); }, "open-menu-symbolic");
    passSluta = knapp(tr("Finish", "Avsluta"), false, [] {
        const char* sida = gtk_stack_get_visible_child_name(GTK_STACK(M->stack));
        if (sida && std::string(sida) == "kort" && M->passGjorda > 0) M->visaPaus();
        else if (sida && std::string(sida) == "placering") M->placAvsluta(true);
        else { tyst(); M->satFokus(false); M->visaHem(); }
    });
    for (GtkWidget* w : {passIkon, passTitel, passPrickar, passText}) gtk_box_append(GTK_BOX(sidoPass), w);
    GtkWidget* fyll = vbox(0);
    gtk_widget_set_vexpand(fyll, TRUE);
    gtk_box_append(GTK_BOX(sidoPass), fyll);
    gtk_box_append(GTK_BOX(sidoPass), passRegel);
    gtk_box_append(GTK_BOX(sidoPass), passSluta);

    stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(stack), wenuy::lowProfile() ? 0 : 150);
    hemRull = rull(); oversiktRull = rull();
    gtk_stack_add_named(GTK_STACK(stack), hemRull, "hem");
    // Kortsidan: regeln, uppgiften och räknepappret. Bredden avgör (egen brytpunkt på sidan, inte fönstret):
    //   < 1000: allt i en kolumn · 1000–1500: uppgiften | pappret · ≥ 1500: regeln | uppgiften | pappret (dragbara)
    byggOvavy();
    regelPlats = vbox(0);
    regelRull = rull(regelPlats);
    gtk_widget_set_size_request(regelRull, 300, -1);
    regelInline = vbox(0);
    GtkWidget* uppgKolumn = vbox(0);
    gtk_box_append(GTK_BOX(uppgKolumn), regelInline);
    gtk_box_append(GTK_BOX(uppgKolumn), ovaKlamp);
    uppgRull = rull(uppgKolumn);
    gtk_widget_set_size_request(uppgRull, 380, -1);
    // räknepappret: rubrik, verktyg, pappret – flyttas mellan egen kolumn och uppgiften
    papper = raknepapper_new();
    papperDel = vbox(6);
    gtk_widget_add_css_class(papperDel, "matte-papper-del");
    GtkWidget* prub = hbox(8);
    gtk_box_append(GTK_BOX(prub), wenuy_icon_new("note", 20));
    GtkWidget* prl = lbl(tr("Working paper", "Räknepapper"), "matte-rubrik", false);
    gtk_box_append(GTK_BOX(prub), prl);
    GtkWidget* ptips = lbl(tr("– not marked, it is for you", "– rättas inte, det är för dig"), "matte-svag", false);
    gtk_widget_set_hexpand(ptips, TRUE);
    gtk_box_append(GTK_BOX(prub), ptips);
    papperStatus = lbl(tr("Not saved yet", "Inte sparat än"), "matte-svag", false);
    gtk_widget_set_valign(papperStatus, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(prub), papperStatus);
    gtk_box_append(GTK_BOX(papperDel), prub);
    raknepapper_andringar(papper,
        [] { if (M) M->sparaPapper(); },
        [] { if (M) M->toast(tr("Working paper is full. Undo or erase some strokes before drawing more.",
                                "Räknepappret är fullt. Ångra eller sudda några streck innan du ritar mer.")); });
    GtkWidget* pv = raknepapper_verktyg(papper);
    gtk_box_append(GTK_BOX(papperDel), pv);
    papperRull = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(papperRull), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(papperRull), papper);
    gtk_box_append(GTK_BOX(papperDel), papperRull);
    papperKolumn = vbox(0);
    gtk_widget_add_css_class(papperKolumn, "matte-papper-kolumn");
    gtk_widget_set_size_request(papperKolumn, 300, -1);
    inrePaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_start_child(GTK_PANED(inrePaned), uppgRull);
    gtk_paned_set_end_child(GTK_PANED(inrePaned), papperKolumn);
    gtk_paned_set_shrink_start_child(GTK_PANED(inrePaned), FALSE);
    gtk_paned_set_shrink_end_child(GTK_PANED(inrePaned), FALSE);
    yttre = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_start_child(GTK_PANED(yttre), regelRull);
    gtk_paned_set_end_child(GTK_PANED(yttre), inrePaned);
    gtk_paned_set_shrink_start_child(GTK_PANED(yttre), FALSE);
    gtk_paned_set_shrink_end_child(GTK_PANED(yttre), FALSE);
    gtk_paned_set_resize_start_child(GTK_PANED(yttre), FALSE);
    gtk_widget_set_visible(regelRull, FALSE);
    gtk_widget_set_visible(papperKolumn, FALSE);
    kortSida = yttre;
    GtkWidget* bin = adw_breakpoint_bin_new();
    kortBin = bin;
    gtk_widget_set_size_request(bin, 320, 200);
    adw_breakpoint_bin_set_child(ADW_BREAKPOINT_BIN(bin), yttre);
    for (const char* villkor : {"min-width: 1000sp", "min-width: 1500sp", "min-width: 2100sp"})
        adw_breakpoint_bin_add_breakpoint(ADW_BREAKPOINT_BIN(bin), adw_breakpoint_new(adw_breakpoint_condition_parse(villkor)));
    g_signal_connect(bin, "notify::current-breakpoint", CB(+[](AdwBreakpointBin* b, GParamSpec*, gpointer) {
        AdwBreakpoint* bp = adw_breakpoint_bin_get_current_breakpoint(b);
        int lage = 0;
        if (bp) {
            gchar* v = adw_breakpoint_condition_to_string(adw_breakpoint_get_condition(bp));
            std::string t = v;
            g_free(v);
            lage = t.find("2100") != std::string::npos ? 3 : t.find("1500") != std::string::npos ? 2 : 1;
        }
        if (lage != M->layout) { M->layout = lage; M->ordnaLayout(true); }
    }), nullptr);
    byggVerktyg();    gtk_stack_add_named(GTK_STACK(stack), bin, "kort");
    kartaRull = rull();
    gtk_stack_add_named(GTK_STACK(stack), kartaRull, "karta");
    byggPlaceringsvy();
    gtk_stack_add_named(GTK_STACK(stack), placRull, "placering");
    gtk_stack_add_named(GTK_STACK(stack), oversiktRull, "oversikt");
    pausRull = rull();
    gtk_stack_add_named(GTK_STACK(stack), pausRull, "paus");
    ordlistaRull = rull();
    gtk_stack_add_named(GTK_STACK(stack), ordlistaRull, "ordlista");
    elevRull = rull();
    gtk_stack_add_named(GTK_STACK(stack), elevRull, "elev");
    installRull = rull();
    gtk_stack_add_named(GTK_STACK(stack), installRull, "installningar");
    regelbokRull = rull();
    gtk_stack_add_named(GTK_STACK(stack), regelbokRull, "regelbok");
    gtk_widget_add_css_class(stack, "matte-yta");
    wenuy_app_window_set_content(win, stack);

    GtkEventController* tang = gtk_event_controller_key_new();
    g_signal_connect(tang, "key-pressed", CB(+[](GtkEventControllerKey*, guint key, guint, GdkModifierType st, gpointer) -> gboolean {
        const char* sida = gtk_stack_get_visible_child_name(GTK_STACK(M->stack));
        if ((st & GDK_CONTROL_MASK) && (key == GDK_KEY_plus || key == GDK_KEY_KP_Add || key == GDK_KEY_equal)) {
            M->elev.zoom = std::min(1.6, M->elev.zoom + 0.1); M->elev.spara(); M->tillampaZoom(true); return TRUE;
        }
        if ((st & GDK_CONTROL_MASK) && (key == GDK_KEY_minus || key == GDK_KEY_KP_Subtract)) {
            M->elev.zoom = std::max(0.8, M->elev.zoom - 0.1); M->elev.spara(); M->tillampaZoom(true); return TRUE;
        }
        if ((st & GDK_CONTROL_MASK) && (key == GDK_KEY_0 || key == GDK_KEY_KP_0)) { M->elev.zoom = 1.0; M->elev.spara(); M->tillampaZoom(true); return TRUE; }
        if (key == GDK_KEY_F11 && sida && std::string(sida) == "kort") { M->satHelskarm(!M->helskarm); return TRUE; }
        if (key == GDK_KEY_Escape && M->helskarm) { M->satHelskarm(false); return TRUE; }
        if (key == GDK_KEY_Escape && sida) {
            std::string s = sida;
            if (s == "kort" && M->fokusLage) { M->satFokus(false); return TRUE; }   // Esc: menyn tillbaka
            if (s == "placering" || s == "paus") { tyst(); M->visaHem(); return TRUE; }
            if (s == "kort") { tyst(); return TRUE; }   // tystar uppläsningen
        }
        if ((st & GDK_CONTROL_MASK) && (key == GDK_KEY_odiaeresis || key == GDK_KEY_Odiaeresis) && M->aktuellt) { M->borjaOva(); return TRUE; }
        return FALSE;
    }), nullptr);
    gtk_widget_add_controller(win, tang);

    g_signal_connect(win, "close-request",
        G_CALLBACK(+[](GtkWindow*, gpointer) -> gboolean {
            // Vid diskfel får fönstret inte stängas och kasta osparad handskrift.
            if (M && !M->sparaPapper()) return TRUE;
            return FALSE;  // lämna stängningen till GTK när sparning lyckats
        }), nullptr);
    tillampaLeklage();
    tillampaZoom(false);
    fyllElevKnapp();
    fyllLista();
    uppdateraStatus();
    visaHem();
    gtk_window_present(GTK_WINDOW(win));
}

void Matte::tillampaVisuellStil(const std::string& valdKurs) {
    if (!win) return;
    std::string stadium = valdKurs;
    if (stadium.empty()) stadium = !valtStadium.empty() ? valtStadium :
                                     aktuellt ? stadiumFor(aktuellt->id) :
                                     !stadieIds.empty() ? stadieIds.front() : "lgr22.4-6";
    static const char* klasser[] = {
        "matte-visuell-lekfull", "matte-visuell-lugn",
        "matte-visuell-akademisk", "matte-visuell-professionell"
    };
    for (const char* klass : klasser) gtk_widget_remove_css_class(win, klass);
    gtk_widget_add_css_class(win, visuellNivaKlass(visuellNiva(stadium, elev.visuellStil)));
}

void Matte::tillampaLeklage() {
    FIGURSKALA = (elev.leklage ? 1.5 : elev.lattlast ? 1.25 : 1.0) * elev.zoom * autoZoom;
    if (elev.leklage) gtk_widget_add_css_class(win, "lek"); else gtk_widget_remove_css_class(win, "lek");
    if (elev.lattlast) gtk_widget_add_css_class(win, "lattlast"); else gtk_widget_remove_css_class(win, "lattlast");
    TALTAKT = elev.lattlast || elev.leklage ? 115 : 140;          // lugnare uppläsning
    if (ovaKlamp) {   // kortare rader i lättläst
        adw_clamp_set_maximum_size(ADW_CLAMP(ovaKlamp), elev.lattlast ? 760 : 980);
        adw_clamp_set_tightening_threshold(ADW_CLAMP(ovaKlamp), elev.lattlast ? 760 : 980);
    }
    passMal = elev.leklage ? 6 : 10;
}

// Lagra en ögonblicksbild efter tangentändring eller färdigt pennstreck.
bool Matte::sparaPapper() {
    if (papperByte || !papper || !giltigtPapperId(elev.papperId)) return true;
    PapperData d = raknepapper_hamta(papper);
    if (d.id != elev.papperId || d.kort != elev.papperKort) return true;
    std::string fel;
    if (!papperSpara(elev.namn, d, &fel)) {
        if (papperStatus) gtk_label_set_text(GTK_LABEL(papperStatus),
            tr("Could not save", "Kunde inte spara"));
        if (!papperFelVisat) {
            papperFelVisat = true;
            toast(tr("Could not save the working paper. Your current work is still on screen.",
                     "Kunde inte spara räknepappret. Ditt arbete finns kvar på skärmen."));
        }
        g_warning("Matte: kunde inte spara räknepappret: %s", fel.c_str());
        return false;
    }
    papperFelVisat = false;
    if (papperStatus) gtk_label_set_text(GTK_LABEL(papperStatus), tr("Saved", "Sparat"));
    return true;
}

// ---- fokusläget -------------------------------------------------------------------------------------

Json Matte::uppgiftsinfo() const {
    Json j = Json::objekt();
    if (aktuellt) j.satt("kort", aktuellt->id);
    if (uppgift.mall) { j.satt("mall", uppgift.mallIndex); j.satt("typ", uppgift.mall->typ); j.satt("niva", uppgift.mall->niva); }
    if (uppgiftBorjade) j.satt("sek", (int)((g_get_monotonic_time() - uppgiftBorjade) / 1000000));
    return j;
}

// Eleven lämnar en uppgift utan att ha klarat den – det är ett av de tydligaste tecknen på att något skaver
void Matte::loggaAvbrott() {
    const char* sida = gtk_stack_get_visible_child_name(GTK_STACK(stack));
    if (!sida || std::string(sida) != "kort" || !uppgift.v || uppgiftKlar) return;
    if (forsok == 0 && !rord && passGjorda == 0) return;   // bara tittat på kortet – inget avbrott
    Json j = uppgiftsinfo();
    j.satt("forsok", forsok);
    logga(elev.namn, "avbrott", j);
    uppgiftKlar = true;   // bara en gång
}

void Matte::sidopanel(bool fokus) {
    GtkWidget* ska = fokus ? sidoPass : sidoLista;
    if (!gtk_widget_get_parent(ska)) wenuy_app_window_set_sidebar(win, ska);
}

void Matte::uppdateraPass() {
    if (ovaPassbar) {
        const double andel = passMal > 0 ? std::clamp((double)passGjorda / passMal, 0.0, 1.0) : 0.0;
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ovaPassbar), andel);
        gtk_widget_set_visible(ovaPassbar, passMal > 1);
    }
    std::string prickar;
    for (int i = 0; i < passMal; ++i) prickar += i < passGjorda ? "●" : "○";
    gtk_label_set_text(GTK_LABEL(passPrickar), prickar.c_str());
    std::string t = std::to_string(passGjorda) + tr(" of ", " av ") + std::to_string(passMal) + tr(" done", " klara");
    if (aktuellt) {
        int rad = elev.status(aktuellt->id).rattIRad;
        if (rad > 1) t += std::string("\n") + std::to_string(rad) + tr(" right in a row", " rätt i rad");
    }
    gtk_label_set_text(GTK_LABEL(passText), t.c_str());
    int nu = std::min(forsok > 0 ? passGjorda : passGjorda + 1, passMal);   // uppgiften man håller på med
    if (ovaNr) {
        gtk_label_set_text(GTK_LABEL(ovaNr), (std::string(tr("Task ", "Uppgift ")) + std::to_string(std::max(nu, 1)) + tr(" of ", " av ") +
                                              std::to_string(passMal)).c_str());
        gtk_label_set_text(GTK_LABEL(ovaPrickar), prickar.c_str());
    }
    {   // statusraden: var man är i passet, nivån, rätt i rad – och kortet med stjärnorna
        std::string st = std::string(tr("Task ", "Uppgift ")) + std::to_string(std::max(nu, 1)) + tr(" of ", " av ") + std::to_string(passMal);
        if (uppgift.mall) st += std::string(" · ") + tr("Level ", "Nivå ") + std::to_string(uppgift.mall->niva) + tr(" of 3", " av 3");
        if (aktuellt) {
            int rad = elev.status(aktuellt->id).rattIRad;
            if (rad > 1) st += " · " + std::to_string(rad) + tr(" right in a row", " rätt i rad");
        }
        std::string slut = aktuellt ? stjarntext(elev.stjarnor(aktuellt->id)) + "  " + aktuellt->titel : "";
        wenuy_app_window_set_status(win, st.c_str(), slut.empty() ? nullptr : slut.c_str());
    }
}

// Passet är klart: en paus med vad man gjort – och valet att fortsätta, inte en oändlig ström av uppgifter
void Matte::visaPaus() {
    tyst();
    loggaAvbrott();
    {
        Json j = Json::objekt();
        if (aktuellt) j.satt("kort", aktuellt->id);
        j.satt("gjorda", passGjorda);
        j.satt("ratt", passRatt);
        logga(elev.namn, "pass", j);
    }
    GtkWidget* klamp = kortyta();
    GtkWidget* box = inre(klamp);
    GtkWidget* inner = vbox(14);
    gtk_widget_add_css_class(inner, "matte-paus");
    GtkWidget* topp = hbox(14);
    gtk_box_append(GTK_BOX(topp), ikonbild(!passKort.empty() ? "kalender" : passRatt * 2 >= passGjorda ? "pokal" : "stjarna", 56));
    GtkWidget* rubrik = lbl(tr("Well done – time for a break!", "Bra jobbat – dags för en paus!"), "matte-stor");
    gtk_widget_set_hexpand(rubrik, TRUE);
    gtk_widget_set_valign(rubrik, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(topp), rubrik);
    gtk_box_append(GTK_BOX(inner), topp);
    bool dagens = !passKort.empty() && passGjorda >= passMal;
    if (dagens) { elev.dagensGjorda(); elev.spara(); }
    std::string t = std::string(tr("You did ", "Du gjorde ")) + std::to_string(passGjorda) + tr(" tasks", " uppgifter") + ". " +
                    std::to_string(passRatt) + tr(" were right the first time.", " var rätt på första försöket.");
    if (dagens) t += std::string(" ") + (elev.dagensSvit > 1 ? std::to_string(elev.dagensSvit) + tr(" days in a row!", " dagar i rad!")
                                                             : std::string(tr("Come back tomorrow for three new ones.", "Kom tillbaka i morgon för tre nya.")));
    if (aktuellt && elev.lage(aktuellt->id) == BEHARSKAR) t += std::string(" ") + tr("And you have mastered the card!", "Och du behärskar kortet!");
    gtk_box_append(GTK_BOX(inner), lbl(t, "matte-fraga"));
    gtk_box_append(GTK_BOX(inner), lbl(tr("Stretch, have some water, look out of the window. Then you choose.",
                                          "Sträck på dig, drick lite vatten, titta ut genom fönstret. Sedan väljer du."), "matte-svag"));
    GtkWidget* knappar = adw_wrap_box_new();
    adw_wrap_box_set_child_spacing(ADW_WRAP_BOX(knappar), 10);
    adw_wrap_box_set_line_spacing(ADW_WRAP_BOX(knappar), 10);
    if (!dagens) {
        GtkWidget* mer = knapp(tr("Five more", "Fem till"), true, [] { M->passMal = M->passGjorda + 5; M->passAktivt = true; M->visa("kort"); M->nyUppgift(); });
        gtk_widget_add_css_class(mer, "matte-fortsatt");
        adw_wrap_box_append(ADW_WRAP_BOX(knappar), mer);
    }
    GtkWidget* hem = knapp(dagens ? tr("Home", "Hem") : tr("Stop for today", "Sluta för i dag"), dagens, [] { M->passKort.clear(); M->satFokus(false); M->visaHem(); });
    gtk_widget_add_css_class(hem, "matte-fortsatt");
    adw_wrap_box_append(ADW_WRAP_BOX(knappar), hem);
    gtk_box_append(GTK_BOX(inner), knappar);
    gtk_box_append(GTK_BOX(box), inner);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(pausRull), klamp);
    passAktivt = false;
    markeraNav("");
    titel(tr("Break", "Paus"));
    gtk_label_set_text(GTK_LABEL(passText), tr("Break", "Paus"));
    visa("paus");
    if (lasesUpp()) lasUpp(std::string(tr("Well done – time for a break! ", "Bra jobbat – dags för en paus! ")) + t);
}

// ---- texten i uppgifterna: ordlistan, fetstil och markering ------------------------------------------

std::string Matte::textMarkup(const std::string& text, int markera, bool fetFraga, std::vector<std::string>* ord) const {
    struct Tok { std::string t; bool radbrytning = false; int mening = 0; };
    std::vector<Tok> tok;
    int mening = 0;
    bool ny = false;
    std::string del;
    auto lagg = [&]() {
        if (del.empty()) return;
        tok.push_back({del, ny, mening});
        ny = false;
        char c = del.back();
        bool slut = c == '.' || c == '?' || c == '!';
        if (c == '.' && (del == "kl." || del == "ca." || del == "st." || std::count(del.begin(), del.end(), '.') > 1)) slut = false;   // t.ex.
        if (slut) ++mening;
        del.clear();
    };
    for (char c : text) {
        if (c == ' ') lagg();
        else if (c == '\n') { lagg(); ny = true; if (!tok.empty() && tok.back().mening == mening) ++mening; }
        else del += c;
    }
    lagg();
    if (ord) for (auto& t : tok) ord->push_back(t.t);
    // Vilken mening är frågan? Den sista som slutar med ? – bara om det finns fler meningar
    int fragaMening = -1;
    if (fetFraga && !tok.empty() && tok.back().mening > 0)
        for (auto& t : tok) if (t.t.back() == '?') fragaMening = t.mening;
    std::string ut;
    for (size_t i = 0; i < tok.size(); ++i) {
        const Tok& t = tok[i];
        if (i) ut += t.radbrytning ? "\n" : (elev.lattlast && fetFraga && t.mening != tok[i - 1].mening ? "\n" : " ");
        bool forsta = i == 0 || tok[i - 1].mening != t.mening, sista = i + 1 == tok.size() || tok[i + 1].mening != t.mening;
        if (t.mening == fragaMening && forsta) ut += "<b>";
        // ordet utan skiljetecken runt om – finns det i ordlistan?
        size_t a = 0, b = t.t.size();
        while (a < b && strchr(".,;:!?()\"'«»", t.t[a])) ++a;
        while (b > a && strchr(".,;:!?()\"'«»", t.t[b - 1])) --b;
        std::string kärna = t.t.substr(a, b - a);
        gchar* sma = g_utf8_strdown(kärna.c_str(), -1);
        int oi = kärna.empty() ? -1 : data.ordFor(sma);
        g_free(sma);
        auto esc = [](const std::string& x) { gchar* e = g_markup_escape_text(x.c_str(), -1); std::string r = e; g_free(e); return r; };
        std::string ordet = oi >= 0 ? "<a href=\"ord:" + std::to_string(oi) + "\" title=\"" + esc(data.ordlista()[oi].ord) + "\">" + esc(kärna) + "</a>"
                                    : esc(kärna);
        std::string helt = esc(t.t.substr(0, a)) + ordet + esc(t.t.substr(b));
        if ((int)i == markera) helt = "<span background=\"#fde68a\" foreground=\"#1f2124\">" + helt + "</span>";
        ut += helt;
        if (t.mening == fragaMening && sista) ut += "</b>";
    }
    return ut;
}

void Matte::sattText(GtkWidget* l, const std::string& text, bool fetFraga, int markera) {
    gtk_label_set_markup(GTK_LABEL(l), textMarkup(text, markera, fetFraga).c_str());
}

void Matte::lankad(GtkWidget* l) {
    g_signal_connect(l, "activate-link", CB(+[](GtkLabel*, const char* uri, gpointer) -> gboolean {
        if (g_str_has_prefix(uri, "ord:")) { M->visaOrd(atoi(uri + 4)); return TRUE; }
        return FALSE;
    }), nullptr);
}

bool Matte::lasMarkerat(const std::vector<Lasdel>& delar_, const std::vector<std::string>& extra) {
    // Etiketterna hålls vid liv tills uppläsningen är klar, även om vyn byts under tiden
    auto delar = std::shared_ptr<std::vector<Lasdel>>(new std::vector<Lasdel>(delar_), [](std::vector<Lasdel>* v) {
        for (auto& d : *v) g_object_unref(d.l);
        delete v;
    });
    for (auto& d : *delar) g_object_ref(d.l);
    std::vector<std::string> alla;
    std::vector<int> start;
    for (auto& d : *delar) {
        start.push_back((int)alla.size());
        textMarkup(d.text, -1, d.fet, &alla);
    }
    start.push_back((int)alla.size());
    for (auto& e : extra) alla.push_back(e);
    bool ok = lasOrd(alla, [this, delar, start](int i) {
        for (size_t k = 0; k < delar->size(); ++k) {
            int lokal = i >= start[k] && i < start[k + 1] ? i - start[k] : -1;
            sattText((*delar)[k].l, (*delar)[k].text, (*delar)[k].fet, lokal);
        }
    });
    if (!ok && !rostSaknasVisad) { rostSaknasVisad = true; toast(tr("No voice installed (espeak-ng)", "Ingen röst installerad (espeak-ng)")); }
    return ok;
}

// En etikett som kan läsas upp med markering: länkar till ordlistan, och läggs i listan för uppläsningen
GtkWidget* Matte::lasbar(const std::string& text, const char* klass, std::vector<Lasdel>& delar, bool fet) {
    GtkWidget* l = lbl("", klass);
    sattText(l, text, fet);
    lankad(l);
    delar.push_back({l, text, fet});
    return l;
}

// Frågan (och exemplet med lucka) läses upp ord för ord – ordet som läses markeras
void Matte::lasUppUppgift() {
    if (!uppgift.v) return;
    const Variant* v = uppgift.v;
    std::vector<Lasdel> delar = {{ovaFraga, v->fraga, true}};
    std::vector<std::string> extra(v->steg.begin(), v->steg.end());
    if (uppgift.mall->typ == "val" || uppgift.mall->typ == "kryssa") {   // alternativen också (utan markering)
        extra.push_back(tr("You can choose:", "Du kan välja:"));
        for (auto& a : v->alternativ) if (a.find(':') == std::string::npos) extra.push_back(a + ",");
    }
    lasMarkerat(delar, extra);
}

// ---- sifferknapparna: svara utan tangentbord (pekskärm, läs- och skrivsvårigheter, motorik) ----------

static void allaFalt(GtkWidget* w, std::vector<GtkWidget*>& ut) {
    if (GTK_IS_ENTRY(w)) { ut.push_back(w); return; }
    for (GtkWidget* c = gtk_widget_get_first_child(w); c; c = gtk_widget_get_next_sibling(c)) allaFalt(c, ut);
}

void Matte::sifferknappar(GtkWidget* forvaring) {
    std::vector<GtkWidget*> falt;
    allaFalt(forvaring, falt);
    if (falt.empty()) return;   // uppgiften har inga skrivfält (klicka, måla, välja …)
    GtkWidget* g = gtk_grid_new();
    gtk_widget_add_css_class(g, "matte-siffror");
    gtk_grid_set_row_spacing(GTK_GRID(g), 6);
    gtk_grid_set_column_spacing(GTK_GRID(g), 6);
    gtk_widget_set_halign(g, GTK_ALIGN_START);
    gtk_widget_set_margin_top(g, 6);
    const char* tangenter[4][5] = {{"7", "8", "9", "+", "⌫"}, {"4", "5", "6", "−", "/"}, {"1", "2", "3", "x", "%"}, {"0", ",", "(", ")", "↵"}};
    // Vilket fält skriver vi i? Det som har fokus, annars det första.
    auto mal = [forvaring]() -> GtkWidget* {
        std::vector<GtkWidget*> f;
        allaFalt(M->uppgKort ? M->uppgKort : forvaring, f);
        GtkWidget* fokus = gtk_root_get_focus(GTK_ROOT(M->win));
        for (GtkWidget* e : f) if (fokus && (fokus == e || gtk_widget_is_ancestor(fokus, e))) return e;
        std::vector<GtkWidget*> svar;
        allaFalt(forvaring, svar);
        return !svar.empty() ? svar.front() : f.empty() ? nullptr : f.front();
    };
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 5; ++c) {
            std::string t = tangenter[r][c];
            GtkWidget* b = gtk_button_new_with_label(t == "↵" ? tr("Check", "Kolla") : t.c_str());
            gtk_widget_set_focusable(b, FALSE);   // fokus stannar i svarsfältet
            gtk_widget_set_size_request(b, t == "↵" ? 0 : 56, 52);
            if (t == "↵") gtk_widget_add_css_class(b, "suggested-action");
            if (t == "⌫") gtk_widget_set_tooltip_text(b, tr("Erase", "Sudda"));
            klick(b, [t, mal] {
                GtkWidget* fokus = gtk_root_get_focus(GTK_ROOT(M->win));
                if (fokus && fokus == M->papper) {   // räknepappret: siffran i rutan, ↵ = nästa rad
                    if (t == "↵") { gtk_widget_activate(M->svarsyta.fokus ? M->svarsyta.fokus : M->papper); return; }
                    raknepapper_skriv(M->papper, t);
                    return;
                }
                GtkWidget* e = mal();
                if (!e) return;
                if (t == "↵") { gtk_widget_activate(e); return; }
                int pos = gtk_editable_get_position(GTK_EDITABLE(e));
                if (t == "⌫") {
                    if (pos > 0) gtk_editable_delete_text(GTK_EDITABLE(e), pos - 1, pos);
                } else {
                    std::string in = t == "−" ? "-" : t;
                    gtk_editable_insert_text(GTK_EDITABLE(e), in.c_str(), -1, &pos);
                    gtk_editable_set_position(GTK_EDITABLE(e), pos);
                }
                gtk_widget_grab_focus(e);
            });
            gtk_grid_attach(GTK_GRID(g), b, c, r, 1, 1);
        }
    gtk_box_append(GTK_BOX(forvaring), g);
}

// ---- ordlistan ---------------------------------------------------------------------------------------

// ---- elevens sida (läraren) -------------------------------------------------------------------------
// Bygger på händelseloggen de senaste 30 dagarna och på framstegen. Allt räknas här på datorn.

void Matte::visaElevvy(const std::string& namn) {
    markeraNav("oversikt");
    Elev e = namn == elev.namn ? elev : Elev::ladda(namn);
    std::string visn = e.visningsnamn.empty() ? namn : e.visningsnamn;
    auto logg = lasLogg(namn, 30);
    GtkWidget* klamp = kortyta();
    GtkWidget* box = inre(klamp);
    GtkWidget* topp = hbox(10);
    gtk_widget_add_css_class(topp, "matte-titelrad");
    GtkWidget* t = lbl(visn, "matte-titel", false);
    gtk_widget_set_valign(t, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(t, TRUE);
    gtk_box_append(GTK_BOX(topp), t);
    GtkWidget* tillbaka = knapp(tr("Back to overview", "Tillbaka till översikten"), false, [] { M->visaOversikt(); });
    gtk_widget_set_valign(tillbaka, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(topp), tillbaka);
    gtk_box_append(GTK_BOX(box), topp);
    gtk_box_append(GTK_BOX(box), marg(lbl(tr("The last 30 days. Everything is calculated from files on this computer.",
                                             "De senaste 30 dagarna. Allt räknas fram ur filer på den här datorn."), "matte-svag")));

    // Räkna ihop loggen
    struct PerKort { int forsta = 0, forstaRatt = 0, avbrott = 0, sekSumma = 0, sekAntal = 0; };
    std::map<std::string, PerKort> perKort;
    std::map<std::string, int> ord;
    int pass = 0, uppgifter = 0, ratt = 0, avbrott = 0, ledtradar = 0, omkastade = 0, sek = 0;
    int skrivA = 0, skrivR = 0, bildA = 0, bildR = 0;
    static const std::set<std::string> SKRIVA = {"text", "uttryck", "tabell", "skiss"};
    std::set<std::string> dagar;
    for (auto& j : logg) {
        std::string h = j["h"].str(), kid = j["kort"].str();
        dagar.insert(j["t"].str().substr(0, 10));
        if (h == "pass") ++pass;
        else if (h == "ord") ++ord[j["ord"].str()];
        else if (h == "ledtrad") ++ledtradar;
        else if (h == "avbrott") { ++avbrott; ++perKort[kid].avbrott; sek += j["sek"].heltal(); }
        else if (h == "svar") {
            if (j["omkastad"].sant()) ++omkastade;
            if (j["forsok"].heltal() != 1 || j["typ"].str() == "rita") { if (j["ratt"].sant()) sek += j["sek"].heltal(); continue; }
            bool r = j["ratt"].sant();
            ++uppgifter; ratt += r;
            PerKort& p = perKort[kid];
            ++p.forsta; p.forstaRatt += r;
            p.sekSumma += j["sek"].heltal(); ++p.sekAntal;
            if (r) sek += j["sek"].heltal();
            if (SKRIVA.count(j["typ"].str())) { ++skrivA; skrivR += r; } else { ++bildA; bildR += r; }
        }
    }

    // Nyckeltalen som rutor
    GtkWidget* tal = adw_wrap_box_new();
    adw_wrap_box_set_child_spacing(ADW_WRAP_BOX(tal), 10);
    adw_wrap_box_set_line_spacing(ADW_WRAP_BOX(tal), 10);
    marg(tal, 16, 8);
    auto ruta = [&](const std::string& varde, const std::string& text) {
        GtkWidget* v = vbox(2);
        gtk_widget_add_css_class(v, "card");
        gtk_widget_add_css_class(v, "matte-ruta");
        gtk_box_append(GTK_BOX(v), lbl(varde, "matte-ruta-tal", false));
        gtk_box_append(GTK_BOX(v), lbl(text, "matte-svag", false));
        adw_wrap_box_append(ADW_WRAP_BOX(tal), v);
    };
    ruta(std::to_string(dagar.size()), tr("days with practice", "dagar med övning"));
    ruta(std::to_string(pass), tr("sessions completed", "pass klara"));
    ruta(std::to_string(uppgifter), tr("tasks", "uppgifter"));
    ruta(uppgifter ? std::to_string(100 * ratt / uppgifter) + " %" : "–", tr("right first time", "rätt första gången"));
    ruta(std::to_string(sek / 60) + " min", tr("time on tasks", "tid med uppgifter"));
    ruta(std::to_string(avbrott), tr("tasks left unfinished", "lämnade ofärdiga"));
    gtk_box_append(GTK_BOX(box), tal);
    {   // uppgifter per dag, de senaste 14 dagarna (systemets diagram)
        std::map<std::string, int> perDag;
        for (auto& j : logg)
            if (j["h"].str() == "svar" && j["forsok"].heltal() == 1) ++perDag[j["t"].str().substr(0, 10)];
        std::vector<double> v;
        std::vector<std::string> etik;
        for (int d = -13; d <= 0; ++d) {
            std::string dag = plusDagar(idag(), d);
            v.push_back(perDag.count(dag) ? perDag[dag] : 0);
            etik.push_back(dag.substr(8, 2));
        }
        std::vector<const char*> ce;
        for (auto& x : etik) ce.push_back(x.c_str());
        gtk_box_append(GTK_BOX(box), marg(lbl(tr("Tasks per day (14 days)", "Uppgifter per dag (14 dagar)"), "matte-rubrik"), 16, 12));
        GtkWidget* d = wenuy_chart_new(WENUY_CHART_BARS);
        wenuy_chart_set_data(WENUY_CHART(d), v.data(), ce.data(), (guint)v.size());
        gtk_widget_set_size_request(d, -1, 160);
        gtk_box_append(GTK_BOX(box), marg(d, 16, 4));
    }
    auto n = e.sammanfattning();
    gtk_box_append(GTK_BOX(box), marg(lbl(std::string(tr("In total: ", "Totalt: ")) + std::to_string(n[BEHARSKAR]) + tr(" cards mastered, ", " kort behärskas, ") +
                                          std::to_string(n[OVAR]) + tr(" being practised, ", " övas, ") + std::to_string(e.attRepetera().size()) +
                                          tr(" to review today.", " att repetera i dag."), "matte-svag"), 16, 4));

    // Vad det kan tyda på – försiktiga förslag, inga diagnoser
    std::vector<std::string> tips;
    if (skrivA >= 8 && bildA >= 8 && 100 * skrivR / skrivA + 20 < 100 * bildR / bildA)
        tips.push_back(std::string(tr("Right ", "Rätt ")) + std::to_string(100 * bildR / bildA) + tr(" % on picture tasks but ", " % på bilduppgifter men ") +
                       std::to_string(100 * skrivR / skrivA) + tr(" % on written ones. Reading or writing may be harder than the maths – try Easy reading and reading aloud.",
                                                                   " % på skrivuppgifter. Läsandet eller skrivandet kan vara svårare än matten – prova Lättläst och uppläsning."));
    int uppslag = 0;
    for (auto& [o, a] : ord) uppslag += a;
    if (uppslag >= 5 && uppgifter > 0 && uppslag * 8 >= uppgifter)
        tips.push_back(tr("Looks up maths words often. Go through the words together – they are often the hard part.",
                          "Slår ofta upp matteord. Gå igenom orden tillsammans – det är ofta de som är svåra."));
    if (omkastade >= 2)
        tips.push_back(std::string(tr("Digits swapped places ", "Siffror har bytt plats ")) + std::to_string(omkastade) +
                       tr(" times (21 for 12). Easy reading gives a new try when that happens.", " gånger (21 för 12). Lättläst ger ett nytt försök när det händer."));
    if (uppgifter >= 10 && avbrott * 5 > uppgifter)
        tips.push_back(tr("Often leaves a task unfinished. Shorter sessions, a break, or an easier card first may help.",
                          "Lämnar ofta en uppgift ofärdig. Kortare pass, en paus eller ett lättare kort först kan hjälpa."));
    if (uppgifter >= 10 && ledtradar * 2 > uppgifter)
        tips.push_back(tr("Uses hints a lot – the worked examples on the card may need going through together.",
                          "Använder ledtråden ofta – de lösta exemplen på kortet kan behöva gås igenom tillsammans."));
    if (!tips.empty()) {
        gtk_box_append(GTK_BOX(box), marg(lbl(tr("Worth noticing", "Värt att lägga märke till"), "matte-rubrik"), 16, 12));
        for (auto& x : tips) gtk_box_append(GTK_BOX(box), lbl(x, "matte-minns"));
        gtk_box_append(GTK_BOX(box), marg(lbl(tr("These are suggestions from the numbers, not assessments. Talk with the student.",
                                                 "Det här är förslag utifrån siffrorna, inga bedömningar. Prata med eleven."), "matte-svag"), 16, 4));
    }

    // Kort där eleven fastnar
    std::vector<std::pair<std::string, PerKort>> fast;
    for (auto& [kid, p] : perKort)
        if (data.kort(kid) && ((p.forsta >= 3 && p.forstaRatt * 10 < p.forsta * 6) || p.avbrott >= 2)) fast.push_back({kid, p});
    std::sort(fast.begin(), fast.end(), [](auto& a, auto& b) {
        double ra = a.second.forsta ? (double)a.second.forstaRatt / a.second.forsta : 0, rb = b.second.forsta ? (double)b.second.forstaRatt / b.second.forsta : 0;
        return ra != rb ? ra < rb : a.second.avbrott > b.second.avbrott;
    });
    gtk_box_append(GTK_BOX(box), marg(lbl(tr("Where it gets stuck", "Där det tar stopp"), "matte-rubrik"), 16, 12));
    if (fast.empty()) gtk_box_append(GTK_BOX(box), marg(lbl(tr("Nothing stands out.", "Inget sticker ut."), "matte-svag")));
    for (size_t i = 0; i < fast.size() && i < 6; ++i) {
        const Kort* k = data.kort(fast[i].first);
        const PerKort& p = fast[i].second;
        GtkWidget* rad = hbox(10);
        marg(rad);
        if (!k->ikon.empty()) gtk_box_append(GTK_BOX(rad), ikonbild(k->ikon, 28));
        std::string txt = k->titel + "\n" + std::to_string(p.forstaRatt) + tr(" of ", " av ") + std::to_string(p.forsta) + tr(" right first time", " rätt första gången");
        if (p.avbrott) txt += " · " + std::to_string(p.avbrott) + tr(" left unfinished", " lämnade ofärdiga");
        if (p.sekAntal) txt += " · " + std::to_string(p.sekSumma / p.sekAntal) + tr(" s per task", " s per uppgift");
        GtkWidget* tl = lbl(txt);
        gtk_widget_set_hexpand(tl, TRUE);
        gtk_box_append(GTK_BOX(rad), tl);
        std::string kid = k->id;
        gtk_box_append(GTK_BOX(rad), knapp(tr("Worksheet", "Arbetsblad"), false, [kid] { if (auto* k = M->data.kort(kid)) M->skrivArbetsblad(*k); },
                                          "printer-symbolic"));
        gtk_box_append(GTK_BOX(rad), knapp(tr("Card", "Kortet"), false, [kid] { if (auto* k = M->data.kort(kid)) M->visaKort(*k); }));
        gtk_box_append(GTK_BOX(box), rad);
        if (const Kort* f = k->forkunskap.empty() ? nullptr : data.kort(k->forkunskap[0]))
            gtk_box_append(GTK_BOX(box), marg(lbl(std::string(tr("Builds on: ", "Bygger på: ")) + f->titel, "matte-svag"), 54));
    }

    // Feltankar som kommer tillbaka
    std::vector<std::tuple<int, std::string, std::string>> ft;
    for (auto& [kid, s] : e.kort)
        for (auto& [fid, antal] : s.feltankeTotalt) {
            const Kort* k = data.kort(kid);
            if (!k || antal < 2) continue;
            auto f = k->feltankar.find(fid);
            ft.push_back({antal, k->titel, f != k->feltankar.end() ? f->second : fid});
        }
    std::sort(ft.begin(), ft.end(), [](auto& a, auto& b) { return std::get<0>(a) > std::get<0>(b); });
    if (!ft.empty()) {
        gtk_box_append(GTK_BOX(box), marg(lbl(tr("Misconceptions that come back", "Feltankar som kommer tillbaka"), "matte-rubrik"), 16, 12));
        for (size_t i = 0; i < ft.size() && i < 8; ++i)
            gtk_box_append(GTK_BOX(box), lbl(std::get<1>(ft[i]) + " – " + std::to_string(std::get<0>(ft[i])) + "×\n" + std::get<2>(ft[i]), "matte-galler"));
    }

    // Ord eleven slår upp
    if (!ord.empty()) {
        gtk_box_append(GTK_BOX(box), marg(lbl(tr("Words the student looks up", "Ord eleven slår upp"), "matte-rubrik"), 16, 12));
        std::vector<std::pair<int, std::string>> ol;
        for (auto& [o, a] : ord) ol.push_back({a, o});
        std::sort(ol.rbegin(), ol.rend());
        GtkWidget* w = adw_wrap_box_new();
        adw_wrap_box_set_child_spacing(ADW_WRAP_BOX(w), 6);
        adw_wrap_box_set_line_spacing(ADW_WRAP_BOX(w), 6);
        marg(w, 16, 4);
        for (size_t i = 0; i < ol.size() && i < 16; ++i) {
            int oi = data.ordMedNamn(ol[i].second);
            GtkWidget* b = knapp(ol[i].second + " · " + std::to_string(ol[i].first), false, [oi] { M->visaOrd(oi); });
            gtk_widget_add_css_class(b, "pill");
            adw_wrap_box_append(ADW_WRAP_BOX(w), b);
        }
        gtk_box_append(GTK_BOX(box), w);
    }

    // Loggen: vad den är, och att den kan rensas
    GtkWidget* sist = hbox(10);
    marg(sist, 16, 18);
    gtk_widget_set_margin_bottom(sist, 8);
    GtkWidget* info = lbl(tr("The log is stored only on this computer, in the student's data folder. Nothing is sent anywhere.",
                             "Loggen sparas bara på den här datorn, i elevens datamapp. Inget skickas någonstans."), "matte-svag");
    gtk_widget_set_hexpand(info, TRUE);
    gtk_box_append(GTK_BOX(sist), info);
    std::string n2 = namn;
    GtkWidget* exp = knapp(tr("Export student…", "Exportera elev…"), false, [n2] { M->exportera(n2); }, "document-save-symbolic");
    gtk_widget_set_tooltip_text(exp, tr("Everything about the student in one file – e.g. on a USB stick, to move to another computer",
                                        "Allt om eleven i en fil – t.ex. på ett USB-minne, för att flytta till en annan dator"));
    gtk_box_append(GTK_BOX(sist), exp);
    gtk_box_append(GTK_BOX(sist), knapp(tr("Clear the log", "Rensa loggen"), false, [n2] {
        AdwAlertDialog* d = ADW_ALERT_DIALOG(adw_alert_dialog_new(tr("Clear the log?", "Rensa loggen?"),
            tr("The progress (cards, rule book) is kept. Only the record of answers, breaks and looked-up words is removed.",
               "Framstegen (kort, regelbok) finns kvar. Bara listan över svar, avbrott och uppslagna ord tas bort.")));
        adw_alert_dialog_add_responses(d, "nej", tr("Cancel", "Avbryt"), "ja", tr("Clear", "Rensa"), nullptr);
        adw_alert_dialog_set_response_appearance(d, "ja", ADW_RESPONSE_DESTRUCTIVE);
        g_object_set_data_full(G_OBJECT(d), "namn", g_strdup(n2.c_str()), g_free);
        g_signal_connect(d, "response", CB(+[](AdwAlertDialog* d, const char* svar, gpointer) {
            if (std::string(svar) != "ja") return;
            std::string n = static_cast<const char*>(g_object_get_data(G_OBJECT(d), "namn"));
            rensaLogg(n);
            M->visaElevvy(n);
            M->toast(tr("The log is cleared.", "Loggen är rensad."));
        }), nullptr);
        adw_dialog_present(ADW_DIALOG(d), M->win);
    }));
    gtk_box_append(GTK_BOX(box), sist);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(elevRull), klamp);
    titel(visn);
    visa("elev");
}

// ---- flytta elever mellan datorer ----------------------------------------------------------------------

void Matte::exportera(const std::string& namn) {
    Elev e = Elev::ladda(namn);
    std::string visn = e.visningsnamn.empty() ? namn : e.visningsnamn;
    GtkFileDialog* d = gtk_file_dialog_new();
    gtk_file_dialog_set_title(d, tr("Export student", "Exportera elev"));
    gtk_file_dialog_set_initial_name(d, (sakertFilnamn(std::string("Matte – ") + visn) + ".json").c_str());
    GFile* dok = g_file_new_for_path(utskriftsmapp().c_str());
    gtk_file_dialog_set_initial_folder(d, dok);
    g_object_unref(dok);
    gtk_file_dialog_save(d, GTK_WINDOW(win), nullptr, +[](GObject* o, GAsyncResult* r, gpointer p) {
        std::string* namn = static_cast<std::string*>(p);
        GFile* f = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(o), r, nullptr);   // nullptr = avbrutet
        if (f) {
            gchar* sok = g_file_get_path(f);
            std::string fel;
            if (sok && exporteraElev(*namn, sok, &fel)) M->toast(tr("The student is exported.", "Eleven är exporterad."));
            else M->toast(std::string(tr("Could not export: ", "Kunde inte exportera: ")) + fel);
            g_free(sok);
            g_object_unref(f);
        }
        delete namn;
        g_object_unref(o);
    }, new std::string(namn));
}

void Matte::importera() {
    GtkFileDialog* d = gtk_file_dialog_new();
    gtk_file_dialog_set_title(d, tr("Import student", "Importera elev"));
    GtkFileFilter* f = gtk_file_filter_new();
    gtk_file_filter_set_name(f, tr("Student files (.json)", "Elevfiler (.json)"));
    gtk_file_filter_add_pattern(f, "*.json");
    GListStore* filter = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filter, f);
    gtk_file_dialog_set_filters(d, G_LIST_MODEL(filter));
    g_object_unref(filter); g_object_unref(f);
    gtk_file_dialog_open(d, GTK_WINDOW(win), nullptr, +[](GObject* o, GAsyncResult* r, gpointer) {
        GFile* f = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(o), r, nullptr);
        if (f) {
            gchar* sok = g_file_get_path(f);
            if (sok) M->importeraFil(sok, false);
            g_free(sok);
            g_object_unref(f);
        }
        g_object_unref(o);
    }, nullptr);
}

void Matte::importeraFil(const std::string& fil, bool ersatt) {
    std::string fel, visn;
    bool fanns = false;
    std::string namn = importeraElev(fil, ersatt, &fel, &fanns, &visn);
    if (namn.empty()) { toast(std::string(tr("Could not import: ", "Kunde inte importera: ")) + fel); return; }
    if (fanns) {   // samma elev finns redan på den här datorn – fråga innan något skrivs över
        AdwAlertDialog* d = ADW_ALERT_DIALOG(adw_alert_dialog_new(
            (std::string(tr("Replace ", "Ersätta ")) + visn + "?").c_str(),
            tr("There is already a student with that name on this computer. Their progress here will be replaced by the file's.",
               "Det finns redan en elev med det namnet på den här datorn. Framstegen här ersätts med dem i filen.")));
        adw_alert_dialog_add_responses(d, "nej", tr("Cancel", "Avbryt"), "ja", tr("Replace", "Ersätt"), nullptr);
        adw_alert_dialog_set_response_appearance(d, "ja", ADW_RESPONSE_DESTRUCTIVE);
        g_object_set_data_full(G_OBJECT(d), "fil", g_strdup(fil.c_str()), g_free);
        g_signal_connect(d, "response", CB(+[](AdwAlertDialog* d, const char* svar, gpointer) {
            if (std::string(svar) == "ja") M->importeraFil(static_cast<const char*>(g_object_get_data(G_OBJECT(d), "fil")), true);
        }), nullptr);
        adw_dialog_present(ADW_DIALOG(d), win);
        return;
    }
    if (namn == elev.namn) { elev = Elev::ladda(namn); tillampaLeklage(); fyllLista(); }
    toast(visn + tr(" is imported.", " är importerad."));
    visaOversikt();
}

void Matte::visaOrd(int i) {
    if (i < 0 || i >= (int)data.ordlista().size()) return;
    const Ord& o = data.ordlista()[i];
    {
        Json j = Json::objekt();
        j.satt("ord", o.ord);
        const char* sida = gtk_stack_get_visible_child_name(GTK_STACK(stack));
        if (aktuellt && sida && (std::string(sida) == "ova" || std::string(sida) == "kort")) j.satt("kort", aktuellt->id);
        logga(elev.namn, "ord", j);
    }
    if (ordDialog) adw_dialog_force_close(ordDialog);
    AdwDialog* dlg = adw_dialog_new();
    ordDialog = dlg;
    g_signal_connect(dlg, "closed", CB(+[](AdwDialog* d, gpointer) { if (M->ordDialog == d) M->ordDialog = nullptr; tyst(); }), nullptr);
    adw_dialog_set_title(dlg, tr("Maths word", "Matteord"));
    adw_dialog_set_content_width(dlg, 560);
    GtkWidget* tv = adw_toolbar_view_new();
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tv), adw_header_bar_new());
    GtkWidget* v = vbox(12);
    gtk_widget_add_css_class(v, "matte-kort");
    gtk_widget_set_margin_top(v, 4);
    GtkWidget* topp = hbox(10);
    marg(topp, 16, 14);
    GtkWidget* rubrik = lbl(o.ord, "matte-stor", false);
    gtk_widget_set_hexpand(rubrik, TRUE);
    gtk_box_append(GTK_BOX(topp), rubrik);
    GtkWidget* las = gtk_button_new_from_icon_name("audio-volume-high-symbolic");
    gtk_widget_add_css_class(las, "circular");
    gtk_widget_set_tooltip_text(las, tr("Read aloud", "Läs upp"));
    auto delar = std::make_shared<std::vector<Lasdel>>();
    delar->push_back({rubrik, o.ord, false});
    gtk_box_append(GTK_BOX(topp), las);
    gtk_box_append(GTK_BOX(v), topp);
    gtk_box_append(GTK_BOX(v), marg(lasbar(o.forklaring, "matte-fraga", *delar)));
    if (!o.exempel.empty()) gtk_box_append(GTK_BOX(v), lasbar(o.exempel, "matte-exempel", *delar));
    klick(las, [delar] { M->lasMarkerat(*delar); });
    if (o.figur.arObjekt()) gtk_box_append(GTK_BOX(v), marg(figurvy(data, o.figur)));
    if (!o.se.empty()) {
        GtkWidget* se = adw_wrap_box_new();
        adw_wrap_box_set_child_spacing(ADW_WRAP_BOX(se), 6);
        adw_wrap_box_set_line_spacing(ADW_WRAP_BOX(se), 6);
        marg(se, 16, 4);
        adw_wrap_box_append(ADW_WRAP_BOX(se), lbl(tr("See also:", "Se också:"), "matte-svag", false));
        for (auto& s : o.se) {
            int j = data.ordMedNamn(s);
            if (j < 0) continue;
            GtkWidget* b = knapp(s, false, [j] { g_idle_add_once(+[](gpointer p) { M->visaOrd(GPOINTER_TO_INT(p)); }, GINT_TO_POINTER(j)); });
            gtk_widget_add_css_class(b, "pill");
            adw_wrap_box_append(ADW_WRAP_BOX(se), b);
        }
        gtk_box_append(GTK_BOX(v), se);
    }
    gtk_widget_set_margin_bottom(v, 16);
    GtkWidget* r = rull(v);
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(r), TRUE);
    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(tv), r);
    adw_dialog_set_child(dlg, tv);
    adw_dialog_present(dlg, win);
    if (lasesUpp()) lasMarkerat(*delar);
}

// Alla matteord i bokstavsordning, med sökfält
void Matte::visaOrdlista() {
    GtkWidget* klamp = kortyta();
    GtkWidget* box = inre(klamp);
    gtk_box_append(GTK_BOX(box), lbl(tr("Maths words", "Matteorden"), "matte-titel"));
    gtk_box_append(GTK_BOX(box), marg(lbl(tr("Words that are hard are often the hardest part of maths. Click a word for a picture and an explanation. "
                                             "The words are also underlined in the tasks.",
                                             "Ofta är det orden som är svåra, inte matten. Klicka på ett ord för en bild och en förklaring. "
                                             "Orden är också understrukna i uppgifterna."), "matte-svag")));
    GtkWidget* sok = gtk_search_entry_new();
    gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(sok), tr("Search …", "Sök …"));
    marg(sok, 16, 4);
    gtk_box_append(GTK_BOX(box), sok);
    std::vector<int> ordning;
    for (size_t i = 0; i < data.ordlista().size(); ++i) ordning.push_back((int)i);
    std::sort(ordning.begin(), ordning.end(), [this](int a, int b) {
        return g_utf8_collate(data.ordlista()[a].ord.c_str(), data.ordlista()[b].ord.c_str()) < 0;
    });
    GtkWidget* galleri = adw_wrap_box_new();
    adw_wrap_box_set_child_spacing(ADW_WRAP_BOX(galleri), 8);
    adw_wrap_box_set_line_spacing(ADW_WRAP_BOX(galleri), 8);
    marg(galleri, 16, 6);
    gtk_widget_set_margin_bottom(galleri, 10);
    for (int i : ordning) {
        GtkWidget* b = knapp(data.ordlista()[i].ord, false, [i] { M->visaOrd(i); });
        gtk_widget_add_css_class(b, "pill");
        g_object_set_data_full(G_OBJECT(b), "former", g_strdup(([&] {
            std::string f = data.ordlista()[i].ord;
            for (auto& x : data.ordlista()[i].former) f += " " + x;
            return f;
        })().c_str()), g_free);
        adw_wrap_box_append(ADW_WRAP_BOX(galleri), b);
    }
    g_object_set_data(G_OBJECT(sok), "galleri", galleri);
    g_signal_connect(sok, "search-changed", CB(+[](GtkSearchEntry* e, gpointer) {
        GtkWidget* g = static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(e), "galleri"));
        gchar* q = g_utf8_strdown(gtk_editable_get_text(GTK_EDITABLE(e)), -1);
        for (GtkWidget* c = gtk_widget_get_first_child(g); c; c = gtk_widget_get_next_sibling(c)) {
            const char* f = static_cast<const char*>(g_object_get_data(G_OBJECT(c), "former"));
            gtk_widget_set_visible(c, !*q || (f && strstr(f, q)));
        }
        g_free(q);
    }), nullptr);
    gtk_box_append(GTK_BOX(box), galleri);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(ordlistaRull), klamp);
    titel(tr("Maths words", "Matteorden"));
    markeraNav("ordlista");
    visa("ordlista");
}

void Matte::uppdateraStatus() {
    auto n = elev.sammanfattning();
    size_t rep = elev.attRepetera().size();
    std::string start = std::string("○ ") + tr("Learning", "Lär") + " " + std::to_string(n[LAR]) + "   ◐ " + tr("Practising", "Övar") + " " +
                        std::to_string(n[OVAR]) + "   ● " + tr("Mastered", "Behärskar") + " " + std::to_string(n[BEHARSKAR]);
    std::string slut = rep ? std::to_string(rep) + " " + tr("to review today", "att repetera i dag") : "";
    wenuy_app_window_set_status(win, start.c_str(), rep ? slut.c_str() : nullptr);
}

// ---- sidopanelen ----------------------------------------------------------------------------------

// Sidomenyn, uppifrån: vem som övar · Hem, Min regelbok, Matteorden · kursen (ett stadium i taget, områden som
// fälls ut) · Inställningar och För läraren längst ned. Kursen är den enda delen som rullar.
static GtkWidget* navRad(GtkWidget* lista, const char* vy, GtkWidget* ikon, const std::string& text) {
    GtkWidget* b = hbox(10);
    gtk_widget_set_margin_top(b, 2); gtk_widget_set_margin_bottom(b, 2);
    gtk_box_append(GTK_BOX(b), ikon);
    gtk_box_append(GTK_BOX(b), lbl(text, nullptr, false));
    GtkWidget* r = gtk_list_box_row_new();
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(r), b);
    g_object_set_data(G_OBJECT(r), "vy", (gpointer) vy);
    gtk_list_box_append(GTK_LIST_BOX(lista), r);
    return r;
}

static void navKlick(GtkListBox*, GtkListBoxRow* rad, gpointer) {
    if (!rad || M->fyller) return;
    std::string vy = static_cast<const char*>(g_object_get_data(G_OBJECT(rad), "vy"));
    if (vy == "hem") M->visaHem();
    else if (vy == "regelbok") M->visaRegelbok();
    else if (vy == "karta") M->visaKarta();
    else if (vy == "ordlista") M->visaOrdlista();
    else if (vy == "installningar") M->visaInstallningar();
    else if (vy == "oversikt") M->visaOversikt();
}

void Matte::byggSidomeny() {
    GtkWidget* meny = vbox(0);
    gtk_widget_add_css_class(meny, "matte13-sidebar");
    elevKnapp = gtk_menu_button_new();
    gtk_widget_add_css_class(elevKnapp, "flat");
    gtk_widget_add_css_class(elevKnapp, "matte-elevknapp");
    gtk_menu_button_set_always_show_arrow(GTK_MENU_BUTTON(elevKnapp), TRUE);
    gtk_widget_set_tooltip_text(elevKnapp, tr("Who is practising? Switch student or add a new one",
                                              "Vem övar? Byt elev eller lägg till en ny"));
    gtk_box_append(GTK_BOX(meny), elevKnapp);

    navLista = gtk_list_box_new();
    gtk_widget_add_css_class(navLista, "navigation-sidebar");
    navRad(navLista, "hem", wenuy_color_icon_new("home", 22), tr("Home", "Hem"));
    navRad(navLista, "karta", wenuy_color_icon_new("apps", 22), tr("Course map", "Kursens karta"));
    navRad(navLista, "regelbok", wenuy_color_icon_new("note", 22), tr("My rule book", "Min regelbok"));
    if (!data.ordlista().empty()) navRad(navLista, "ordlista", wenuy_color_icon_new("search", 22), tr("Maths words", "Matteorden"));
    g_signal_connect(navLista, "row-activated", G_CALLBACK(navKlick), nullptr);
    gtk_box_append(GTK_BOX(meny), navLista);

    GtkWidget* rub = lbl(tr("THE COURSE", "KURSEN"), "matte-meny-rubrik", false);
    gtk_box_append(GTK_BOX(meny), rub);
    stadieVal = gtk_drop_down_new(nullptr, nullptr);
    gtk_widget_add_css_class(stadieVal, "matte-stadieval");
    gtk_widget_set_tooltip_text(stadieVal, tr("Which stage the list shows", "Vilket stadium listan visar"));
    g_signal_connect(stadieVal, "notify::selected", CB(+[](GtkDropDown* dd, GParamSpec*, gpointer) {
        if (M->fyller) return;
        guint i = gtk_drop_down_get_selected(dd);
        if (i >= M->stadieIds.size()) return;
        M->valtStadium = M->stadieIds[i];
        g_idle_add_once(+[](gpointer) { M->fyllLista(); }, nullptr);
    }), nullptr);
    gtk_box_append(GTK_BOX(meny), stadieVal);

    lista = gtk_list_box_new();
    gtk_widget_add_css_class(lista, "navigation-sidebar");
    // row-activated (klick/Enter), inte row-selected: listan väljer första raden själv när den får fokus
    g_signal_connect(lista, "row-activated", CB(+[](GtkListBox*, GtkListBoxRow* rad, gpointer) {
        if (!rad || M->fyller) return;
        const char* omr = static_cast<const char*>(g_object_get_data(G_OBJECT(rad), "omrade"));
        const char* kid = static_cast<const char*>(g_object_get_data(G_OBJECT(rad), "kort"));
        if (omr) {   // fäll ut eller ihop området
            std::string o = omr;
            bool oppen = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(rad), "oppen"));
            if (oppen) { M->oppnaOmraden.erase(o); M->stangdaOmraden.insert(o); }
            else { M->stangdaOmraden.erase(o); M->oppnaOmraden.insert(o); }
            g_idle_add_once(+[](gpointer) { M->fyllLista(); }, nullptr);   // inte medan listan hanterar klicket
        } else if (kid) {
            if (const Kort* k = M->data.kort(kid)) M->visaKort(*k);
        }
    }), nullptr);
    kursRull = rull(lista);
    gtk_box_append(GTK_BOX(meny), kursRull);

    gtk_box_append(GTK_BOX(meny), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    navLista2 = gtk_list_box_new();
    gtk_widget_add_css_class(navLista2, "navigation-sidebar");
    navRad(navLista2, "installningar", wenuy_color_icon_new("settings", 22), tr("Settings", "Inställningar"));
    navRad(navLista2, "oversikt", wenuy_color_icon_new("activity", 22), tr("For the teacher", "För läraren"));
    g_signal_connect(navLista2, "row-activated", G_CALLBACK(navKlick), nullptr);
    gtk_box_append(GTK_BOX(meny), navLista2);

    sidoLista = GTK_WIDGET(g_object_ref_sink(meny));   // byts mot passpanelen i fokusläget – behålls
    wenuy_app_window_set_sidebar(win, sidoLista);
}

// Elevknappen: en rund bokstav och namnet. Menyn: eleverna på datorn och "Ny elev".
void Matte::fyllElevKnapp() {
    std::string visn = elev.visningsnamn.empty() ? elev.namn : elev.visningsnamn;
    GtkWidget* b = hbox(10);
    std::string bokstav = visn.empty() ? "?" : visn.substr(0, g_utf8_next_char(visn.c_str()) - visn.c_str());
    gchar* stor = g_utf8_strup(bokstav.c_str(), -1);
    GtkWidget* av = gtk_label_new(stor);
    g_free(stor);
    gtk_widget_add_css_class(av, "matte-avatar");
    gtk_widget_set_valign(av, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(b), av);
    GtkWidget* t = vbox(0);
    gtk_widget_set_valign(t, GTK_ALIGN_CENTER);
    GtkWidget* n = lbl(visn == "elev" ? tr("Student", "Elev") : visn, "matte-elevnamn", false);
    gtk_label_set_ellipsize(GTK_LABEL(n), PANGO_ELLIPSIZE_END);
    gtk_box_append(GTK_BOX(t), n);
    GtkWidget* byt = lbl(tr("Switch student", "Byt elev"), "dim-label", false);   // systemets små texter: följer inte zoomen
    gtk_widget_add_css_class(byt, "caption");
    gtk_box_append(GTK_BOX(t), byt);
    gtk_box_append(GTK_BOX(b), t);
    gtk_menu_button_set_child(GTK_MENU_BUTTON(elevKnapp), b);

    GtkWidget* pop = gtk_popover_new();
    GtkWidget* pb = vbox(4);
    gtk_widget_set_margin_top(pb, 6); gtk_widget_set_margin_bottom(pb, 6);
    gtk_widget_set_margin_start(pb, 6); gtk_widget_set_margin_end(pb, 6);
    auto namnen = elever();
    if (std::find(namnen.begin(), namnen.end(), elev.namn) == namnen.end()) namnen.push_back(elev.namn);
    for (auto& nm : namnen) {
        std::string v = nm == elev.namn ? visn : Elev::ladda(nm).visningsnamn;
        if (v.empty()) v = nm;
        GtkWidget* rb = gtk_button_new();
        GtkWidget* rbb = hbox(8);
        GtkWidget* bock = gtk_image_new_from_icon_name("object-select-symbolic");
        gtk_widget_set_opacity(bock, nm == elev.namn ? 1.0 : 0.0);
        gtk_box_append(GTK_BOX(rbb), bock);
        gtk_box_append(GTK_BOX(rbb), lbl(v, nullptr, false));
        gtk_button_set_child(GTK_BUTTON(rb), rbb);
        gtk_widget_add_css_class(rb, "flat");
        std::string namn = nm;
        klick(rb, [namn, pop] {
            gtk_popover_popdown(GTK_POPOVER(pop));
            if (namn != M->elev.namn)
                g_idle_add_once(+[](gpointer p) { auto* s = static_cast<std::string*>(p); M->bytElev(*s); delete s; }, new std::string(namn));
        });
        gtk_box_append(GTK_BOX(pb), rb);
    }
    gtk_box_append(GTK_BOX(pb), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(pb), lbl(tr("New student", "Ny elev"), "matte-svag", false));
    GtkWidget* nyRad = hbox(6);
    GtkWidget* falt = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(falt), tr("Name", "Namn"));
    auto laggTill = [falt, pop]() {
        std::string t = gtk_editable_get_text(GTK_EDITABLE(falt));
        while (!t.empty() && t.back() == ' ') t.pop_back();
        if (t.empty()) return;
        Elev e = Elev::ladda(filnamn(t));
        e.visningsnamn = t;
        e.spara();
        gtk_popover_popdown(GTK_POPOVER(pop));
        g_idle_add_once(+[](gpointer p) { auto* s = static_cast<std::string*>(p); M->bytElev(*s); delete s; }, new std::string(e.namn));
    };
    GtkWidget* ok = knapp(tr("Add", "Lägg till"), true, laggTill);
    auto* lt = new std::function<void()>(laggTill);
    g_signal_connect_data(falt, "activate", CB(+[](GtkEntry*, gpointer p) { (*static_cast<std::function<void()>*>(p))(); }), lt,
                          +[](gpointer p, GClosure*) { delete static_cast<std::function<void()>*>(p); }, (GConnectFlags)0);
    gtk_box_append(GTK_BOX(nyRad), falt);
    gtk_box_append(GTK_BOX(nyRad), ok);
    gtk_box_append(GTK_BOX(pb), nyRad);
    gtk_popover_set_child(GTK_POPOVER(pop), pb);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(elevKnapp), pop);
}

// Markerar sidan i menyn ("" = ingen av dem – t.ex. ett kort, som markeras i kursen)
void Matte::markeraNav(const std::string& vy) {
    for (GtkWidget* l : {navLista, navLista2}) {
        bool hittad = false;
        for (int i = 0; GtkListBoxRow* r = gtk_list_box_get_row_at_index(GTK_LIST_BOX(l), i); ++i) {
            const char* v = static_cast<const char*>(g_object_get_data(G_OBJECT(r), "vy"));
            if (v && vy == v) { gtk_list_box_select_row(GTK_LIST_BOX(l), r); hittad = true; }
        }
        if (!hittad) gtk_list_box_unselect_all(GTK_LIST_BOX(l));
    }
    if (!vy.empty()) gtk_list_box_unselect_all(GTK_LIST_BOX(lista));
}

std::string Matte::stadiumFor(const std::string& kortId) const {
    for (auto& m : data.laroplan())
        if (std::find(m.kort.begin(), m.kort.end(), kortId) != m.kort.end()) return m.stadium;
    return "";
}

void Matte::fyllLista() {
    fyller = true;
    GtkAdjustment* adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(kursRull));
    double las = gtk_adjustment_get_value(adj);
    while (GtkListBoxRow* r = gtk_list_box_get_row_at_index(GTK_LIST_BOX(lista), 0)) gtk_list_box_remove(GTK_LIST_BOX(lista), GTK_WIDGET(r));
    ordning.clear();
    // Ordningen över alla kort (för "Fortsätt")
    for (auto& [st, stNamn] : data.stadier()) {
        if (elev.leklage && !LEKLAGE_STADIER.count(st)) continue;
        for (auto& omr : data.omradeOrdning())
            for (auto& m : data.laroplan())
                if (m.stadium == st && m.omrade == omr)
                    for (auto& id : m.kort) if (data.kort(id) && std::find(ordning.begin(), ordning.end(), id) == ordning.end()) ordning.push_back(id);
    }
    // Stadierna som har kort – ett i taget i listan
    std::vector<std::string> namn;
    stadieIds.clear();
    for (auto& [st, stNamn] : data.stadier()) {
        if (elev.leklage && !LEKLAGE_STADIER.count(st)) continue;
        bool nagot = false;
        for (auto& m : data.laroplan()) if (m.stadium == st) for (auto& id : m.kort) nagot |= data.kort(id) != nullptr;
        if (!nagot) continue;
        stadieIds.push_back(st);
        namn.push_back(stNamn);
    }
    std::string nasta = nastaKort();
    std::string visas = valtStadium;
    if (std::find(stadieIds.begin(), stadieIds.end(), visas) == stadieIds.end()) {
        visas = aktuellt ? stadiumFor(aktuellt->id) : "";
        if (visas.empty() && !nasta.empty()) visas = stadiumFor(nasta);
        if (std::find(stadieIds.begin(), stadieIds.end(), visas) == stadieIds.end()) visas = stadieIds.empty() ? "" : stadieIds[0];
    }
    {
        std::vector<const char*> cv;
        for (auto& n : namn) cv.push_back(n.c_str());
        cv.push_back(nullptr);
        GtkStringList* sl = gtk_string_list_new(cv.data());
        gtk_drop_down_set_model(GTK_DROP_DOWN(stadieVal), G_LIST_MODEL(sl));
        g_object_unref(sl);
        auto it = std::find(stadieIds.begin(), stadieIds.end(), visas);
        if (it != stadieIds.end()) gtk_drop_down_set_selected(GTK_DROP_DOWN(stadieVal), (guint)(it - stadieIds.begin()));
        gtk_widget_set_visible(stadieVal, stadieIds.size() > 1);
    }
    tillampaVisuellStil(visas);  // nivå följer vald kurs, oavsett elevens ålder
    std::vector<const Mal*> mal;
    for (auto& m : data.laroplan()) if (m.stadium == visas) mal.push_back(&m);
    for (auto& omr : data.omradeOrdning()) {
        std::vector<std::string> ids;
        for (auto* m : mal)
            if (m->omrade == omr)
                for (auto& id : m->kort) if (data.kort(id) && std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
        if (ids.empty()) continue;
        std::string nyckel = visas + "/" + omr;
        bool harAktuellt = (aktuellt && std::find(ids.begin(), ids.end(), aktuellt->id) != ids.end()) ||
                           std::find(ids.begin(), ids.end(), nasta) != ids.end();
        bool oppen = !stangdaOmraden.count(nyckel) && (oppnaOmraden.count(nyckel) || harAktuellt);
        int klara = 0;
        for (auto& id : ids) klara += elev.lage(id) == BEHARSKAR;
        // områdets rad: pil, ikon, namn (radbryts – inget kapas), hur många kort som sitter
        GtkWidget* ob = hbox(8);
        gtk_widget_set_margin_top(ob, 4); gtk_widget_set_margin_bottom(ob, 4);
        GtkWidget* pil = gtk_image_new_from_icon_name(oppen ? "pan-down-symbolic" : "pan-end-symbolic");
        gtk_box_append(GTK_BOX(ob), pil);
        auto oi = OMRADE_IKON.find(omr);
        gtk_box_append(GTK_BOX(ob), ikonbild(oi == OMRADE_IKON.end() ? "fragetecken" : oi->second, 22));
        GtkWidget* on = lbl(data.omradeNamn(omr), "matte-omrade-rad");
        gtk_widget_set_hexpand(on, TRUE);
        gtk_box_append(GTK_BOX(ob), on);
        GtkWidget* antal = lbl(std::to_string(klara) + "/" + std::to_string(ids.size()), "dim-label", false);
        gtk_widget_add_css_class(antal, "caption");
        gtk_widget_set_tooltip_text(antal, tr("Mastered cards", "Kort du behärskar"));
        gtk_box_append(GTK_BOX(ob), antal);
        GtkWidget* orad = gtk_list_box_row_new();
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(orad), ob);
        gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(orad), FALSE);
        g_object_set_data_full(G_OBJECT(orad), "omrade", g_strdup(nyckel.c_str()), g_free);
        g_object_set_data(G_OBJECT(orad), "oppen", GINT_TO_POINTER(oppen));
        gtk_accessible_update_state(GTK_ACCESSIBLE(orad), GTK_ACCESSIBLE_STATE_EXPANDED, oppen, -1);
        gtk_list_box_append(GTK_LIST_BOX(lista), orad);
        if (!oppen) continue;
        for (auto& id : ids) {
            const Kort* k = data.kort(id);
            GtkWidget* kb = hbox(8);
            gtk_widget_set_margin_start(kb, 26);
            gtk_widget_set_margin_top(kb, 2); gtk_widget_set_margin_bottom(kb, 2);
            if (!k->ikon.empty()) gtk_box_append(GTK_BOX(kb), ikonbild(k->ikon, 20));
            GtkWidget* t = lbl(k->titel, "matte-kortrad");
            gtk_widget_set_hexpand(t, TRUE);
            gtk_box_append(GTK_BOX(kb), t);
            if (id == nasta && (!aktuellt || aktuellt->id != id)) {
                GtkWidget* n = lbl(tr("Next", "Nästa"), "matte-nasta", false);
                gtk_widget_set_valign(n, GTK_ALIGN_CENTER);
                gtk_box_append(GTK_BOX(kb), n);
            } else if (int st = elev.stjarnor(id); st > 0) {
                GtkWidget* sl = lbl(stjarntext(st), "matte-stjarnor-liten", false);
                gtk_widget_set_valign(sl, GTK_ALIGN_CENTER);
                gtk_box_append(GTK_BOX(kb), sl);
            }
            GtkWidget* r = gtk_list_box_row_new();
            gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(r), kb);
            g_object_set_data_full(G_OBJECT(r), "kort", g_strdup(id.c_str()), g_free);
            gtk_list_box_append(GTK_LIST_BOX(lista), r);
            const char* sida = gtk_stack_get_visible_child_name(GTK_STACK(stack));
            if (aktuellt && aktuellt->id == id && sida && std::string(sida) == "kort") gtk_list_box_select_row(GTK_LIST_BOX(lista), GTK_LIST_BOX_ROW(r));
        }
    }
    // tillbaka till samma ställe i listan (den byggs om efter varje svar)
    g_idle_add_once(+[](gpointer p) {
        double* v = static_cast<double*>(p);
        gtk_adjustment_set_value(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(M->kursRull)), *v);
        delete v;
    }, new double(las));
    fyller = false;
}

std::string Matte::nastaKort() const {
    auto rep = elev.attRepetera();
    if (!rep.empty()) return rep[0];
    for (auto& id : ordning) {
        auto f = elev.kort.find(id);
        if (f == elev.kort.end() || f->second.lage != BEHARSKAR) return id;
    }
    return "";
}

// ---- hem ------------------------------------------------------------------------------------------

// En ruta i systemets kortstil (.card) – rubrik med ikon, innehåll, en knapp längst ned
static GtkWidget* hemRuta(const char* wikon, const std::string& rubrik, GtkWidget** innehall) {
    GtkWidget* r = vbox(10);
    gtk_widget_add_css_class(r, "card");
    gtk_widget_add_css_class(r, "matte-ruta");
    GtkWidget* t = hbox(8);
    if (wikon) gtk_box_append(GTK_BOX(t), wenuy_color_icon_new(wikon, 24));
    gtk_box_append(GTK_BOX(t), lbl(rubrik, "matte-ruta-rubrik", false));
    gtk_box_append(GTK_BOX(r), t);
    *innehall = vbox(8);
    gtk_widget_set_vexpand(*innehall, TRUE);
    gtk_box_append(GTK_BOX(r), *innehall);
    return r;
}

static GtkWidget* rutKnapp(GtkWidget* ruta, const std::string& text, bool huvud, std::function<void()> f) {
    GtkWidget* b = knapp(text, huvud, std::move(f));
    gtk_widget_add_css_class(b, "pill");
    gtk_widget_set_halign(b, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(ruta), b);
    return b;
}

// Hem: hälsningen och rutor – härnäst, dagens tre, repetition, kursen, stjärnorna, regelboken, matteorden.
// Rutorna lägger sig i så många kolumner som får plats (1–3) och fyller bredden.
void Matte::visaHem() {
    franPass = false;
    tyst();
    uppdateraStatus();
    markeraNav("hem");
    GtkWidget* sida = vbox(12);
    gtk_widget_add_css_class(sida, "matte-kort");
    gtk_widget_add_css_class(sida, "matte13-dashboard");
    GtkWidget* topp = hbox(14);
    marg(topp, 16, 18);
    gtk_box_append(GTK_BOX(topp), ikonbild("kulram", 52));
    std::string hej = elev.visningsnamn.empty() || elev.visningsnamn == "elev" ? tr("Hi!", "Hej!")
                                                                                : std::string(tr("Hi, ", "Hej, ")) + elev.visningsnamn + "!";
    GtkWidget* hejL = lbl(hej, "matte-hej", false);
    gtk_widget_set_valign(hejL, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(topp), hejL);
    gtk_box_append(GTK_BOX(sida), topp);

    GtkWidget* rutor = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(rutor), GTK_SELECTION_NONE);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(rutor), TRUE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(rutor), 1);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(rutor), 3);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(rutor), 14);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(rutor), 14);
    marg(rutor, 16, 4);
    auto lagg = [&](GtkWidget* r) {
        gtk_widget_set_size_request(r, 300, -1);
        // radbrytande texter får en rimlig naturlig bredd – annars vill varje ruta ha en hel rad
        std::function<void(GtkWidget*)> kort = [&](GtkWidget* w) {
            if (GTK_IS_LABEL(w) && gtk_label_get_wrap(GTK_LABEL(w))) gtk_label_set_max_width_chars(GTK_LABEL(w), 30);
            for (GtkWidget* c = gtk_widget_get_first_child(w); c; c = gtk_widget_get_next_sibling(c)) kort(c);
        };
        kort(r);
        gtk_flow_box_append(GTK_FLOW_BOX(rutor), r);
        GtkWidget* barn = gtk_widget_get_parent(r);
        gtk_widget_set_focusable(barn, FALSE);
    };
    GtkWidget* forsta = nullptr;
    GtkWidget* in;
    auto n = elev.sammanfattning();
    auto rep = elev.attRepetera();
    std::string nasta = nastaKort();

    // Matte 1.3: en tydlig väg framåt; befintlig placering, progression och kort följer med.
    GtkWidget* matte13Hero = hbox(18);
    gtk_widget_add_css_class(matte13Hero, "matte13-hero");
    gtk_widget_set_hexpand(matte13Hero, TRUE);
    GtkWidget* matte13HeroText = vbox(6);
    gtk_widget_set_hexpand(matte13HeroText, TRUE);
    gtk_box_append(GTK_BOX(matte13HeroText),
        lbl(tr("A little maths every day", "Lite matematik varje dag"), "matte13-hero-title"));
    gtk_box_append(GTK_BOX(matte13HeroText),
        lbl(tr("Continue at your own pace. You can always listen to the question or ask for a hint.",
               "Fortsätt i din egen takt. Du kan alltid lyssna på frågan eller få en ledtråd."),
            "matte13-hero-sub"));
    gtk_box_append(GTK_BOX(matte13Hero), matte13HeroText);
    GtkWidget* matte13HeroButton = nullptr;
    if (const Kort* matte13Next = nasta.empty() ? nullptr : data.kort(nasta)) {
        const std::string matte13Id = matte13Next->id;
        gtk_box_append(GTK_BOX(matte13HeroText),
            lbl(std::string(tr("Up next: ", "Härnäst: ")) + matte13Next->titel,
                "matte13-hero-sub"));
        matte13HeroButton = knapp(tr("Continue", "Fortsätt"), true,
            [matte13Id] { if (auto* k = M->data.kort(matte13Id)) M->visaKort(*k, true); },
            "media-playback-start-symbolic");
    } else {
        matte13HeroButton = knapp(tr("Explore the course", "Utforska kursen"), true,
            [] { M->visaKarta(); }, "view-grid-symbolic");
    }
    gtk_widget_set_valign(matte13HeroButton, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(matte13Hero), matte13HeroButton);
    gtk_box_append(GTK_BOX(sida), matte13Hero);

    // Matte 1.3 etapp 4: översikt baserad på faktisk elevprogression.
    // Inga låtsaspoäng eller krav att avsluta uppgiften innan man kan fortsätta.
    const auto matte13Stat = matte13::gorFramstegOversikt(
        n[BEHARSKAR], n[OVAR], n[LAR], static_cast<int>(rep.size()), elev.allaStjarnor());
    GtkWidget* matte13Panel = vbox(12);
    gtk_widget_add_css_class(matte13Panel, "matte13-dashboard-progress");
    GtkWidget* matte13Rubrik = hbox(8);
    GtkWidget* matte13RubrikText = lbl(tr("Your progress", "Dina framsteg"), "matte13-progress-heading", false);
    gtk_widget_set_hexpand(matte13RubrikText, TRUE);
    gtk_box_append(GTK_BOX(matte13Rubrik), matte13RubrikText);
    gtk_box_append(GTK_BOX(matte13Panel), matte13Rubrik);
    GtkWidget* matte13Sammanfattning = adw_wrap_box_new();
    adw_wrap_box_set_child_spacing(ADW_WRAP_BOX(matte13Sammanfattning), 8);
    adw_wrap_box_set_line_spacing(ADW_WRAP_BOX(matte13Sammanfattning), 8);
    auto matte13Sifferruta = [&](const char* namn, int tal) {
        GtkWidget* k = vbox(5);
        gtk_widget_add_css_class(k, "matte13-progress-stat");
        GtkWidget* sif = lbl(std::to_string(tal), "matte13-progress-stat-num", false);
        gtk_box_append(GTK_BOX(k), sif);
        gtk_box_append(GTK_BOX(k), lbl(namn, "matte13-progress-stat-name"));
        adw_wrap_box_append(ADW_WRAP_BOX(matte13Sammanfattning), k);
    };
    matte13Sifferruta(tr("Mastered", "Behärskar"), matte13Stat.beharskar);
    matte13Sifferruta(tr("Practising", "Övar"), matte13Stat.ovar);
    matte13Sifferruta(tr("To review", "Att repetera"), matte13Stat.repetition);
    matte13Sifferruta(tr("Stars", "Stjärnor"), matte13Stat.stjarnor);
    gtk_box_append(GTK_BOX(matte13Panel), matte13Sammanfattning);
    GtkWidget* matte13Andel = gtk_progress_bar_new();
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(matte13Andel), matte13Stat.andel);
    gtk_widget_add_css_class(matte13Andel, "matte13-progress-bar");
    gtk_accessible_update_property(GTK_ACCESSIBLE(matte13Andel), GTK_ACCESSIBLE_PROPERTY_LABEL,
        tr("Share of registered cards mastered", "Andel registrerade regelkort som du behärskar"), -1);
    gtk_box_append(GTK_BOX(matte13Panel), matte13Andel);
    const int matte13Pct = static_cast<int>(matte13Stat.andel * 100 + 0.5);
    gtk_box_append(GTK_BOX(matte13Panel), lbl(
        std::to_string(matte13Pct) + tr("% of registered cards are mastered.",
                                             "% av registrerade regelkort behärskar du."),
        "matte-svag"));
    GtkWidget* matte13Actions = adw_wrap_box_new();
    gtk_widget_add_css_class(matte13Actions, "matte13-progress-actions");
    adw_wrap_box_set_child_spacing(ADW_WRAP_BOX(matte13Actions), 8);
    adw_wrap_box_set_line_spacing(ADW_WRAP_BOX(matte13Actions), 8);
    adw_wrap_box_append(ADW_WRAP_BOX(matte13Actions),
        knapp(tr("Open course map", "Visa mattekartan"), false, [] { M->visaKarta(); }, "view-grid-symbolic"));
    GtkWidget* matte13Lasstod = knapp(elev.lattlast
        ? tr("Reading support: on", "Lässtöd: på")
        : tr("Reading support: off", "Lässtöd: av"),
        false, [] {
            M->elev.lattlast = !M->elev.lattlast;
            if (!M->elev.spara()) {
                M->toast(tr("Could not save reading settings", "Kunde inte spara lässtödet"));
            }
            M->tillampaLeklage();
            M->tillampaZoom(false);
            g_idle_add_once(+[](gpointer) { M->visaHem(); }, nullptr);
        }, "preferences-desktop-accessibility-symbolic");
    gtk_widget_set_tooltip_text(matte13Lasstod,
        tr("Reading support enlarges text and enables spoken instructions.",
           "Lässtöd förstorar text och läser upp instruktioner."));
    adw_wrap_box_append(ADW_WRAP_BOX(matte13Actions), matte13Lasstod);
    gtk_box_append(GTK_BOX(matte13Panel), matte13Actions);
    gtk_box_append(GTK_BOX(sida), matte13Panel);

    // Första gången: vad heter du?
    bool forstaGangen = elever().size() <= 1 && (elev.visningsnamn.empty() || elev.visningsnamn == "elev");
    if (forstaGangen) {
        GtkWidget* r = hemRuta("info", tr("What is your name?", "Vad heter du?"), &in);
        gtk_box_append(GTK_BOX(in), lbl(tr("Your name is shown in the menu and to your teacher.", "Namnet syns i menyn och för din lärare."), "matte-svag"));
        GtkWidget* falt = gtk_entry_new();
        gtk_entry_set_placeholder_text(GTK_ENTRY(falt), tr("Your name", "Ditt namn"));
        auto spara = [falt]() {
            std::string t = gtk_editable_get_text(GTK_EDITABLE(falt));
            while (!t.empty() && t.back() == ' ') t.pop_back();
            if (t.empty()) return;
            M->elev.visningsnamn = t;
            M->elev.spara();
            M->fyllElevKnapp();
            g_idle_add_once(+[](gpointer) { M->visaHem(); }, nullptr);
        };
        auto* sp = new std::function<void()>(spara);
        g_signal_connect_data(falt, "activate", CB(+[](GtkEntry*, gpointer p) { (*static_cast<std::function<void()>*>(p))(); }), sp,
                              +[](gpointer p, GClosure*) { delete static_cast<std::function<void()>*>(p); }, (GConnectFlags)0);
        gtk_box_append(GTK_BOX(in), falt);
        rutKnapp(r, tr("Save", "Spara"), false, spara);
        lagg(r);
    }
    // Härnäst: placeringen första gången, annars nästa kort
    if (!elev.placerad) {
        GtkWidget* r = hemRuta("help", tr("Where should I start?", "Var ska jag börja?"), &in);
        gtk_box_append(GTK_BOX(in), lbl(tr("A few questions per area find where you should start. Wrong answers are fine – you can skip.",
                                           "Några frågor per område hittar var du ska börja. Fel svar gör inget – du kan hoppa över."), "matte-svag"));
        forsta = rutKnapp(r, tr("How much do I already know?", "Hur mycket kan jag redan?"), true, [] { M->borjaPlacering(); });
        lagg(r);
    }
    if (const Kort* k = nasta.empty() ? nullptr : data.kort(nasta)) {
        GtkWidget* r = hemRuta("next", tr("Next", "Härnäst"), &in);
        GtkWidget* kr = hbox(12);
        gtk_box_append(GTK_BOX(kr), ikonbild(k->ikon.empty() ? "bok" : k->ikon, 56));
        GtkWidget* kt = vbox(2);
        gtk_widget_set_valign(kt, GTK_ALIGN_CENTER);
        GtkWidget* titel = lbl(k->titel, "matte-stor");
        gtk_box_append(GTK_BOX(kt), titel);
        gtk_box_append(GTK_BOX(kt), lbl(data.omradeNamn(k->omrade) + "  " + stjarntext(elev.stjarnor(k->id)), "matte-svag", false));
        gtk_box_append(GTK_BOX(kr), kt);
        gtk_box_append(GTK_BOX(in), kr);
        std::string kid = nasta;
        GtkWidget* b = rutKnapp(r, std::find(rep.begin(), rep.end(), nasta) != rep.end() ? tr("Review", "Repetera") : tr("Practise", "Öva"), !forsta,
                                [kid] { if (auto* k = M->data.kort(kid)) M->visaKort(*k, true); });
        if (!forsta) forsta = b;
        lagg(r);
    }
    if (!elev.dagensKort().empty()) {   // dagens tre: en kort blandad repetition varje dag
        GtkWidget* r = hemRuta("calendar", tr("Today's three", "Dagens tre"), &in);
        bool klar = elev.dagensKlar == idag();
        std::string t = klar ? tr("Done for today – well done!", "Klara för i dag – bra jobbat!")
                             : tr("Three tasks from cards you have met. A little every day makes it stick.",
                                  "Tre uppgifter från kort du mött. Lite varje dag får det att sitta.");
        if (elev.dagensSvit > 1 || (elev.dagensSvit > 0 && elev.dagensKlar == plusDagar(idag(), -1)))
            gtk_box_append(GTK_BOX(in), lbl(std::to_string(elev.dagensSvit) + tr(" days in a row", " dagar i rad"), "matte-ruta-tal", false));
        gtk_box_append(GTK_BOX(in), lbl(t, "matte-svag"));
        if (!klar) { GtkWidget* b = rutKnapp(r, tr("Start", "Börja"), !forsta, [] { M->borjaDagens(); }); if (!forsta) forsta = b; }
        lagg(r);
    }
    if (!rep.empty()) {   // repetition – det är det som får kunskapen att sitta
        GtkWidget* r = hemRuta("update", tr("To review", "Att repetera"), &in);
        gtk_box_append(GTK_BOX(in), lbl(std::to_string(rep.size()), "matte-ruta-tal", false));
        std::string namn;
        for (size_t i = 0; i < rep.size() && i < 3; ++i) if (const Kort* k = data.kort(rep[i])) namn += (namn.empty() ? "" : " · ") + k->titel;
        gtk_box_append(GTK_BOX(in), lbl(namn, "matte-svag"));
        std::string id = rep[0];
        rutKnapp(r, tr("Review", "Repetera"), false, [id] { if (auto* k = M->data.kort(id)) M->visaKort(*k, true); });
        lagg(r);
    }
    // Kursen: en stapel per område på det stadium man är på
    {
        std::string st = nasta.empty() ? (aktuellt ? stadiumFor(aktuellt->id) : "") : stadiumFor(nasta);
        std::string stNamn;
        for (auto& [id, nm] : data.stadier()) if (id == st) stNamn = nm;
        if (!st.empty()) {
            GtkWidget* r = hemRuta("apps", std::string(tr("Your course: ", "Din kurs: ")) + stNamn, &in);
            GtkWidget* g = gtk_grid_new();
            gtk_grid_set_row_spacing(GTK_GRID(g), 8);
            gtk_grid_set_column_spacing(GTK_GRID(g), 10);
            int rad = 0;
            for (auto& omr : data.omradeOrdning()) {
                std::vector<std::string> ids;
                for (auto& m : data.laroplan())
                    if (m.stadium == st && m.omrade == omr)
                        for (auto& id : m.kort) if (data.kort(id) && std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
                if (ids.empty()) continue;
                int klara = 0;
                for (auto& id : ids) klara += elev.lage(id) == BEHARSKAR;
                auto oi = OMRADE_IKON.find(omr);
                gtk_grid_attach(GTK_GRID(g), ikonbild(oi == OMRADE_IKON.end() ? "fragetecken" : oi->second, 20), 0, rad, 1, 1);
                GtkWidget* on = lbl(data.omradeNamn(omr), nullptr, false);
                gtk_label_set_ellipsize(GTK_LABEL(on), PANGO_ELLIPSIZE_END);
                gtk_label_set_xalign(GTK_LABEL(on), 0);
                gtk_widget_set_hexpand(on, TRUE);
                gtk_grid_attach(GTK_GRID(g), on, 1, rad, 1, 1);
                GtkWidget* stapel = gtk_level_bar_new_for_interval(0, (double)ids.size());
                gtk_level_bar_set_value(GTK_LEVEL_BAR(stapel), klara);
                gtk_widget_set_size_request(stapel, 80, -1);
                gtk_widget_set_valign(stapel, GTK_ALIGN_CENTER);
                gtk_grid_attach(GTK_GRID(g), stapel, 2, rad, 1, 1);
                gtk_grid_attach(GTK_GRID(g), lbl(std::to_string(klara) + "/" + std::to_string(ids.size()), "matte-svag", false), 3, rad, 1, 1);
                ++rad;
            }
            gtk_box_append(GTK_BOX(in), g);
            rutKnapp(r, tr("Course map", "Kursens karta"), false, [] { M->visaKarta(); });
            lagg(r);
        }
    }
    // Stjärnorna: samlingen och de senaste korten
    {
        GtkWidget* r = hemRuta("check", tr("Your collection", "Din samling"), &in);
        gtk_box_append(GTK_BOX(in), lbl(std::to_string(elev.allaStjarnor()) + " ★", "matte-ruta-tal", false));
        gtk_box_append(GTK_BOX(in), lbl(std::to_string(n[BEHARSKAR]) + tr(" cards mastered, ", " kort sitter, ") + std::to_string(n[OVAR]) +
                                        tr(" being practised.", " övar du på."), "matte-svag"));
        int visade = 0;
        for (auto it = elev.regelbok.rbegin(); it != elev.regelbok.rend() && visade < 3; ++it, ++visade)
            if (const Kort* k = data.kort(*it)) {
                GtkWidget* rr = hbox(8);
                gtk_box_append(GTK_BOX(rr), ikonbild(k->ikon.empty() ? "bok" : k->ikon, 18));
                GtkWidget* kt = lbl(k->titel, nullptr, false);
                gtk_label_set_ellipsize(GTK_LABEL(kt), PANGO_ELLIPSIZE_END);
                gtk_widget_set_hexpand(kt, TRUE);
                gtk_box_append(GTK_BOX(rr), kt);
                gtk_box_append(GTK_BOX(rr), lbl(stjarntext(elev.stjarnor(k->id)), "matte-stjarnor-liten", false));
                gtk_box_append(GTK_BOX(in), rr);
            }
        rutKnapp(r, tr("My rule book", "Min regelbok"), false, [] { M->visaRegelbok(); });
        lagg(r);
    }
    if (!data.ordlista().empty()) {
        GtkWidget* r = hemRuta("search", tr("Maths words", "Matteorden"), &in);
        gtk_box_append(GTK_BOX(in), lbl(std::to_string(data.ordlista().size()) + tr(" words explained with pictures – täljare, omkrets, median …",
                                                                                    " ord förklarade med bilder – täljare, omkrets, median …"), "matte-svag"));
        rutKnapp(r, tr("Open", "Öppna"), false, [] { M->visaOrdlista(); });
        lagg(r);
    }
    gtk_box_append(GTK_BOX(sida), rutor);
    GtkWidget* klamp = adw_clamp_new();
    adw_clamp_set_maximum_size(ADW_CLAMP(klamp), 1500);
    adw_clamp_set_tightening_threshold(ADW_CLAMP(klamp), 1500);
    adw_clamp_set_child(ADW_CLAMP(klamp), sida);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(hemRull), klamp);
    titel(tr("Home", "Hem"));
    visa("hem");
    if (forsta) gtk_widget_grab_focus(forsta);
}

void Matte::visaInstallningar() {
    tyst();
    markeraNav("installningar");
    GtkWidget* sidan = adw_preferences_page_new();
    gtk_widget_add_css_class(sidan, "matte13-settings");
    std::string visn = elev.visningsnamn.empty() || elev.visningsnamn == "elev" ? "" : elev.visningsnamn;
    adw_preferences_page_set_description(ADW_PREFERENCES_PAGE(sidan),
        tr("The settings apply to this student. Switch student in the menu at the top left.",
           "Inställningarna gäller den här eleven. Byt elev i menyn uppe till vänster."));
    // Eleven
    GtkWidget* g1 = adw_preferences_group_new();
    adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(g1), tr("Student", "Elev"));
    GtkWidget* namn = adw_entry_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(namn), tr("Name", "Namn"));
    gtk_editable_set_text(GTK_EDITABLE(namn), visn.c_str());
    adw_entry_row_set_show_apply_button(ADW_ENTRY_ROW(namn), TRUE);
    g_signal_connect(namn, "apply", CB(+[](AdwEntryRow* r, gpointer) {
        std::string t = gtk_editable_get_text(GTK_EDITABLE(r));
        while (!t.empty() && t.back() == ' ') t.pop_back();
        if (t.empty()) return;
        M->elev.visningsnamn = t;
        M->elev.spara();
        M->fyllElevKnapp();
        M->toast(tr("The name is saved.", "Namnet är sparat."));
    }), nullptr);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(g1), namn);
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(sidan), ADW_PREFERENCES_GROUP(g1));
    // Stöd
    GtkWidget* g2 = adw_preferences_group_new();
    adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(g2), tr("Support", "Stöd"));
    auto brytare = [&](bool pa, const char* rubrik, const char* text, int vilket) {   // 1 lekläge, 0 lättläst, 2 sifferknappar
        GtkWidget* r = wenuy::switchRow(rubrik, text, pa);
        g_object_set_data(G_OBJECT(r), "lek", GINT_TO_POINTER(vilket));
        g_signal_connect(r, "notify::active", CB(+[](AdwSwitchRow* r, GParamSpec*, gpointer) {
            int vilket = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(r), "lek"));
            bool pa = adw_switch_row_get_active(r);
            bool& varde = vilket == 1 ? M->elev.leklage : vilket == 2 ? M->elev.sifferknappar : M->elev.lattlast;
            if (pa == varde) return;
            varde = pa;
            M->elev.spara();
            if (vilket == 2) return;
            g_idle_add_once(vilket == 1 ? +[](gpointer) { M->tillampaLeklage(); M->tillampaZoom(false); M->fyllLista(); }
                                        : +[](gpointer) { M->tillampaLeklage(); M->tillampaZoom(false); }, nullptr);
        }), nullptr);
        adw_preferences_group_add(ADW_PREFERENCES_GROUP(g2), r);
    };
    brytare(elev.lattlast, tr("Easy reading", "Lättläst"),
            tr("For reading difficulties such as dyslexia: bigger text with more space, one sentence per line, the question in bold, "
               "everything read aloud, swapped digits and small spelling mistakes are forgiven",
               "Vid läs- och skrivsvårigheter som dyslexi: större text med mer luft, en mening per rad, frågan i fetstil, "
               "allt läses upp, omkastade siffror och små stavfel förlåts"), 0);
    brytare(elev.sifferknappar, tr("Number buttons", "Sifferknappar"),
            tr("Answer by tapping digits on the screen. They write in the box you are in – also on the working paper",
               "Svara genom att trycka på siffror på skärmen. De skriver i rutan du står i – också på räknepappret"), 2);
    brytare(elev.leklage, tr("Play mode", "Lekläge"),
            tr("For preschool and years 1–3: big buttons, pictures, the questions are read aloud, shorter sessions",
               "För förskola och åk 1–3: stora knappar, bilder, frågorna läses upp, kortare pass"), 1);
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(sidan), ADW_PREFERENCES_GROUP(g2));

    // Visuell nivå följer kursen, inte elevens ålder eller stödbehov.
    GtkWidget* utseende = adw_preferences_group_new();
    adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(utseende),
                                    tr("Appearance", "Utseende"));
    GtkWidget* stilRad = adw_combo_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(stilRad),
                                  tr("Visual style", "Visuell stil"));
    adw_action_row_set_subtitle(ADW_ACTION_ROW(stilRad),
        tr("Automatic follows the course. Easy reading, pictures and handwriting always remain available.",
           "Automatisk följer kursen. Lättläst, bilder och handskrift finns alltid kvar."));
    const char* val[] = {
        tr("Automatic – follows course", "Automatisk – följer kursen"),
        tr("Illustrated", "Illustrerad"),
        tr("Minimal", "Avskalad"), nullptr
    };
    GtkStringList* stilLista = gtk_string_list_new(val);
    adw_combo_row_set_model(ADW_COMBO_ROW(stilRad), G_LIST_MODEL(stilLista));
    g_object_unref(stilLista);
    adw_combo_row_set_selected(ADW_COMBO_ROW(stilRad), visuellStilIndex(elev.visuellStil));
    g_signal_connect(stilRad, "notify::selected", CB(+[](AdwComboRow* row, GParamSpec*, gpointer) {
        std::string nytt = visuellStilVarde(adw_combo_row_get_selected(row));
        if (nytt == M->elev.visuellStil) return;
        const std::string gammal = M->elev.visuellStil;
        M->elev.visuellStil = nytt;
        if (!M->elev.spara()) {
            M->elev.visuellStil = gammal;
            adw_combo_row_set_selected(row, visuellStilIndex(gammal));
            M->toast(tr("Could not save visual style.", "Kunde inte spara visuell stil."));
            return;
        }
        M->tillampaVisuellStil();
    }), nullptr);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(utseende), stilRad);
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(sidan), ADW_PREFERENCES_GROUP(utseende));

    // Text och uppläsning
    GtkWidget* g3 = adw_preferences_group_new();
    adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(g3), tr("Text and voice", "Text och röst"));
    GtkWidget* zoom = adw_spin_row_new_with_range(80, 160, 10);
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(zoom), tr("Text size in the tasks (%)", "Textstorlek i uppgifterna (%)"));
    adw_action_row_set_subtitle(ADW_ACTION_ROW(zoom), tr("Also Ctrl + and Ctrl − while you practise. Bigger screens get bigger text by themselves.",
                                                         "Också Ctrl + och Ctrl − medan du övar. Större skärmar får större text av sig själva."));
    adw_spin_row_set_value(ADW_SPIN_ROW(zoom), std::lround(elev.zoom * 100));
    g_signal_connect(zoom, "notify::value", CB(+[](AdwSpinRow* r, GParamSpec*, gpointer) {
        M->elev.zoom = adw_spin_row_get_value(r) / 100.0;
        M->elev.spara();
        M->tillampaZoom(false);
    }), nullptr);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(g3), zoom);
    {
        std::string r = rostNamn(), t;
        if (r.empty()) t = tr("None found – install espeak-ng to have questions read aloud.", "Ingen hittades – installera espeak-ng för att få frågorna upplästa.");
        else if (r.rfind("Piper", 0) == 0) t = r;
        else t = r + tr(". A more natural Swedish voice: install Piper with an sv_SE voice in ~/.local/share/piper.",
                        ". En naturligare svensk röst: installera Piper med en sv_SE-röst i ~/.local/share/piper.");
        GtkWidget* rost = wenuy::actionRow(tr("Voice", "Röst"), t.c_str());
        adw_action_row_add_prefix(ADW_ACTION_ROW(rost), wenuy_color_icon_new("volume-2", 24));
        adw_preferences_group_add(ADW_PREFERENCES_GROUP(g3), rost);
    }
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(sidan), ADW_PREFERENCES_GROUP(g3));
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(installRull), sidan);
    titel(tr("Settings", "Inställningar"));
    visa("installningar");
}

// Kursens karta: alla kort i ett stadium som rutor, område för område – vad som sitter, pågår och kommer härnäst
void Matte::visaKarta() {
    tyst();
    markeraNav("karta");
    GtkWidget* sida = vbox(4);
    gtk_widget_add_css_class(sida, "matte-kort");
    gtk_widget_add_css_class(sida, "matte13-map");
    std::string nasta = nastaKort();
    std::string visas = valtStadium;
    if (std::find(stadieIds.begin(), stadieIds.end(), visas) == stadieIds.end()) {
        visas = aktuellt ? stadiumFor(aktuellt->id) : nasta.empty() ? "" : stadiumFor(nasta);
        if (std::find(stadieIds.begin(), stadieIds.end(), visas) == stadieIds.end()) visas = stadieIds.empty() ? "" : stadieIds[0];
    }
    GtkWidget* t = hbox(12);
    marg(t, 16, 16);
    GtkWidget* tl = lbl(tr("Course map", "Kursens karta"), "matte-hej", false);
    gtk_widget_set_hexpand(tl, TRUE);
    gtk_box_append(GTK_BOX(t), tl);
    // stadierna som knappar i en grupp (som flikar)
    GtkWidget* grupp = hbox(0);
    gtk_widget_add_css_class(grupp, "linked");
    gtk_widget_set_valign(grupp, GTK_ALIGN_CENTER);
    for (auto& id : stadieIds) {
        std::string nm;
        for (auto& [sid, n] : data.stadier()) if (sid == id) nm = n;
        GtkWidget* b = gtk_toggle_button_new_with_label(nm.c_str());
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b), id == visas);
        std::string sid = id;
        klick(b, [sid] { M->valtStadium = sid; M->fyllLista(); M->visaKarta(); });
        gtk_box_append(GTK_BOX(grupp), b);
    }
    GtkWidget* grs = gtk_scrolled_window_new();   // många stadier: rullas i sidled hellre än att tränga ut fönstret
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(grs), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    gtk_scrolled_window_set_propagate_natural_width(GTK_SCROLLED_WINDOW(grs), TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(grs), grupp);
    gtk_box_append(GTK_BOX(t), grs);
    gtk_box_append(GTK_BOX(sida), t);
    gtk_box_append(GTK_BOX(sida), marg(lbl(tr("Green: it sticks · blue frame: you are practising · Next: the card to do next. Click a card to open it.",
                                              "Grönt: det sitter · blå ram: du övar · Nästa: kortet att göra härnäst. Klicka på ett kort för att öppna det."),
                                           "matte-svag"), 16, 0));
    // Etapp 7: kartans andel gäller bara korten i det valda stadiet, varje kort en gång.
    const auto kartaRep = elev.attRepetera();
    std::set<std::string> kartaSet;
    std::vector<matte13::KartaKort> kartaPoster;
    auto kartaPost = [&](const std::string& id) -> matte13::KartaKort {
        const int lage = elev.lage(id);
        const auto status = lage == BEHARSKAR ? matte13::KartaLage::Beharskar
                           : lage == OVAR ? matte13::KartaLage::Ovar : matte13::KartaLage::Lar;
        const bool repeteras = std::find(kartaRep.begin(), kartaRep.end(), id) != kartaRep.end();
        return {status, id == nasta, repeteras};
    };
    for (const auto& rad : data.laroplan()) {
        if (rad.stadium != visas) continue;
        for (const auto& id : rad.kort) {
            if (data.kort(id) && kartaSet.insert(id).second)
                kartaPoster.push_back(kartaPost(id));
        }
    }
    const auto kartaStat = matte13::summeraKarta(kartaPoster);
    gtk_box_append(GTK_BOX(sida), matte13::skapaKartaOversikt(kartaStat, wenuy::swedish()));
    GtkWidget* kartaFilterRad = hbox(12);
    gtk_widget_add_css_class(kartaFilterRad, "matte13-kartafilter");
    GtkWidget* kartaFilterRubrik = lbl(tr("Show", "Visa"), nullptr, false);
    gtk_widget_set_valign(kartaFilterRubrik, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(kartaFilterRad), kartaFilterRubrik);
    const char* kartaFilterTexter[] = {
        tr("All lessons", "Alla regelkort"),
        tr("Practising and next", "Pågår och nästa"),
        tr("To review", "Att repetera"),
        tr("Mastered", "Behärskar"), nullptr
    };
    GtkWidget* kartaValj = gtk_drop_down_new_from_strings(kartaFilterTexter);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(kartaValj), static_cast<guint>(kartaFilter));
    gtk_accessible_update_property(GTK_ACCESSIBLE(kartaValj), GTK_ACCESSIBLE_PROPERTY_LABEL,
        tr("Filter the learning path", "Filtrera lärandekartan"), -1);
    g_signal_connect(kartaValj, "notify::selected", CB(+[](GtkDropDown* val, GParamSpec*, gpointer) {
        const int vald = static_cast<int>(gtk_drop_down_get_selected(val));
        if (vald == M->kartaFilter) return;
        M->kartaFilter = vald;
        g_idle_add_once(+[](gpointer) { M->visaKarta(); }, nullptr);
    }), nullptr);
    gtk_box_append(GTK_BOX(kartaFilterRad), kartaValj);
    gtk_box_append(GTK_BOX(sida), kartaFilterRad);
    const auto filter = matte13::kartaFilter(kartaFilter);
    if (matte13::antalSynliga(kartaPoster, filter) == 0) {
        GtkWidget* inget = lbl(tr("No cards in this view yet. Select 'All lessons' to see your path.",
            "Här finns inga regelkort än. Välj 'Alla regelkort' för att se hela vägen."), "matte13-karta-tom");
        gtk_box_append(GTK_BOX(sida), inget);
    }
    for (auto& omr : data.omradeOrdning()) {
        std::vector<std::string> ids;
        for (auto& m : data.laroplan())
            if (m.stadium == visas && m.omrade == omr)
                for (auto& id : m.kort) if (data.kort(id) && std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
        if (ids.empty()) continue;
        // Filtrera i varje delområde utan att ändra elevens framsteg.
        int synliga = 0;
        for (const auto& id : ids) if (matte13::synligtKort(kartaPost(id), filter)) ++synliga;
        if (!synliga) continue;
        int klara = 0;
        for (auto& id : ids) klara += elev.lage(id) == BEHARSKAR;
        GtkWidget* oh = hbox(10);
        gtk_widget_add_css_class(oh, "matte-karta-omrade");
        auto oi = OMRADE_IKON.find(omr);
        gtk_box_append(GTK_BOX(oh), ikonbild(oi == OMRADE_IKON.end() ? "fragetecken" : oi->second, 28));
        gtk_box_append(GTK_BOX(oh), lbl(data.omradeNamn(omr), nullptr, false));
        gtk_box_append(GTK_BOX(oh), lbl(std::to_string(klara) + tr(" of ", " av ") + std::to_string(ids.size()), "matte-svag", false));
        gtk_box_append(GTK_BOX(sida), oh);
        GtkWidget* fb = gtk_flow_box_new();
        gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(fb), GTK_SELECTION_NONE);
        gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(fb), TRUE);
        gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(fb), 4);
        gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(fb), 10);
        gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(fb), 10);
        marg(fb, 16, 0);
        for (auto& id : ids) {
            if (!matte13::synligtKort(kartaPost(id), filter)) continue;
            const Kort* k = data.kort(id);
            int lage = elev.lage(id);
            const Kortstatus* ks = elev.kort.count(id) ? &elev.kort.at(id) : nullptr;
            bool pagar = lage == OVAR || (ks && ks->antal > 0 && lage != BEHARSKAR);
            const char* klass = lage == BEHARSKAR ? "klar" : id == nasta ? "nasta" : pagar ? "pagar" : "ny";
            const char* chip = lage == BEHARSKAR ? tr("It sticks", "Sitter") : id == nasta ? tr("Next", "Nästa") : pagar ? tr("Practising", "Övar") : tr("New", "Ny");
            GtkWidget* b = gtk_button_new();
            gtk_widget_add_css_class(b, "card");
            gtk_widget_add_css_class(b, "matte-kartruta");
            gtk_widget_add_css_class(b, "matte13-vagkort");
            gtk_widget_add_css_class(b, klass);
            GtkWidget* bb = vbox(6);
            GtkWidget* rad1 = hbox(8);
            gtk_box_append(GTK_BOX(rad1), lbl("●", "matte13-vagpunkt", false));
            gtk_box_append(GTK_BOX(rad1), ikonbild(k->ikon.empty() ? "bok" : k->ikon, 36));
            GtkWidget* fyll = hbox(0);
            gtk_widget_set_hexpand(fyll, TRUE);
            gtk_box_append(GTK_BOX(rad1), fyll);
            GtkWidget* c = lbl(chip, "matte-chip", false);
            gtk_widget_add_css_class(c, klass);
            gtk_widget_set_valign(c, GTK_ALIGN_START);
            gtk_box_append(GTK_BOX(rad1), c);
            gtk_box_append(GTK_BOX(bb), rad1);
            GtkWidget* tt = lbl(k->titel, "matte-rubrik");
            gtk_label_set_lines(GTK_LABEL(tt), 2);
            gtk_label_set_ellipsize(GTK_LABEL(tt), PANGO_ELLIPSIZE_END);
            gtk_box_append(GTK_BOX(bb), tt);
            gtk_box_append(GTK_BOX(bb), lbl(stjarntext(elev.stjarnor(id)), "matte-stjarnor-liten", false));
            gtk_button_set_child(GTK_BUTTON(b), bb);
            const std::string tillganglig = k->titel + ". " + chip + ". " +
                std::to_string(elev.stjarnor(id)) + tr(" out of 3 stars", " av 3 stjärnor");
            gtk_accessible_update_property(GTK_ACCESSIBLE(b), GTK_ACCESSIBLE_PROPERTY_LABEL,
                tillganglig.c_str(), -1);
            gtk_widget_set_size_request(b, 180, -1);
            std::string kid = id;
            klick(b, [kid] { if (auto* k = M->data.kort(kid)) M->visaKort(*k); });
            gtk_flow_box_append(GTK_FLOW_BOX(fb), b);
            gtk_widget_set_focusable(gtk_widget_get_parent(b), FALSE);
        }
        gtk_box_append(GTK_BOX(sida), fb);
    }
    GtkWidget* klamp = adw_clamp_new();
    adw_clamp_set_maximum_size(ADW_CLAMP(klamp), 1600);
    adw_clamp_set_tightening_threshold(ADW_CLAMP(klamp), 1600);
    adw_clamp_set_child(ADW_CLAMP(klamp), sida);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(kartaRull), klamp);
    titel(tr("Course map", "Kursens karta"));
    visa("karta");
}

void Matte::visaRegelbok() {
    tyst();
    markeraNav("regelbok");
    GtkWidget* klamp = kortyta();
    GtkWidget* box = inre(klamp);
    GtkWidget* t = hbox(10);
    gtk_widget_add_css_class(t, "matte-titel");
    gtk_box_append(GTK_BOX(t), ikonbild("bok", 32));
    GtkWidget* tl = lbl(tr("My rule book", "Min regelbok"), nullptr, false);
    gtk_widget_set_hexpand(tl, TRUE);
    gtk_box_append(GTK_BOX(t), tl);
    gtk_box_append(GTK_BOX(t), lbl(std::to_string(elev.allaStjarnor()) + " ★", "matte-stjarnor", false));
    gtk_box_append(GTK_BOX(box), t);
    if (elev.regelbok.empty()) {
        gtk_box_append(GTK_BOX(box), wenuy_empty_state("note", tr("Empty so far", "Tom än så länge"),
                                                       tr("Every card you work with ends up here – your own maths book.",
                                                          "Varje kort du arbetar med hamnar här – din egen mattebok."),
                                                       tr("To the course map", "Till kursens karta"),
                                                       G_CALLBACK(+[](GtkButton*, gpointer) { M->visaKarta(); }), nullptr));
    } else {
        gtk_box_append(GTK_BOX(box), marg(lbl(tr("The cards you have worked with, in the order you met them. ★ a right answer · ★★ mastered · ★★★ it sticks (reviewed).",
                                                 "Korten du arbetat med, i den ordning du mötte dem. ★ ett rätt svar · ★★ behärskar · ★★★ det sitter (repeterat)."), "matte-svag"), 16, 8));
        GtkWidget* flode = adw_wrap_box_new();
        adw_wrap_box_set_child_spacing(ADW_WRAP_BOX(flode), 8);
        adw_wrap_box_set_line_spacing(ADW_WRAP_BOX(flode), 8);
        marg(flode, 16, 10);
        for (auto& id : elev.regelbok) {
            const Kort* k = data.kort(id);
            if (!k) continue;
            GtkWidget* b = gtk_button_new();
            GtkWidget* bb = hbox(8);
            auto oi = OMRADE_IKON.find(k->omrade);
            gtk_box_append(GTK_BOX(bb), ikonbild(!k->ikon.empty() ? k->ikon : oi == OMRADE_IKON.end() ? "bok" : oi->second, 26));
            GtkWidget* tb = vbox(0);
            gtk_box_append(GTK_BOX(tb), lbl(k->titel, nullptr, false));
            gtk_box_append(GTK_BOX(tb), lbl(stjarntext(elev.stjarnor(id)), "matte-stjarnor-liten", false));
            gtk_box_append(GTK_BOX(bb), tb);
            gtk_button_set_child(GTK_BUTTON(b), bb);
            std::string kid = id;
            klick(b, [kid] { if (auto* k = M->data.kort(kid)) M->visaKort(*k, true); });
            if (nyaBeharskade.count(id)) {
                gtk_widget_add_css_class(b, "matte-ny");
                gtk_widget_set_tooltip_text(b, tr("New in your rule book", "Nytt i din regelbok"));
            }
            adw_wrap_box_append(ADW_WRAP_BOX(flode), b);
        }
        gtk_box_append(GTK_BOX(box), flode);
        GtkWidget* skriv = knapp(tr("Print my rule book", "Skriv ut min regelbok"), false, [] { M->skrivRegelbok(false); }, "printer-symbolic");
        gtk_widget_set_tooltip_text(skriv, tr("All your cards as a PDF in Documents/Matte", "Alla dina kort som PDF i Dokument/Matte"));
        gtk_widget_set_halign(skriv, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(box), marg(skriv, 16, 16));
    }
    gtk_widget_set_margin_bottom(box, 20);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(regelbokRull), klamp);
    titel(tr("My rule book", "Min regelbok"));
    visa("regelbok");
}

void Matte::bytElev(const std::string& namn) {
    if (!sparaPapper()) return;
    tyst();
    papperByte = true;
    elev = Elev::ladda(namn);
    uppgift = Uppgift{};
    papperAterupptaVidStart = false;
    raknepapper_ny_session(papper, "", "", "", -1, -1);
    papperByte = false;
    if (papperStatus) gtk_label_set_text(GTK_LABEL(papperStatus), tr("Not saved yet", "Inte sparat än"));
    sattSenasteElev(namn);
    aktuellt = nullptr;
    passAktivt = false;
    valtStadium.clear(); oppnaOmraden.clear(); stangdaOmraden.clear();
    nyaBeharskade.clear();
    tillampaLeklage(); fyllElevKnapp(); fyllLista(); uppdateraStatus(); visaHem();
    toast(std::string(tr("Hello ", "Hej ")) + (elev.visningsnamn.empty() ? namn : elev.visningsnamn) + "!");
}

// ---- regelkortet ----------------------------------------------------------------------------------

// Kortet: regeln och uppgifterna på samma sida. Att öppna ett kort börjar ett pass direkt – första uppgiften
// står under (eller bredvid) regeln. Samma kort igen mitt i ett pass = samma uppgift som förut.
void Matte::visaKort(const Kort& k, bool nyttPass) {
    if (!sparaPapper()) return;
    tyst();
    bool samma = aktuellt == &k && passAktivt && uppgift.v && passKort.empty() && !nyttPass;
    papperAterupptaVidStart = !nyttPass && !samma &&
        elev.papperKort == k.id && giltigtPapperId(elev.papperId);
    if (!samma) loggaAvbrott();
    if (aktuellt != &k) {
        // Yngre elever får hela introduktionen först; högstadiet/gymnasiet
        // börjar med kärnan och ser uppgiften direkt. Hela regeln kan öppnas.
        const auto niva = visuellNiva(stadiumFor(k.id), elev.visuellStil);
        regelHel = elev.lage(k.id) == LAR &&
                   (niva == VisuellNiva::Lekfull || niva == VisuellNiva::Lugn);
    }
    aktuellt = &k;
    elev.settKort(k.id);
    elev.spara();
    valtStadium.clear();   // kursen i menyn följer kortet
    tillampaVisuellStil(stadiumFor(k.id));
    markeraNav("");
    if (samma) {
        visa("kort");
        fyllLista();
        visaRegel();
        titel(std::string(tr("Practise", "Öva")) + " · " + k.titel);
        return;
    }
    if (data.mallar(k.id).empty()) {   // ett kort utan uppgifter: bara regeln
        passAktivt = false;
        uppgift = Uppgift{};
        gtk_widget_set_visible(ovaKlamp, FALSE);
        visa("kort");
        fyllLista();
        visaRegel();
        titel(k.titel);
        return;
    }
    gtk_widget_set_visible(ovaKlamp, TRUE);
    borjaOva();
}

// Regeln: brett fönster – hela regeln i en egen kolumn till vänster. Smalt – ovanför uppgiften, hel eller bara
// kärnan (rubrik, meningen, gäller) beroende på om eleven lär sig kortet eller redan övar på det.
void Matte::visaRegel() {
    if (!aktuellt || !regelPlats) return;
    const Kort& k = *aktuellt;
    if (regelVy) { if (GtkWidget* f = gtk_widget_get_parent(regelVy)) gtk_box_remove(GTK_BOX(f), regelVy); regelVy = nullptr; }
    bool iKolumn = layout >= 2 && regelKolumn && !helskarm;
    smal = !iKolumn;
    gtk_widget_set_visible(regelRull, iKolumn);
    if (GtkWidget* d = static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(uppgKort), "delning")))
        gtk_widget_set_visible(d, !iKolumn && !helskarm);   // strecket mellan regeln och uppgiften – bara när de står ovanför varandra
    if (helskarm) { synkaVerktyg(); return; }   // helskärm: bara uppgiften och pappret
    bool hel = iKolumn || regelHel || !uppgift.v;
    regelLasDelar = std::make_shared<std::vector<Lasdel>>();
    auto las = regelLasDelar;
    GtkWidget* klamp = kortyta();
    GtkWidget* box = inre(klamp);
    gtk_widget_add_css_class(box, "matte-regel-kort");
    if (smal) gtk_widget_set_margin_bottom(box, 0);
    GtkWidget* t = hbox(10);
    gtk_widget_add_css_class(t, "matte-titel");
    if (!k.ikon.empty()) gtk_box_append(GTK_BOX(t), ikonbild(k.ikon, 32));
    GtkWidget* titelL = lasbar(k.titel, nullptr, *las);
    gtk_widget_set_hexpand(titelL, TRUE);
    gtk_box_append(GTK_BOX(t), titelL);
    GtkWidget* st = lbl(stjarntext(elev.stjarnor(k.id)), "matte-stjarnor", false);
    gtk_widget_set_tooltip_text(st, tr("★ a right answer · ★★ mastered · ★★★ it sticks (reviewed)",
                                       "★ ett rätt svar · ★★ behärskar · ★★★ det sitter (repeterat)"));
    const auto niv = visuellNiva(stadiumFor(k.id), elev.visuellStil);
    // Belöningsstjärnor finns kvar i elevens framsteg men tar inte plats
    // i den mer professionella lektionsvyn för högstadiet/gymnasiet.
    gtk_widget_set_visible(st, niv != VisuellNiva::Akademisk &&
                               niv != VisuellNiva::Professionell);
    gtk_box_append(GTK_BOX(t), st);
    GtkWidget* lasKnapp = gtk_button_new_from_icon_name("audio-volume-high-symbolic");
    gtk_widget_add_css_class(lasKnapp, "circular");
    gtk_widget_set_valign(lasKnapp, GTK_ALIGN_CENTER);
    gtk_widget_set_tooltip_text(lasKnapp, tr("Read the rule aloud", "Läs upp regeln"));
    klick(lasKnapp, [las] {
        logga(M->elev.namn, "lasupp", Json::objekt().satt("kort", M->aktuellt ? M->aktuellt->id : ""));
        M->lasMarkerat(*las);
    });
    gtk_box_append(GTK_BOX(t), lasKnapp);
    gtk_box_append(GTK_BOX(box), t);
    gtk_box_append(GTK_BOX(box), marg(lbl(std::string(tr("The rule · ", "Regeln · ")) + data.omradeNamn(k.omrade), "matte-svag"), 16, 4));

    // På små skärmar kan en bild och en hel regel fylla flera skärmhöjder.
    // Lägg genvägen till uppgiften OVANFÖR bilden så att eleven slipper leta
    // efter den befintliga nedre knappen och alltid kan börja öva direkt.
    if (smal && hel && uppgift.v) {
        GtkWidget* till = knapp(tr("Go to the task", "Till uppgiften"), true,
                                [] { M->tillUppgiften(); }, "go-down-symbolic");
        gtk_widget_set_halign(till, GTK_ALIGN_START);
        gtk_widget_set_margin_start(till, 16);
        gtk_widget_set_margin_bottom(till, 6);
        gtk_box_append(GTK_BOX(box), till);
    }

    if (hel && !k.bild.empty()) {   // målningen först – verkligheten före staplarna och symbolerna
        Json scen = Json::objekt();
        scen.satt("typ", "scen"); scen.satt("bild", k.bild); scen.satt("bredd", smal ? 640 : 560);
        GtkWidget* f = figurvy(data, scen, "", 1.0);
        marg(f, 16, 8);
        gtk_box_append(GTK_BOX(box), f);
        if (!k.bildtext.empty()) gtk_box_append(GTK_BOX(box), marg(lbl(k.bildtext, "matte-svag"), 16, 2));
    }
    if (!k.sager.empty()) gtk_box_append(GTK_BOX(box), lasbar(k.sager, "matte-sager", *las));

    if (hel) {
        for (auto& aid : k.ateruppta) {   // "Minns du?" – regler som slutade gälla
            const Kort* a = data.kort(aid);
            if (a && !a->galler_inte.empty() && std::find(elev.regelbok.begin(), elev.regelbok.end(), aid) != elev.regelbok.end())
                gtk_box_append(GTK_BOX(box), lbl(std::string(tr("Remember ", "Minns du ")) + a->titel + "? " + a->galler_inte, "matte-minns"));
        }
        if (!k.rader.empty()) {
            bool tre = std::all_of(k.rader.begin(), k.rader.end(), [](const Rad& r) { return r.skriver.size() == 3; });
            GtkWidget* g = gtk_grid_new();
            gtk_grid_set_row_spacing(GTK_GRID(g), 8);
            gtk_grid_set_column_spacing(GTK_GRID(g), 18);
            marg(g, 16, 10);
            std::vector<std::string> rub = tre ? std::vector<std::string>{"", tr("Fraction", "Bråkform"), tr("Percent", "Procentform"), tr("Decimal", "Decimalform")}
                                               : std::vector<std::string>{tr("We say", "Man säger"), "", tr("We write", "Man skriver")};
            for (size_t i = 0; i < rub.size(); ++i) gtk_grid_attach(GTK_GRID(g), lbl(rub[i], "matte-rubrik", false), (int)i, 0, 1, 1);
            int ri = 1;
            for (auto& r : k.rader) {
                if (!tre) {
                    GtkWidget* s = lasbar(r.sager, nullptr, *las);
                    gtk_label_set_max_width_chars(GTK_LABEL(s), 22);
                    gtk_grid_attach(GTK_GRID(g), s, 0, ri, 1, 1);
                }
                GtkWidget* fig = r.figur.arObjekt() ? figurvy(data, r.figur) : gtk_label_new("");
                gtk_grid_attach(GTK_GRID(g), fig, tre ? 0 : 1, ri, 1, 1);
                if (tre) {
                    for (size_t c = 0; c < r.skriver.size(); ++c) {
                        GtkWidget* l = lbl(r.skriver[c], "matte-skriver", false);
                        gtk_label_set_xalign(GTK_LABEL(l), 0.5);
                        gtk_grid_attach(GTK_GRID(g), l, (int)c + 1, ri, 1, 1);
                    }
                } else {
                    std::string s;
                    for (auto& x : r.skriver) s += (s.empty() ? "" : "\n") + x;
                    GtkWidget* l = lasbar(s, "matte-skriver", *las);
                    gtk_label_set_max_width_chars(GTK_LABEL(l), 30);
                    gtk_grid_attach(GTK_GRID(g), l, 2, ri, 1, 1);
                }
                ++ri;
                if (!r.kommentar.empty()) { gtk_grid_attach(GTK_GRID(g), lbl(r.kommentar, "matte-svag"), 0, ri, 4, 1); ++ri; }
            }
            gtk_box_append(GTK_BOX(box), g);
        }
        for (auto& e : k.exempel) {   // lösta exempel, steg för steg
            GtkWidget* eb = vbox(6);
            gtk_widget_add_css_class(eb, "matte-exempel");
            if (!e.rubrik.empty()) gtk_box_append(GTK_BOX(eb), lasbar(e.rubrik, "matte-rubrik", *las));
            if (!e.text.empty()) gtk_box_append(GTK_BOX(eb), lasbar(e.text, nullptr, *las));
            if (e.figur.arObjekt()) gtk_box_append(GTK_BOX(eb), figurvy(data, e.figur));
            for (size_t i = 0; i < e.steg.size(); ++i) gtk_box_append(GTK_BOX(eb), lasbar(std::to_string(i + 1) + ". " + e.steg[i], nullptr, *las));
            gtk_box_append(GTK_BOX(box), eb);
        }
    }
    if (!k.galler.empty()) gtk_box_append(GTK_BOX(box), lasbar(std::string(tr("Applies: ", "Gäller: ")) + k.galler, "matte-galler", *las));
    if (hel && !k.galler_inte.empty()) gtk_box_append(GTK_BOX(box), lasbar(std::string(tr("Stops applying: ", "Slutar gälla: ")) + k.galler_inte, "matte-galler-inte", *las));

    GtkWidget* knappar = adw_wrap_box_new();
    adw_wrap_box_set_child_spacing(ADW_WRAP_BOX(knappar), 8);
    adw_wrap_box_set_line_spacing(ADW_WRAP_BOX(knappar), 8);
    marg(knappar, 16, 12);
    if (smal && uppgift.v) {
        GtkWidget* vaxla = knapp(hel ? tr("Show less of the rule", "Visa mindre av regeln") : tr("Show the whole rule", "Visa hela regeln"), false,
                                 [] { M->regelHel = !M->regelHel; M->visaRegel(); }, hel ? "pan-up-symbolic" : "pan-down-symbolic");
        adw_wrap_box_append(ADW_WRAP_BOX(knappar), vaxla);
        if (hel) {
            GtkWidget* ner = knapp(tr("To the task", "Till uppgiften"), true, [] { M->tillUppgiften(); }, "pan-down-symbolic");
            adw_wrap_box_append(ADW_WRAP_BOX(knappar), ner);
        }
    }
    if (gtk_widget_get_first_child(knappar)) gtk_box_append(GTK_BOX(box), knappar);
    if (hel && !k.kalla.empty() && !elev.leklage) gtk_box_append(GTK_BOX(box), marg(lbl(std::string(tr("Based on: ", "Bygger på: ")) + k.kalla, "matte-svag"), 16, 6));

    if (elev.lattlast) { adw_clamp_set_maximum_size(ADW_CLAMP(klamp), 760); adw_clamp_set_tightening_threshold(ADW_CLAMP(klamp), 760); }
    regelVy = klamp;
    gtk_box_append(GTK_BOX(smal ? regelInline : regelPlats), klamp);
    synkaVerktyg();
}

// Smalt fönster: rulla ned så att uppgiftens rubrik står överst
void Matte::tillUppgiften() {
    if (!smal || !uppgKort) return;
    g_idle_add_once(+[](gpointer) {
        graphene_point_t p, noll = GRAPHENE_POINT_INIT(0, 0);
        GtkWidget* kol = gtk_widget_get_parent(M->ovaKlamp);
        if (!kol || !gtk_widget_compute_point(M->ovaKlamp, kol, &noll, &p)) return;
        GtkAdjustment* a = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(M->uppgRull));
        gtk_adjustment_set_value(a, std::max(0.0, (double)p.y + 8));
    }, nullptr);
}

// ---- verktygsraden, layouten, zoom, helskärm -------------------------------------------------------

static GtkWidget* verktygKnapp(GtkWidget* rad, const char* ikon, const char* text, const char* tips, bool toggle, std::function<void(GtkWidget*)> f) {
    GtkWidget* b = toggle ? gtk_toggle_button_new() : gtk_button_new();
    GtkWidget* bb = hbox(6);
    if (ikon) gtk_box_append(GTK_BOX(bb), wenuy_icon_new(ikon, 18));
    if (text) gtk_box_append(GTK_BOX(bb), gtk_label_new(text));
    gtk_button_set_child(GTK_BUTTON(b), bb);
    gtk_widget_add_css_class(b, "flat");
    gtk_widget_set_focus_on_click(b, FALSE);   // fokus stannar i svaret eller pappret
    if (tips) gtk_widget_set_tooltip_text(b, tips);
    auto* fn = new std::function<void(GtkWidget*)>(std::move(f));
    g_signal_connect_data(b, toggle ? "toggled" : "clicked", G_CALLBACK(+[](GtkWidget* w, gpointer p) {
        if (M->synkarVerktyg) return;
        (*static_cast<std::function<void(GtkWidget*)>*>(p))(w);
    }), fn, +[](gpointer p, GClosure*) { delete static_cast<std::function<void(GtkWidget*)>*>(p); }, (GConnectFlags)0);
    adw_wrap_box_append(ADW_WRAP_BOX(rad), b);
    return b;
}

// En rad under titelraden (som i Rita): det man gör med kortet och uppgiften
void Matte::byggVerktyg() {
    verktyg = adw_wrap_box_new();
    gtk_widget_add_css_class(verktyg, "matte13-toolbar");   // radbryts i ett smalt fönster i stället för att tränga ut fönstret
    adw_wrap_box_set_child_spacing(ADW_WRAP_BOX(verktyg), 2);
    adw_wrap_box_set_line_spacing(ADW_WRAP_BOX(verktyg), 2);
    gtk_widget_add_css_class(verktyg, "toolbar");
    gtk_widget_add_css_class(verktyg, "matte-verktyg");
    vRegel = verktygKnapp(verktyg, "note", tr("Rule", "Regeln"), tr("Show the rule – next to the task or all of it above", "Visa regeln – bredvid uppgiften eller hela ovanför"),
                          true, [](GtkWidget* b) {
        bool pa = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b));
        if (M->layout >= 2 && !M->helskarm) M->regelKolumn = pa; else M->regelHel = pa;
        M->visaRegel();
        if (M->smal && !pa) M->tillUppgiften();
    });
    verktygKnapp(verktyg, "volume-2", tr("Read aloud", "Läs upp"), tr("Read the task aloud", "Läs upp uppgiften"), false, [](GtkWidget*) {
        logga(M->elev.namn, "lasupp", M->uppgiftsinfo()); M->lasUppUppgift();
    });
    vLedtrad = verktygKnapp(verktyg, "help", tr("Hint", "Ledtråd"), tr("A hint for this task", "En ledtråd till uppgiften"), false, [](GtkWidget*) { M->ledtrad(); });
    vPapper = verktygKnapp(verktyg, "region", tr("Working paper", "Räknepapper"), tr("Squared paper to calculate on – one digit per square",
                                                                                   "Rutat papper att räkna på – en siffra per ruta"), true, [](GtkWidget* b) {
        M->papperVal = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b)) ? 1 : 0;
        M->papperVisas = M->papperVal == 1;
        M->ordnaLayout(false);
        if (M->papperVisas) gtk_widget_grab_focus(M->papper);
    });
    GtkWidget* blad = verktygKnapp(verktyg, "report", tr("Worksheet", "Arbetsblad"), tr("New tasks to print, with answers on a separate page (PDF in Documents/Matte)",
                                                                                    "Nya uppgifter att skriva ut, med facit på en egen sida (PDF i Dokument/Matte)"),
                                   false, [](GtkWidget*) { if (M->aktuellt) M->skrivArbetsblad(*M->aktuellt); });
    (void)blad;
    adw_wrap_box_append(ADW_WRAP_BOX(verktyg), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    verktygKnapp(verktyg, nullptr, "A−", tr("Smaller text (Ctrl −)", "Mindre text (Ctrl −)"), false, [](GtkWidget*) {
        M->elev.zoom = std::max(0.8, M->elev.zoom - 0.1); M->elev.spara(); M->tillampaZoom(true);
    });
    vZoom = lbl("100 %", "matte-zoom", false);
    gtk_label_set_xalign(GTK_LABEL(vZoom), 0.5);
    gtk_widget_set_tooltip_text(vZoom, tr("Text size (Ctrl 0 = 100 %)", "Textstorlek (Ctrl 0 = 100 %)"));
    adw_wrap_box_append(ADW_WRAP_BOX(verktyg), vZoom);
    verktygKnapp(verktyg, nullptr, "A+", tr("Bigger text (Ctrl +)", "Större text (Ctrl +)"), false, [](GtkWidget*) {
        M->elev.zoom = std::min(1.6, M->elev.zoom + 0.1); M->elev.spara(); M->tillampaZoom(true);
    });
    adw_wrap_box_append(ADW_WRAP_BOX(verktyg), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    vFokus = verktygKnapp(verktyg, "center", tr("Focus", "Fokus"), tr("Hide the menu while you practise (Esc brings it back)",
                                                                       "Göm menyn medan du övar (Esc tar fram den igen)"), true, [](GtkWidget* b) {
        M->satFokus(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b)));
    });
    vHel = verktygKnapp(verktyg, "maximize", tr("Full screen", "Helskärm"), tr("Only the task and the working paper (F11, Esc to leave)",
                                                                              "Bara uppgiften och räknepappret (F11, Esc för att gå ur)"), true, [](GtkWidget* b) {
        M->satHelskarm(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b)));
    });
    gtk_widget_set_visible(verktyg, FALSE);
    wenuy_app_window_add_top_bar(win, verktyg);
}

void Matte::synkaVerktyg() {
    if (!verktyg) return;
    synkarVerktyg = true;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(vRegel), layout >= 2 && !helskarm ? regelKolumn : regelHel);
    gtk_widget_set_sensitive(vRegel, !helskarm && aktuellt);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(vPapper), papperVisas);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(vFokus), fokusLage);
    gtk_widget_set_sensitive(vFokus, !helskarm);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(vHel), helskarm);
    gtk_widget_set_sensitive(vLedtrad, uppgift.v && !uppgift.v->ledtrad.empty());
    gtk_label_set_text(GTK_LABEL(vZoom), (std::to_string((int)std::lround(elev.zoom * 100)) + " %").c_str());
    synkarVerktyg = false;
}

void Matte::ledtrad() {
    if (!uppgift.v) return;
    logga(elev.namn, "ledtrad", uppgiftsinfo());
    if (uppgift.v->ledtrad.empty()) return;
    tom(ovaSvarIkon);
    gtk_label_set_text(GTK_LABEL(ovaSvarText), uppgift.v->ledtrad.c_str());
    gtk_widget_set_css_classes(ovaSvarText, nullptr);
    gtk_widget_add_css_class(ovaSvarText, "matte-svag");
    if (lasesUpp()) lasUpp(uppgift.v->ledtrad);
}

// Var regeln och räknepappret står, efter sidans bredd och vad eleven valt
void Matte::ordnaLayout(bool nyLayout) {
    if (!papperDel) return;
    bool tva = layout >= 1 && !(helskarm && false);
    GtkWidget* ska = tva ? papperKolumn : ovaRakna;
    if (gtk_widget_get_parent(papperDel) != ska) {
        if (GtkWidget* f = gtk_widget_get_parent(papperDel)) {
            g_object_ref(papperDel);
            gtk_box_remove(GTK_BOX(f), papperDel);
            gtk_box_append(GTK_BOX(ska), papperDel);
            g_object_unref(papperDel);
        } else gtk_box_append(GTK_BOX(ska), papperDel);
    }
    gtk_widget_set_visible(papperKolumn, tva && papperVisas);
    gtk_widget_set_visible(ovaRakna, !tva && papperVisas);
    // i en egen kolumn fyller pappret höjden; i uppgiften är det lagom högt och växer när man skriver
    gtk_widget_set_vexpand(papperRull, tva);
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(papperRull), !tva);
    gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(papperRull), tva ? -1 : (int)(460 * elev.zoom));
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(papperRull), tva ? -1 : (int)(8 * 34 * elev.zoom * autoZoom));
    double az = layout >= 3 ? 1.3 : layout >= 2 ? 1.15 : 1.0;   // större skärm: större text och figurer
    if (nyLayout) {
        if (az != autoZoom) { autoZoom = az; tillampaZoom(true); }
        else visaRegel();
        // kolumnernas bredd: regeln ~33 %, pappret ~42 % av resten (går att dra)
        g_idle_add_once(+[](gpointer) {
            int b = gtk_widget_get_width(M->yttre);
            if (b <= 0) return;
            if (gtk_widget_get_visible(M->regelRull)) gtk_paned_set_position(GTK_PANED(M->yttre), (int)(b * 0.33));
            int rest = gtk_widget_get_visible(M->regelRull) ? b - gtk_paned_get_position(GTK_PANED(M->yttre)) : b;
            gtk_paned_set_position(GTK_PANED(M->inrePaned), (int)(rest * 0.58));
        }, nullptr);
    }
    synkaVerktyg();
}

void Matte::tillampaZoom(bool omrita) {
    double z = elev.zoom * autoZoom;
    gtk_css_provider_load_from_string(stil, stilCss(z).c_str());
    FIGURSKALA = (elev.leklage ? 1.5 : elev.lattlast ? 1.25 : 1.0) * z;
    raknepapper_zoom(papper, z);
    synkaVerktyg();
    const char* sida = gtk_stack_get_visible_child_name(GTK_STACK(stack));
    if (omrita && sida && std::string(sida) == "kort") {
        visaRegel();
        visaUppgift();
    }
}

// Helskärm: bara uppgiften och räknepappret – menyn, regeln och titelraden undan
void Matte::satHelskarm(bool pa) {
    if (pa == helskarm) { synkaVerktyg(); return; }
    helskarm = pa;
    GtkWidget* split = adw_application_window_get_content(ADW_APPLICATION_WINDOW(win));
    if (pa) {
        gtk_window_fullscreen(GTK_WINDOW(win));
        if (ADW_IS_NAVIGATION_SPLIT_VIEW(split)) {
            adw_navigation_split_view_set_collapsed(ADW_NAVIGATION_SPLIT_VIEW(split), TRUE);
            adw_navigation_split_view_set_show_content(ADW_NAVIGATION_SPLIT_VIEW(split), TRUE);
        }
        if (uppgift.v && !papperVisas && papperVal != 0) {   // pappret hör till helskärmen när man räknar
            const std::string& typ = uppgift.mall->typ;
            if (typ == "text" || typ == "uttryck" || typ == "tabell") { papperVisas = true; ordnaLayout(false); }
        }
    } else {
        gtk_window_unfullscreen(GTK_WINDOW(win));
        if (ADW_IS_NAVIGATION_SPLIT_VIEW(split)) adw_navigation_split_view_set_collapsed(ADW_NAVIGATION_SPLIT_VIEW(split), FALSE);
    }
    visaRegel();
    synkaVerktyg();
}

void Matte::satFokus(bool pa) {
    fokusLage = pa;
    synkaVerktyg();
    const char* sida = gtk_stack_get_visible_child_name(GTK_STACK(stack));
    sidopanel(pa && sida && std::string(sida) == "kort");
    if (!pa) fyllLista();
}

// ---- övningen -------------------------------------------------------------------------------------

// Luckorna i stegen: ett "?" som står för sig självt (inte efter en bokstav, som i "Vad blir det?")
static std::vector<std::string> delaLuckor(const std::string& s) {
    std::vector<std::string> delar(1);
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '?') {
            size_t j = i;
            while (j > 0 && s[j - 1] == ' ') --j;
            unsigned char fore = j > 0 ? (unsigned char)s[j - 1] : ' ';
            bool bokstav = std::isalpha(fore) || fore >= 0x80;
            if (!bokstav) { delar.emplace_back(); continue; }
        }
        delar.back() += s[i];
    }
    return delar;
}

void Matte::kopplaRord(GtkWidget* w) {
    std::vector<GtkWidget*> f;
    allaFalt(w, f);
    for (GtkWidget* e : f) g_signal_connect(e, "changed", CB(+[](GtkEditable*, gpointer) { M->rord = true; }), nullptr);
}

void Matte::byggOvavy() {
    GtkWidget* klamp = kortyta();
    GtkWidget* box = inre(klamp);
    uppgKort = box;
    // Rubriken: vilken uppgift och hur långt man kommit (knapparna finns i verktygsraden)
    GtkWidget* delning = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_add_css_class(delning, "matte-delning");
    g_object_set_data(G_OBJECT(box), "delning", delning);
    GtkWidget* huvud = hbox(14);
    gtk_widget_add_css_class(huvud, "matte-uppg-huvud");
    ovaNr = lbl("", "matte-uppg-nr", false);
    gtk_box_append(GTK_BOX(huvud), ovaNr);
    ovaPrickar = lbl("", "matte-prickar", false);
    gtk_widget_set_hexpand(ovaPrickar, TRUE);
    gtk_widget_set_halign(ovaPrickar, GTK_ALIGN_START);
    gtk_widget_set_valign(ovaPrickar, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(huvud), ovaPrickar);
    ovaPassbar = gtk_progress_bar_new();
    gtk_widget_add_css_class(ovaPassbar, "matte13-progress");
    gtk_accessible_update_property(GTK_ACCESSIBLE(ovaPassbar), GTK_ACCESSIBLE_PROPERTY_LABEL,
                                   tr("Exercises completed", "Klara uppgifter"), -1);
    ovaTitel = huvud;
    ovaFira = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(ovaFira), GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
    GtkWidget* fb = hbox(12);
    gtk_widget_add_css_class(fb, "matte-fira");
    gtk_box_append(GTK_BOX(fb), ikonbild("pokal", 48));
    GtkWidget* ft = vbox(2);
    gtk_box_append(GTK_BOX(ft), lbl(tr("Mastered!", "Behärskar!"), "matte-fira-rubrik"));
    ovaFiraText = lbl("");
    gtk_box_append(GTK_BOX(ft), ovaFiraText);
    gtk_box_append(GTK_BOX(fb), ft);
    gtk_revealer_set_child(GTK_REVEALER(ovaFira), fb);
    gtk_widget_set_margin_top(ovaFira, 12);
    ovaFraga = lbl("", "matte-fraga");
    gtk_widget_set_hexpand(ovaFraga, TRUE);
    GtkWidget* fragaRad = hbox(10);
    gtk_widget_add_css_class(fragaRad, "matte-fragarad");
    gtk_box_append(GTK_BOX(fragaRad), ovaFraga);
    lankad(ovaFraga);
    ovaFokus = lbl("", "matte-galler");
    lankad(ovaFokus);
    // Stegen: rader att följa – ett "?" blir en ruta att fylla i
    ovaSteg = vbox(6);
    gtk_widget_add_css_class(ovaSteg, "matte-steg");
    ovaFigur = marg(hbox(0));
    gtk_widget_add_css_class(ovaFigur, "matte-figur");
    ovaBrakModell = matte13::skapaBrakpanel();
    gtk_widget_set_visible(ovaBrakModell, FALSE);
    // Två frivilliga, fristående visualiseringar. Skapar ingen databas och ändrar inte svaret.
    ovaTallinje = matte13::skapaTallinjepanel();
    gtk_widget_set_visible(ovaTallinje, FALSE);
    ovaGeometri = matte13::skapaGeometripanel();
    gtk_widget_set_visible(ovaGeometri, FALSE);
    ovaSteghjalp = matte13::skapaSteghjalp([](const std::string& s) { M->lasUpp(s); });
    gtk_widget_set_visible(ovaSteghjalp, FALSE);
    ovaHundraruta = matte13::skapaHundrarutapanel();
    gtk_widget_set_visible(ovaHundraruta, FALSE);
    // De fem ursprungliga verktygen får en gemensam förälder och väljare.
    ovaArbetsyta = matte13::skapaLektionsyta(
        ovaBrakModell, ovaHundraruta, ovaTallinje, ovaGeometri, ovaSteghjalp);
    if (ovaArbetsyta) gtk_widget_set_visible(ovaArbetsyta, FALSE);
    // Räknepappret står här i en smal layout (annars i en egen kolumn till höger)
    ovaRakna = vbox(0);
    gtk_widget_add_css_class(ovaRakna, "matte-papper-inne");
    ovaSvarRubrik = lbl(tr("Your answer", "Ditt svar"), "matte-sektion");
    ovaSvar = marg(vbox(8));
    gtk_widget_set_margin_top(ovaSvar, 4);
    GtkWidget* svarRad = marg(hbox(10));
    ovaSvarIkon = hbox(0);
    ovaSvarText = lbl("");
    gtk_widget_set_hexpand(ovaSvarText, TRUE);
    gtk_box_append(GTK_BOX(svarRad), ovaSvarIkon);
    gtk_box_append(GTK_BOX(svarRad), ovaSvarText);
    ovaLosning = lbl("", "matte-losning");
    lankad(ovaLosning);
    lankad(ovaSvarText);
    ovaTillbaka = marg(hbox(8));
    GtkWidget* knappar = marg(hbox(8));
    gtk_widget_set_margin_bottom(knappar, 4);
    ovaNasta = knapp(tr("Next task", "Nästa uppgift"), true, [] { M->nyUppgift(); M->tillUppgiften(); }, "go-next-symbolic");
    gtk_widget_set_sensitive(ovaNasta, FALSE);
    gtk_box_append(GTK_BOX(knappar), ovaNasta);
    ovaNiva = lbl("", "matte-svag", false);
    gtk_widget_set_hexpand(ovaNiva, TRUE);
    gtk_widget_set_halign(ovaNiva, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(knappar), ovaNiva);
    // Huvuduppgiften först: särskilt i geometri får en stor frivillig hjälppanel
    // aldrig trycka ned grafen under skärmkanten. Extra modeller/hjälp ligger
    // efter den verkliga svarsytan, men är kvar och kan användas när som helst.
    for (GtkWidget* w : {delning, huvud, ovaPassbar, ovaFira, fragaRad, ovaFokus, ovaSteg, ovaFigur, ovaRakna, ovaSvarRubrik, ovaSvar, ovaArbetsyta, svarRad, ovaLosning, ovaTillbaka, knappar})
        gtk_box_append(GTK_BOX(box), w);
    gtk_widget_set_margin_bottom(box, 8);
    gtk_widget_add_css_class(box, "matte13-exercise");
    ovaKlamp = klamp;
}

// Dagens tre: tre uppgifter från tre kort eleven redan mött – blandad repetition, en liten stund varje dag
void Matte::borjaDagens() {
    auto kort = elev.dagensKort();
    if (kort.empty()) return;
    aktuellt = data.kort(kort[0]);
    if (!aktuellt) return;
    tom(passIkon);
    gtk_box_append(GTK_BOX(passIkon), ikonbild("kalender", 48));
    gtk_label_set_text(GTK_LABEL(passTitel), tr("Today's three", "Dagens tre"));
    gtk_widget_set_visible(passRegel, TRUE);
    gtk_button_set_label(GTK_BUTTON(passSluta), tr("Finish", "Avsluta"));
    loggaAvbrott();
    passGjorda = passRatt = 0;
    passKort = kort;
    passMal = (int)kort.size();
    passAktivt = true;
    uppgiftKlar = true;
    regelHel = false;
    markeraNav("");
    gtk_widget_set_visible(ovaKlamp, TRUE);
    visa("kort");
    nyUppgift();
}

void Matte::borjaOva() {
    if (!aktuellt) return;
    tom(passIkon);
    if (!aktuellt->ikon.empty()) gtk_box_append(GTK_BOX(passIkon), ikonbild(aktuellt->ikon, 48));
    gtk_label_set_text(GTK_LABEL(passTitel), aktuellt->titel.c_str());
    gtk_widget_set_visible(passRegel, TRUE);
    gtk_button_set_label(GTK_BUTTON(passSluta), tr("Finish", "Avsluta"));
    passGjorda = passRatt = 0;
    passMal = elev.leklage ? 6 : 10;
    passKort.clear();
    franPass = false;
    passAktivt = true;
    uppgiftKlar = true;   // ett nytt pass – en gammal uppgift är redan räknad
    titel(std::string(tr("Practise", "Öva")) + " · " + aktuellt->titel);
    gtk_widget_set_visible(ovaKlamp, TRUE);
    visa("kort");
    fyllLista();
    nyUppgift();
    visaRegel();
    if (gtk_adjustment_get_value(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(uppgRull))) > 0)
        gtk_adjustment_set_value(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(uppgRull)), 0);
    if (smal && !regelHel) tillUppgiften();
}

void Matte::nyUppgift() {
    if (!aktuellt) return;
    if (!sparaPapper()) return; // byt aldrig uppgift när diskfullt riskerar arbetet
    tyst();
    if (passGjorda >= passMal) { visaPaus(); return; }
    if (gtk_stack_get_visible_child_name(GTK_STACK(stack)) != std::string("kort")) visa("kort");
    if (!passKort.empty())   // dagens tre: nästa kort i blandningen – och dess regel
        if (const Kort* k = data.kort(passKort[std::min<size_t>(passGjorda, passKort.size() - 1)])) {
            bool nytt = aktuellt != k;
            aktuellt = k;
            titel(std::string(tr("Today's three", "Dagens tre")) + " · " + k->titel);
            if (nytt) { regelHel = false; g_idle_add_once(+[](gpointer) { M->visaRegel(); M->fyllLista(); }, nullptr); }
        }
    Kortstatus& s = elev.status(aktuellt->id);
    tranar = false;
    uppgift = valjUppgift(data, *aktuellt, s, elev.leklage, rnd, &tranar);
    if (tvingaMall >= 0 && tvingaMall < (int)data.mallar(aktuellt->id).size()) {
        uppgift.mallIndex = tvingaMall;
        uppgift.mall = &data.mallar(aktuellt->id)[tvingaMall];
        uppgift.v = &uppgift.mall->varianter[0];
        tranar = false;
    }
    loggaAvbrott();   // Nästa utan rätt svar (efter lösningen) räknas inte – uppgiftKlar sätts då i kolla
    forsok = 0;
    rord = false;
    ritfil.clear();
    uppgiftKlar = false;
    uppgiftBorjade = g_get_monotonic_time();
    if (!uppgift.v) { toast(tr("This card has no tasks yet.", "Det här kortet har inga uppgifter än.")); return; }

    // Första uppgiften i ett återupptaget pass: återställ samma variant, inte
    // en ny slumpfråga. Varje nytt försök får sedan sitt eget unika pappers-id.
    bool aterstall = false;
    PapperData gammalt;
    if (papperAterupptaVidStart && passKort.empty() &&
        elev.papperKort == aktuellt->id && giltigtPapperId(elev.papperId)) {
        const auto& ms = data.mallar(aktuellt->id);
        if (elev.papperMall >= 0 && (size_t)elev.papperMall < ms.size()) {
            const auto& mall = ms[elev.papperMall];
            if (elev.papperVariant >= 0 && (size_t)elev.papperVariant < mall.varianter.size() &&
                mall.varianter[elev.papperVariant].fraga == elev.papperFraga) {
                const std::string path = papperFil(elev.namn, elev.papperId);
                bool finns = g_file_test(path.c_str(), G_FILE_TEST_EXISTS);
                if (finns && papperLadda(elev.namn, elev.papperId, gammalt) &&
                    gammalt.kort == aktuellt->id && gammalt.fraga == elev.papperFraga &&
                    gammalt.mallIndex == elev.papperMall && gammalt.variantIndex == elev.papperVariant) {
                    uppgift.kort = aktuellt;
                    uppgift.mall = &mall;
                    uppgift.mallIndex = elev.papperMall;
                    uppgift.v = &mall.varianter[elev.papperVariant];
                    aterstall = true;
                } else if (finns) {
                    // Behåll den skadade filen oförändrad för eventuell räddning.
                    toast(tr("Could not restore an earlier paper. The old file was kept.",
                             "Kunde inte återställa ett tidigare papper. Den gamla filen behölls."));
                }
            }
        }
    }
    papperAterupptaVidStart = false;
    papperByte = true;
    if (aterstall) {
        raknepapper_ladda_session(papper, gammalt);
        if (papperStatus) gtk_label_set_text(GTK_LABEL(papperStatus), tr("Restored", "Återställd"));
    } else {
        // Ett nytt försök kan aldrig skriva över det förra, även om samma variant
        // slumpas fram en gång till.
        const auto& variants = uppgift.mall->varianter;
        const int variant = (int)(uppgift.v - variants.data());
        elev.papperId = nyttPapperId();
        elev.papperKort = aktuellt->id;
        elev.papperFraga = uppgift.v->fraga;
        elev.papperMall = uppgift.mallIndex;
        elev.papperVariant = variant;
        raknepapper_ny_session(papper, elev.papperId, elev.papperKort,
                               elev.papperFraga, elev.papperMall, variant);
        if (!elev.spara())
            toast(tr("Could not save the current task. Check available disk space.",
                     "Kunde inte spara aktuell uppgift. Kontrollera ledigt diskutrymme."));
        if (papperStatus) gtk_label_set_text(GTK_LABEL(papperStatus), tr("Not saved yet", "Inte sparat än"));
    }
    papperByte = false;
    visaUppgift();
    const Variant& v = *uppgift.v;
    if (lasesUpp()) {
        // lekläget, ett nytt kort: regelns kärna först, sedan frågan
        if (elev.leklage && passGjorda == 0 && forsok == 0 && regelLasDelar && elev.lage(aktuellt->id) == LAR && passKort.empty()) {
            std::vector<Lasdel> d(regelLasDelar->begin(), regelLasDelar->begin() + std::min<size_t>(regelLasDelar->size(), aktuellt->sager.empty() ? 1 : 2));
            d.push_back({ovaFraga, v.fraga, true});
            lasMarkerat(d);
        } else lasUppUppgift();
    }
}

// Ritar uppgiften som står i `uppgift` (en ny, eller samma igen efter zoom)
void Matte::visaUppgift() {
    if (!uppgift.v) return;
    const Variant& v = *uppgift.v;
    gtk_widget_set_visible(ovaFokus, tranar && !v.ledtrad.empty());   // en feltanke tränas: ledtråden står framme
    if (tranar) sattText(ovaFokus, std::string(tr("Think about: ", "Tänk på: ")) + v.ledtrad);
    tom(ovaTillbaka);
    gtk_widget_set_visible(ovaTillbaka, FALSE);
    gtk_revealer_set_reveal_child(GTK_REVEALER(ovaFira), FALSE);
    gtk_widget_set_visible(ovaFira, FALSE);
    sattText(ovaFraga, v.fraga, true);
    // Stegen: varje rad för sig, ett "?" blir en ruta. Rutorna rättas inte – utom den sista när den är
    // själva svaret ("= ?" sist): då fyller den svarsrutan, så att man bara skriver en gång.
    tom(ovaSteg);
    luckor.clear();
    GtkWidget* sistaLucka = nullptr;
    bool sistaArSvar = false;
    for (size_t i = 0; i < v.steg.size(); ++i) {
        auto delar = delaLuckor(v.steg[i]);
        if (delar.size() == 1) {
            GtkWidget* l = lbl("");
            sattText(l, v.steg[i]);
            lankad(l);
            gtk_box_append(GTK_BOX(ovaSteg), l);
            continue;
        }
        GtkWidget* rad = hbox(6);
        gtk_widget_add_css_class(rad, "matte-steg-rad");
        for (size_t j = 0; j < delar.size(); ++j) {
            if (!delar[j].empty()) {
                GtkWidget* l = lbl(delar[j], nullptr, false);
                gtk_widget_set_valign(l, GTK_ALIGN_CENTER);
                gtk_box_append(GTK_BOX(rad), l);
            }
            if (j + 1 < delar.size()) {
                GtkWidget* e = gtk_entry_new();
                gtk_editable_set_width_chars(GTK_EDITABLE(e), 5);
                gtk_editable_set_max_width_chars(GTK_EDITABLE(e), 8);
                gtk_widget_add_css_class(e, "matte-lucka");
                gtk_widget_set_valign(e, GTK_ALIGN_CENTER);
                gtk_accessible_update_property(GTK_ACCESSIBLE(e), GTK_ACCESSIBLE_PROPERTY_LABEL, tr("Fill in", "Fyll i"), -1);
                luckor.push_back(e);
                gtk_box_append(GTK_BOX(rad), e);
                sistaLucka = e;
                std::string efter = j + 1 < delar.size() ? delar[j + 1] : "";
                sistaArSvar = i + 1 == v.steg.size() && j + 2 == delar.size() && efter.find_first_not_of(" .") == std::string::npos;
            }
        }
        gtk_box_append(GTK_BOX(ovaSteg), rad);
    }
    gtk_widget_set_visible(ovaSteg, !v.steg.empty());
    gtk_widget_set_visible(ovaLosning, FALSE);
    tom(ovaFigur);
    bool fig = v.figur.arObjekt() && figurVisasSeparat(uppgift.mall->typ);
    if (fig) gtk_box_append(GTK_BOX(ovaFigur), figurvy(data, v.figur));
    gtk_widget_set_visible(ovaFigur, fig);
    // Frivillig laboration, skild från den riktiga uppgiften och svarsrättningen.
    matte13::Brak matte13B;
    const bool harBrak = aktuellt && matte13::hittaBrak(aktuellt->titel, v.fraga, matte13B);
    gtk_widget_set_visible(ovaBrakModell, harBrak);
    if (harBrak) matte13::stallInBrakpanel(ovaBrakModell, matte13B);
    // Ett separat, frivilligt sätt att förstå sambandet mellan bråk och procent.
    gtk_widget_set_visible(ovaHundraruta, harBrak);
    if (harBrak) matte13::stallInHundrarutapanel(ovaHundraruta, matte13B);
    // Tillgängliga experiment som inte är kopplade till elevens rättning.
    matte13::Tallinje matte13L;
    const bool harTallinje = aktuellt && matte13::hittaTallinje(aktuellt->titel, v.fraga, matte13L);
    gtk_widget_set_visible(ovaTallinje, harTallinje);
    if (harTallinje) matte13::stallInTallinjepanel(ovaTallinje, matte13L);
    const bool harGeometri = aktuellt && matte13::arGeometrilektion(aktuellt->titel);
    gtk_widget_set_visible(ovaGeometri, harGeometri);
    if (harGeometri) matte13::stallInGeometripanel(ovaGeometri);
    std::vector<std::string> matte13Hjalp;
    if (!v.ledtrad.empty()) matte13Hjalp.push_back(v.ledtrad);
    for (const auto& steg : v.losning) if (!steg.empty()) matte13Hjalp.push_back(steg);
    matte13::stallInSteghjalp(ovaSteghjalp, matte13Hjalp);
    // Endast relevanta verktyg. Allt stöd är redo innan väljaren uppdateras.
    matte13::uppdateraLektionsyta(
        ovaArbetsyta, harBrak, harTallinje, harGeometri, !matte13Hjalp.empty());
    tom(ovaSvar);
    svarsyta = matte::svarsyta(data, *uppgift.mall, v, [](std::vector<std::string> sv) { M->kolla(sv); });
    gtk_box_append(GTK_BOX(ovaSvar), svarsyta.widget);
    const std::string& typ = uppgift.mall->typ;
    bool skrivs = typ == "text" || typ == "uttryck" || typ == "tabell";
    // Räknepappret: för uppgifter man räknar ut (inte välja, klicka, måla) – inte för de minsta
    {
        std::string text = v.fraga;
        for (auto& x : v.steg) text += "\n" + x;
        raknepapper_satt_uppgift(papper, text);
    }
    papperVisas = papperVal == 1 || (papperVal == -1 && skrivs && !elev.leklage);
    ordnaLayout(false);
    gtk_widget_set_visible(ovaSvarRubrik, skrivs);
    // Luckorna: Enter går till nästa ruta och till sist till svaret
    GtkWidget* svarsfalt = svarsyta.fokus && GTK_IS_ENTRY(svarsyta.fokus) ? svarsyta.fokus : nullptr;
    for (size_t i = 0; i < luckor.size(); ++i) {
        GtkWidget* nasta = i + 1 < luckor.size() ? luckor[i + 1] : (svarsfalt ? svarsfalt : nullptr);
        if (luckor[i] == sistaLucka && sistaArSvar && svarsfalt && (typ == "text" || typ == "uttryck")) {
            // den sista rutan ÄR svaret: det man skriver där står i svarsrutan, och Enter kollar
            g_object_set_data(G_OBJECT(luckor[i]), "svar", svarsfalt);
            g_signal_connect(luckor[i], "changed", CB(+[](GtkEditable* e, gpointer) {
                GtkWidget* sv = static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(e), "svar"));
                const char* forra = static_cast<const char*>(g_object_get_data(G_OBJECT(e), "spegel"));
                std::string nu = gtk_editable_get_text(GTK_EDITABLE(sv));
                if (nu.empty() || (forra && nu == forra)) gtk_editable_set_text(GTK_EDITABLE(sv), gtk_editable_get_text(e));
                g_object_set_data_full(G_OBJECT(e), "spegel", g_strdup(gtk_editable_get_text(e)), g_free);
            }), nullptr);
            g_signal_connect(luckor[i], "activate", CB(+[](GtkEntry* e, gpointer) {
                gtk_widget_activate(static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(e), "svar")));
            }), nullptr);
            gtk_widget_set_tooltip_text(luckor[i], tr("This is the answer – it is also written in the answer box",
                                                      "Det här är svaret – det skrivs också i svarsrutan"));
        } else if (nasta) {
            g_object_set_data(G_OBJECT(luckor[i]), "nasta", nasta);
            g_signal_connect(luckor[i], "activate", CB(+[](GtkEntry* e, gpointer) {
                gtk_widget_grab_focus(static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(e), "nasta")));
            }), nullptr);
        }
    }
    kopplaRord(ovaSteg);
    kopplaRord(ovaSvar);
    if (elev.sifferknappar) sifferknappar(ovaSvar);
    tom(ovaSvarIkon);
    gtk_label_set_text(GTK_LABEL(ovaSvarText), "");
    gtk_widget_set_css_classes(ovaSvarText, nullptr);
    gtk_widget_set_sensitive(ovaNasta, FALSE);
    std::string niva = std::string(tr("Level ", "Nivå ")) + std::to_string(uppgift.mall->niva) + tr(" of 3", " av 3");
    gtk_widget_set_tooltip_text(ovaNiva, tr("How hard the task is", "Hur svår uppgiften är"));
    gtk_label_set_text(GTK_LABEL(ovaNiva), niva.c_str());
    uppdateraPass();
    // Fokus: första rutan i stegen om det finns några (man räknar uppifrån), annars svaret
    // (inte när hela regeln står ovanför i ett smalt fönster – fokus skulle rulla förbi den innan eleven läst)
    if (!(smal && regelHel && passGjorda == 0 && forsok == 0)) {
        if (!luckor.empty()) gtk_widget_grab_focus(luckor.front());
        else if (svarsyta.fokus) gtk_widget_grab_focus(svarsyta.fokus);
    } else gtk_window_set_focus(GTK_WINDOW(win), nullptr);
}

// Fri rityta: rättas inte – sparas som PNG hos eleven och syns i Översikt
void Matte::sparaTeckning() {
    tom(ovaSvarIkon);
    gtk_widget_set_css_classes(ovaSvarText, nullptr);
    if (svarsyta.tom && svarsyta.tom()) {
        gtk_label_set_text(GTK_LABEL(ovaSvarText), tr("Draw something first.", "Rita något först."));
        gtk_widget_add_css_class(ovaSvarText, "fel");
        return;
    }
    std::string mapp = ritmapp(elev.namn);
    g_mkdir_with_parents(mapp.c_str(), 0700);
    bool ny = ritfil.empty();
    std::string tid;
    {
        GDateTime* nu = g_date_time_new_now_local();
        gchar* t = g_date_time_format(nu, "%Y-%m-%d %H:%M");
        tid = t; g_free(t);
        if (ny) { gchar* f = g_date_time_format(nu, "%Y%m%d-%H%M%S"); ritfil = std::string(f) + "-" + aktuellt->id + ".png"; g_free(f); }
        g_date_time_unref(nu);
    }
    if (!svarsyta.spara || !svarsyta.spara(mapp + "/" + ritfil)) {
        g_warning("Matte: kunde inte spara teckningen i %s", mapp.c_str());
        toast(tr("Could not save the drawing.", "Kunde inte spara teckningen."));
        ritfil.clear();
        return;
    }
    if (ny) {
        elev.ritningar.push_back({aktuellt->id, ritfil, uppgift.v->fraga, tid}); ++passGjorda; ++passRatt; uppdateraPass();
        Json j = uppgiftsinfo();
        j.satt("ratt", true);
        logga(elev.namn, "svar", j);
    } else {
        for (auto& r : elev.ritningar) if (r.fil == ritfil) r.tid = tid;
    }
    uppgiftKlar = true;
    elev.spara();
    gtk_box_append(GTK_BOX(ovaSvarIkon), ikonbild("ratt", 28));
    gtk_label_set_text(GTK_LABEL(ovaSvarText), tr("Saved! Your teacher can look at it in the overview.",
                                                  "Sparad! Din lärare kan titta på den i Översikt."));
    gtk_widget_add_css_class(ovaSvarText, "ratt");
    if (!uppgift.v->losning.empty()) {
        std::string t = tr("Compare with your drawing:", "Jämför med din teckning:");
        for (size_t i = 0; i < uppgift.v->losning.size(); ++i) t += "\n" + std::to_string(i + 1) + ". " + uppgift.v->losning[i];
        sattText(ovaLosning, t);
        gtk_widget_set_visible(ovaLosning, TRUE);
    }
    if (lasesUpp()) lasUpp(tr("Lovely! The drawing is saved.", "Fint! Teckningen är sparad."));
    gtk_widget_set_sensitive(ovaNasta, TRUE);
    gtk_widget_grab_focus(ovaNasta);
}

void Matte::kolla(const std::vector<std::string>& svar) {
    if (!uppgift.v) return;
    if (uppgift.mall->typ == "rita") { sparaTeckning(); return; }
    Bedomning b;
    {
        wenuy::Measure mat("ova.kolla");
        b = bedom(*uppgift.mall, *uppgift.v, svar);
    }
    if (svarsyta.visa) svarsyta.visa(b, svar);
    tom(ovaSvarIkon);
    gtk_widget_set_css_classes(ovaSvarText, nullptr);
    if (b.omkastad && elev.lattlast && forsok == 0) {   // lättläst: omkastade siffror räknas inte – försök igen
        b.otolkbart = true;
        b.text = tr("Almost! The digits have swapped places. Try again – which digit comes first?",
                    "Nästan! Siffrorna har bytt plats. Försök igen – vilken siffra kommer först?");
    }
    if (b.otolkbart) {
        if (lasesUpp()) lasUpp(b.text);
        gtk_label_set_text(GTK_LABEL(ovaSvarText), b.text.c_str());
        gtk_widget_add_css_class(ovaSvarText, "fel");
        return;
    }
    ++forsok;
    {
        Json j = uppgiftsinfo();
        j.satt("ratt", b.ratt);
        j.satt("forsok", forsok);
        if (!b.feltankeId.empty()) j.satt("feltanke", b.feltankeId);
        if (b.omkastad) j.satt("omkastad", true);
        if (forsok == 1 && svar.size() == 1 && svar[0].size() < 40) j.satt("svar", svar[0]);
        if (forsok == 1) {   // räknade eleven själv? (rader i Räkna här, ifyllda rutor i stegen)
            int rader = raknepapper_rader(papper), ifyllda = 0;
            for (GtkWidget* e : luckor) ifyllda += *gtk_editable_get_text(GTK_EDITABLE(e)) != 0;
            if (rader) j.satt("rader", rader);
            if (ifyllda) j.satt("luckor", ifyllda);
        }
        logga(elev.namn, "svar", j);
    }
    if (b.ratt || forsok >= 2) uppgiftKlar = true;   // klarat, eller fått se lösningen
    if (forsok == 1) {   // bara första svaret räknas mot Behärskar – nya försök är för att lära sig
        int fore = elev.status(aktuellt->id).lage;
        elev.svarade(aktuellt->id, b.ratt, b.feltankeId, uppgift.mallIndex);
        elev.spara();
        ++passGjorda;
        if (b.ratt) ++passRatt;
        if (fore != BEHARSKAR && elev.status(aktuellt->id).lage == BEHARSKAR) fira(*aktuellt);
    }
    if (!b.ratt) {   // fastnat? föreslå ett förkunskapskort
        tom(ovaTillbaka);
        if (const Kort* f = gaTillbaka(data, *aktuellt, elev)) {
            gtk_widget_set_visible(ovaTillbaka, TRUE);
            gtk_box_append(GTK_BOX(ovaTillbaka), lbl(tr("This builds on something earlier.", "Det här bygger på något tidigare."), "matte-svag", false));
            std::string fid = f->id;
            gtk_box_append(GTK_BOX(ovaTillbaka), knapp(std::string(tr("Practise first: ", "Öva först: ")) + f->titel, false, [fid] {
                if (auto* k = M->data.kort(fid)) { M->visaKort(*k, true); }
            }));
        }
    }
    // Första felet: feltankens förklaring eller ledtråden – eleven försöker själv.
    // Andra felet: hela lösningen, så att ingen fastnar.
    if (!b.ratt && forsok >= 2 && svarsyta.visaFacit) svarsyta.visaFacit();   // kryssa, para: de rätta syns
    bool visaLosning = !b.ratt && forsok >= 2 && !uppgift.v->losning.empty();
    if (visaLosning) {
        std::string t = tr("One way to think:", "Så här kan man tänka:");
        for (size_t i = 0; i < uppgift.v->losning.size(); ++i) t += "\n" + std::to_string(i + 1) + ". " + uppgift.v->losning[i];
        sattText(ovaLosning, t);
        gtk_widget_set_visible(ovaLosning, TRUE);
    }
    gtk_box_append(GTK_BOX(ovaSvarIkon), ikonbild(b.ratt ? "ratt" : "fel", 28));
    std::string text = b.text;
    if (b.ratt && forsok > 1) text = tr("Right – now you know how. The next one counts.", "Rätt – nu vet du hur man gör. Nästa uppgift räknas.");
    sattText(ovaSvarText, text);
    gtk_widget_add_css_class(ovaSvarText, b.ratt ? "ratt" : "fel");
    if (lasesUpp()) {
        std::string t = b.ratt ? tr("Right!", "Rätt!") : b.text;
        if (visaLosning) for (auto& x : uppgift.v->losning) t += " " + x;
        lasUpp(t);
    }
    if (b.ratt) { gtk_widget_set_sensitive(ovaNasta, TRUE); gtk_widget_grab_focus(ovaNasta); }
    fyllLista();
    uppdateraPass();
}

void Matte::fira(const Kort& k) {
    nyaBeharskade.insert(k.id);
    gboolean anim = TRUE;
    g_object_get(gtk_settings_get_default(), "gtk-enable-animations", &anim, nullptr);
    gtk_revealer_set_transition_duration(GTK_REVEALER(ovaFira), anim ? 180 : 0);
    std::string t = std::string("“") + k.titel + tr("” is now in your rule book. It will come back for review in a few days.",
                                                     "” sitter nu och finns i din regelbok. Det kommer tillbaka för repetition om några dagar.");
    gtk_label_set_text(GTK_LABEL(ovaFiraText), t.c_str());
    gtk_widget_set_visible(ovaFira, TRUE);
    gtk_revealer_set_reveal_child(GTK_REVEALER(ovaFira), TRUE);
    wenuy::playSound("complete");
    if (lasesUpp()) lasUpp(tr("Well done! You know it now.", "Bra jobbat! Nu kan du det."));
}

// ---- placeringen ----------------------------------------------------------------------------------

void Matte::byggPlaceringsvy() {
    GtkWidget* klamp = kortyta();
    GtkWidget* box = inre(klamp);
    gtk_box_append(GTK_BOX(box), lbl(tr("How much do you already know?", "Hur mycket kan du redan?"), "matte-titel"));
    gtk_box_append(GTK_BOX(box), marg(lbl(tr("Answer as well as you can. Wrong answers are fine – they only show where to start.",
                                             "Svara så gott du kan. Fel svar är inte farliga – de visar bara var vi ska börja."), "matte-svag")));
    placOmrade = marg(lbl("", "matte-svag"));
    placFraga = marg(lbl("", "matte-fraga"));
    placSteg = lbl("", "matte-steg");
    lankad(placFraga);
    lankad(placSteg);
    placFigur = marg(hbox(0));
    placSvar = marg(vbox(8));
    placText = marg(lbl("", "matte-svag"));
    GtkWidget* knappar = marg(hbox(8));
    gtk_box_append(GTK_BOX(knappar), knapp(tr("Don't know", "Vet inte"), false, [] { M->placSvara(nullptr); }));
    gtk_box_append(GTK_BOX(knappar), knapp(tr("Skip the test", "Hoppa över testet"), false, [] { M->placAvsluta(true); }));
    for (GtkWidget* w : {placOmrade, placFraga, placSteg, placFigur, placSvar, placText, knappar}) gtk_box_append(GTK_BOX(box), w);
    placRull = rull(klamp);
}

void Matte::borjaPlacering() {
    markeraNav("");
    placeringar = placeraAlla(data);
    placI = 0;
    titel(tr("How much do you already know?", "Hur mycket kan du redan?"));
    tom(passIkon);
    gtk_box_append(GTK_BOX(passIkon), ikonbild("fragetecken", 48));
    gtk_label_set_text(GTK_LABEL(passTitel), tr("How much do you already know?", "Hur mycket kan du redan?"));
    gtk_label_set_text(GTK_LABEL(passPrickar), "");
    gtk_label_set_text(GTK_LABEL(passText), tr("Wrong answers are fine – they only show where to start.",
                                               "Fel svar är inte farliga – de visar bara var vi ska börja."));
    gtk_widget_set_visible(passRegel, FALSE);
    gtk_button_set_label(GTK_BUTTON(passSluta), tr("Skip the test", "Hoppa över testet"));
    visa("placering");
    placNasta();
}

void Matte::placNasta() {
    while (placI < placeringar.size() && placeringar[placI].klar()) ++placI;
    if (placI >= placeringar.size()) { placAvsluta(false); return; }
    Placering& p = placeringar[placI];
    placUppgift = p.nasta(data, rnd);
    if (!placUppgift.v) { p.svara(false); placNasta(); return; }
    const Variant& v = *placUppgift.v;
    gtk_label_set_text(GTK_LABEL(placOmrade), (data.omradeNamn(p.omrade) + " · " + tr("question ", "fråga ") + std::to_string(p.fragor + 1)).c_str());
    sattText(placFraga, v.fraga, true);
    std::string steg;
    for (auto& x : v.steg) steg += (steg.empty() ? "" : "\n") + x;
    sattText(placSteg, steg);
    gtk_widget_set_visible(placSteg, !v.steg.empty());
    tom(placFigur);
    bool fig = v.figur.arObjekt() && figurVisasSeparat(placUppgift.mall->typ);
    if (fig) gtk_box_append(GTK_BOX(placFigur), figurvy(data, v.figur));
    gtk_widget_set_visible(placFigur, fig);
    tom(placSvar);
    placSvarsyta = matte::svarsyta(data, *placUppgift.mall, v, [](std::vector<std::string> sv) { M->placSvara(&sv); });
    gtk_box_append(GTK_BOX(placSvar), placSvarsyta.widget);
    gtk_label_set_text(GTK_LABEL(placText), "");
    if (placSvarsyta.fokus) gtk_widget_grab_focus(placSvarsyta.fokus);
    if (lasesUpp()) lasUpp(v.fraga);
}

void Matte::placSvara(const std::vector<std::string>* svar) {
    if (placI >= placeringar.size()) return;
    bool ratt = false;
    if (svar) {
        Bedomning b = bedom(*placUppgift.mall, *placUppgift.v, *svar);
        if (b.otolkbart) { gtk_label_set_text(GTK_LABEL(placText), b.text.c_str()); return; }
        ratt = b.ratt;
    }
    placeringar[placI].svara(ratt);
    g_idle_add_once(+[](gpointer) { M->placNasta(); }, nullptr);   // svarsytan som anropade oss byts ut
}

void Matte::placAvsluta(bool hoppade) {
    if (!hoppade) for (auto& p : placeringar) p.tillampa(elev);
    elev.placerad = true;
    elev.spara();
    fyllLista(); uppdateraStatus(); visaHem();
    toast(hoppade ? tr("You start from the beginning.", "Du börjar från början.")
                  : tr("Done! Everything you already know is in your rule book.", "Klart! Det du redan kan finns i din regelbok."));
}

// ---- översikten för läraren eller föräldern -------------------------------------------------------

void Matte::visaOversikt() {
    markeraNav("oversikt");
    GtkWidget* klamp = kortyta();
    GtkWidget* box = inre(klamp);
    gtk_box_append(GTK_BOX(box), lbl(tr("Overview", "Översikt"), "matte-titel"));
    GtkWidget* g = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(g), 18);
    gtk_grid_set_row_spacing(GTK_GRID(g), 6);
    marg(g);
    std::vector<std::string> rub = {tr("Student", "Elev"), std::string("● ") + tr("Mastered", "Behärskar"), std::string("◐ ") + tr("Practising", "Övar"),
                                    tr("Review today", "Repetera i dag"), tr("Answers", "Svar"), tr("Correct", "Rätt")};
    for (size_t c = 0; c < rub.size(); ++c) gtk_grid_attach(GTK_GRID(g), lbl(rub[c], "matte-rubrik", false), (int)c, 0, 1, 1);
    struct Grupp { std::string kort, text; int antal = 0; std::set<std::string> elever; };
    std::map<std::string, Grupp> grupp;
    int r = 1;
    for (auto& namn : elever()) {
        Elev e = Elev::ladda(namn);
        auto n = e.sammanfattning();
        int svar = 0, ratt = 0;
        for (auto& [id, s] : e.kort) { svar += s.antal; ratt += s.antalRatt; }
        std::string visn = e.visningsnamn.empty() ? namn : e.visningsnamn;
        std::vector<std::string> cell = {visn, std::to_string(n[BEHARSKAR]), std::to_string(n[OVAR]), std::to_string(e.attRepetera().size()),
                                         std::to_string(svar), svar ? std::to_string(100 * ratt / svar) + " %" : "–"};
        for (size_t c = 0; c < cell.size(); ++c) {
            if (c == 0) {   // namnet öppnar elevens egen sida
                std::string n = namn;
                GtkWidget* b = knapp(cell[0], false, [n] { M->visaElevvy(n); });
                gtk_widget_add_css_class(b, "flat");
                gtk_widget_set_halign(b, GTK_ALIGN_START);
                gtk_widget_set_tooltip_text(b, tr("Show this student's page", "Visa elevens sida"));
                gtk_grid_attach(GTK_GRID(g), b, 0, r, 1, 1);
            } else {
                gtk_grid_attach(GTK_GRID(g), lbl(cell[c], nullptr, false), (int)c, r, 1, 1);
            }
        }
        ++r;
        for (auto& [kid, s] : e.kort)
            for (auto& [fid, antal] : s.feltankeTotalt) {
                const Kort* k = data.kort(kid);
                Grupp& gr = grupp[kid + "#" + fid];
                if (gr.kort.empty()) {
                    gr.kort = k ? k->titel : kid;
                    auto f = k ? k->feltankar.find(fid) : std::map<std::string, std::string>::const_iterator();
                    gr.text = k && f != k->feltankar.end() ? f->second : fid;
                }
                gr.antal += antal;
                gr.elever.insert(visn);
            }
    }
    gtk_box_append(GTK_BOX(box), g);
    gtk_box_append(GTK_BOX(box), marg(lbl(tr("Click a name to see where that student gets stuck.",
                                             "Klicka på ett namn för att se var just den eleven fastnar."), "matte-svag"), 16, 2));
    std::vector<Grupp> vanligast;
    for (auto& [k, gr] : grupp) vanligast.push_back(gr);
    std::sort(vanligast.begin(), vanligast.end(), [](const Grupp& a, const Grupp& b) {
        return a.elever.size() != b.elever.size() ? a.elever.size() > b.elever.size() : a.antal > b.antal;
    });
    if (vanligast.size() > 10) vanligast.resize(10);
    if (!vanligast.empty()) {
        gtk_box_append(GTK_BOX(box), marg(lbl(tr("Most common misconceptions in the group", "Vanligaste feltankarna i gruppen"), "matte-rubrik"), 16, 10));
        for (auto& f : vanligast)
            gtk_box_append(GTK_BOX(box), lbl(f.kort + " – " + std::to_string(f.elever.size()) + " " + tr("student(s)", "elev(er)") + ", " +
                                             std::to_string(f.antal) + "×\n" + f.text, "matte-galler"));
    } else {
        gtk_box_append(GTK_BOX(box), marg(lbl(tr("No misconceptions recorded yet.", "Inga feltankar registrerade än."), "matte-svag")));
    }
    // Teckningarna (fri rityta) – de nyaste först
    struct Teck { std::string elev, fil, kort, fraga, tid; };
    std::vector<Teck> teck;
    for (auto& namn : elever()) {
        Elev e = Elev::ladda(namn);
        std::string visn = e.visningsnamn.empty() ? namn : e.visningsnamn;
        for (auto& rt : e.ritningar) {
            const Kort* k = data.kort(rt.kort);
            teck.push_back({visn, ritmapp(namn) + "/" + rt.fil, k ? k->titel : rt.kort, rt.fraga, rt.tid});
        }
    }
    std::sort(teck.begin(), teck.end(), [](const Teck& a, const Teck& b) { return a.tid > b.tid; });
    if (teck.size() > 24) teck.resize(24);
    gtk_box_append(GTK_BOX(box), marg(lbl(tr("Drawings", "Teckningar"), "matte-rubrik"), 16, 14));
    if (teck.empty()) {
        gtk_box_append(GTK_BOX(box), marg(lbl(tr("No drawings yet. They are saved when a student presses Done on a drawing task.",
                                                 "Inga teckningar än. De sparas när en elev trycker Klar på en rituppgift."), "matte-svag")));
    } else {
        GtkWidget* galleri = adw_wrap_box_new();
        adw_wrap_box_set_child_spacing(ADW_WRAP_BOX(galleri), 10);
        adw_wrap_box_set_line_spacing(ADW_WRAP_BOX(galleri), 10);
        marg(galleri, 16, 6);
        for (auto& t : teck) {
            GdkPixbuf* pb = gdk_pixbuf_new_from_file_at_scale(t.fil.c_str(), 220, 140, TRUE, nullptr);   // liten kopia i minnet
            if (!pb) continue;
            GdkTexture* tx = gdk_texture_new_for_pixbuf(pb);
            g_object_unref(pb);
            GtkWidget* v = vbox(4);
            GtkWidget* bild = gtk_picture_new_for_paintable(GDK_PAINTABLE(tx));
            g_object_unref(tx);
            gtk_widget_set_size_request(bild, 220, 140);
            gtk_box_append(GTK_BOX(v), bild);
            gtk_box_append(GTK_BOX(v), lbl(t.elev + " · " + t.kort + "\n" + t.tid, "matte-svag", false));
            Teck kopia = t;
            GtkWidget* b = knapp("", false, [kopia] { M->visaTeckning(kopia.fil, kopia.elev + " · " + kopia.kort, kopia.fraga); });
            gtk_button_set_child(GTK_BUTTON(b), v);
            gtk_widget_add_css_class(b, "flat");
            gtk_widget_add_css_class(b, "matte-teckning");
            adw_wrap_box_append(ADW_WRAP_BOX(galleri), b);
        }
        gtk_box_append(GTK_BOX(box), galleri);
    }
    GtkWidget* imp = knapp(tr("Import student…", "Importera elev…"), false, [] { M->importera(); }, "document-open-symbolic");
    gtk_widget_set_tooltip_text(imp, tr("Bring in a student exported from another computer", "Ta in en elev som exporterats från en annan dator"));
    gtk_widget_set_halign(imp, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), marg(imp, 16, 14));
    GtkWidget* hela = knapp(tr("The whole rule book (PDF)", "Hela regelboken (PDF)"), false, [] { M->skrivRegelbok(true); }, "printer-symbolic");
    gtk_widget_set_tooltip_text(hela, tr("All cards from preschool to upper secondary – for the classroom wall or a binder",
                                         "Alla kort från förskola till gymnasium – till klassrummets vägg eller en pärm"));
    gtk_widget_set_halign(hela, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), marg(hela, 16, 8));
    gtk_box_append(GTK_BOX(box), marg(lbl(tr("Everything comes from the files on this computer. Nothing is sent anywhere.",
                                             "Allt kommer från filerna på den här datorn. Inget skickas någonstans."), "matte-svag"), 16, 8));
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(oversiktRull), klamp);
    titel(tr("Overview", "Översikt"));
    visa("oversikt");
}

// ---- utskrift ---------------------------------------------------------------------------------------

void Matte::pdfKlar(const std::string& fil, const std::string& text) {
    senastePdf = fil;
    wenuy_app_window_toast_action(win, text.c_str(), tr("Open", "Öppna"), CB(+[](AdwToast*, gpointer) {
        GFile* f = g_file_new_for_path(M->senastePdf.c_str());
        GtkFileLauncher* l = gtk_file_launcher_new(f);
        gtk_file_launcher_launch(l, GTK_WINDOW(M->win), nullptr, nullptr, nullptr);   // PDF-läsaren på datorn
        g_object_unref(l); g_object_unref(f);
    }), nullptr);
}

void Matte::skrivArbetsblad(const Kort& k) {
    std::string fel, mapp = utskriftsmapp();
    int nr = g_random_int_range(1000, 10000);
    std::string fil = mapp + "/" + sakertFilnamn(std::string(tr("Worksheet ", "Arbetsblad ")) + std::to_string(nr) + " – " + k.titel) + ".pdf";
    {
        wenuy::Measure m("utskrift.arbetsblad");
        nr = arbetsbladPdf(data, {&k}, fil, nr, elev.lattlast ? 6 : 8, &fel, elev.lattlast);
    }
    if (!nr) {
        g_warning("Matte: arbetsbladet gick inte att skriva: %s", fel.c_str());
        toast(std::string(tr("Could not save the worksheet: ", "Kunde inte spara arbetsbladet: ")) + fel);
        return;
    }
    pdfKlar(fil, std::string(tr("Worksheet ", "Arbetsblad ")) + std::to_string(nr) + tr(" saved in Documents/Matte", " sparat i Dokument/Matte"));
}

void Matte::skrivRegelbok(bool hela) {
    std::vector<const Kort*> kort;
    std::vector<std::string> grupper;
    std::string titel, under;
    if (hela) {
        kort = kortILaroplansordning(data, &grupper);
        titel = tr("The rule book", "Regelboken");
        under = tr("All cards – from preschool to upper secondary", "Alla kort – från förskola till gymnasium");
    } else {
        for (auto& id : elev.regelbok) if (const Kort* k = data.kort(id)) kort.push_back(k);
        titel = tr("My rule book", "Min regelbok");
        under = elev.visningsnamn.empty() ? elev.namn : elev.visningsnamn;
    }
    if (kort.empty()) { toast(tr("The rule book is empty so far.", "Regelboken är tom än så länge.")); return; }
    std::string fil = utskriftsmapp() + "/" + sakertFilnamn(hela ? titel : titel + " – " + under) + ".pdf", fel;
    bool ok;
    {
        wenuy::Measure m("utskrift.regelbok");
        ok = regelbokPdf(data, kort, titel, under, fil, &fel, hela ? &grupper : nullptr);
    }
    if (!ok) {
        g_warning("Matte: regelboken gick inte att skriva: %s", fel.c_str());
        toast(std::string(tr("Could not save the rule book: ", "Kunde inte spara regelboken: ")) + fel);
        return;
    }
    pdfKlar(fil, std::string(titel) + tr(" saved in Documents/Matte", " sparad i Dokument/Matte"));
}

void Matte::visaTeckning(const std::string& fil, const std::string& rubrik, const std::string& fraga) {
    AdwDialog* dlg = adw_dialog_new();
    adw_dialog_set_title(dlg, rubrik.c_str());
    adw_dialog_set_content_width(dlg, 720);
    GtkWidget* tv = adw_toolbar_view_new();
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tv), adw_header_bar_new());
    GtkWidget* v = vbox(8);
    marg(v, 16, 8);
    gtk_widget_set_margin_bottom(v, 16);
    gtk_box_append(GTK_BOX(v), lbl(fraga, "matte-fraga"));
    GtkWidget* bild = gtk_picture_new_for_filename(fil.c_str());
    gtk_picture_set_content_fit(GTK_PICTURE(bild), GTK_CONTENT_FIT_CONTAIN);
    gtk_widget_set_size_request(bild, 640, 400);
    gtk_box_append(GTK_BOX(v), bild);
    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(tv), v);
    adw_dialog_set_child(dlg, tv);
    adw_dialog_present(dlg, win);
}

// Testkrok: MATTE_SKARMBILD=fil.png [MATTE_VY=hem|kort:<id>|ova:<id>|svara:<text>|placering|oversikt]
// ritar fönstret till en PNG och avslutar – för att se fönstret utan skärm (bygget och felsökning).
void skarmbild() {
    const char* fil = g_getenv("MATTE_SKARMBILD");
    if (!fil) return;
    std::string vyer = g_getenv("MATTE_VY") ? g_getenv("MATTE_VY") : "hem";
    for (auto& vy : wenuy::lines(std::string(vyer) + "\n")) {
        for (std::string del; !vy.empty();) {
            size_t p = vy.find(';');
            del = vy.substr(0, p);
            vy = p == std::string::npos ? "" : vy.substr(p + 1);
            if (del.rfind("kort:", 0) == 0) { if (auto* k = M->data.kort(del.substr(5))) M->visaKort(*k); }
            else if (del.rfind("ova:", 0) == 0) { if (auto* k = M->data.kort(del.substr(4))) { M->visaKort(*k, true); } }
            else if (del == "installningar") M->visaInstallningar();
            else if (del == "kolla") { if (M->svarsyta.svar) M->kolla(M->svarsyta.svar()); }
            else if (del == "regelbokvy") M->visaRegelbok();
            else if (del == "fokus") M->satFokus(true);
            else if (del == "helregel") { M->regelHel = true; M->visaRegel(); }
            else if (del == "kortregel") { M->regelHel = false; M->visaRegel(); M->tillUppgiften(); }
            else if (del.rfind("lucka:", 0) == 0) {   // lucka:<nr>:<text>
                auto p = del.find(':', 6);
                size_t i = atoi(del.c_str() + 6);
                if (i < M->luckor.size()) gtk_editable_set_text(GTK_EDITABLE(M->luckor[i]), del.substr(p + 1).c_str());
            }
            else if (del.rfind("kladd:", 0) == 0) raknepapper_fyll(M->papper, del.substr(6));   // kladd:rad1|rad2
            else if (del == "stallupp") { if (M->uppgift.v) raknepapper_stall_upp(M->papper, M->uppgift.v->fraga); }
            else if (del == "helskarm") M->satHelskarm(true);
            else if (del == "karta") M->visaKarta();
            else if (del.rfind("zoom:", 0) == 0) { M->elev.zoom = atof(del.c_str() + 5); M->tillampaZoom(true); }
            else if (del.rfind("svara:", 0) == 0) M->kolla({del.substr(6)});
            else if (del.rfind("mall:", 0) == 0) {   // mall:<kort>:<index> – en viss uppgiftsmall
                auto p = del.find(':', 5);
                if (auto* k = M->data.kort(del.substr(5, p - 5))) { M->tvingaMall = atoi(del.c_str() + p + 1); M->visaKort(*k, true); }
            }
            else if (del == "arbetsblad") { if (M->aktuellt) M->skrivArbetsblad(*M->aktuellt); }
            else if (del == "regelbok") M->skrivRegelbok(false);
            else if (del == "facit" || del == "fel2") {   // facit – eller två fel svar (då visas lösningen och de rätta)
                if (!M->uppgift.v) continue;
                const Variant& v = *M->uppgift.v;
                std::vector<std::string> sv = v.facitSvar;
                if (M->uppgift.mall->typ == "mala") sv.resize(celler(M->data, v.figur).size());
                if (del == "fel2") {
                    if (M->uppgift.mall->typ == "kryssa") { sv.clear(); for (auto& a : v.alternativ) if (std::find(v.facitSvar.begin(), v.facitSvar.end(), a) == v.facitSvar.end()) sv.push_back(a); sv.push_back(v.facitSvar[0]); }
                    else if (!sv.empty()) std::rotate(sv.begin(), sv.begin() + 1, sv.end());
                }
                if (M->svarsyta.fyll) M->svarsyta.fyll(sv);
                M->kolla(sv);
                if (del == "fel2") M->kolla(sv);
            }
            else if (del == "placering") M->borjaPlacering();
            else if (del == "oversikt") M->visaOversikt();
            else if (del == "mork") adw_style_manager_set_color_scheme(adw_style_manager_get_default(), ADW_COLOR_SCHEME_FORCE_DARK);
            else if (del == "lek") { M->elev.leklage = true; M->tillampaLeklage(); M->fyllLista(); }
            else if (del == "ordlista") M->visaOrdlista();
            else if (del == "hem") M->visaHem();
            else if (del == "dagens") M->borjaDagens();
            else if (del.rfind("elev:", 0) == 0) M->visaElevvy(del.substr(5));
            else if (del == "lasupp") M->lasUppUppgift();
            else if (del.rfind("ord:", 0) == 0) M->visaOrd(M->data.ordMedNamn(del.substr(4)));
            else if (del.rfind("markera:", 0) == 0) { if (M->uppgift.v) M->sattText(M->ovaFraga, M->uppgift.v->fraga, true, atoi(del.c_str() + 8)); }
            else if (del == "lattlast") { M->elev.lattlast = true; M->tillampaLeklage(); M->fyllLista(); }
            else if (del == "paus") { M->passGjorda = M->passMal; M->passRatt = M->passMal - 2; M->visaPaus(); }
            else if (del.rfind("namn:", 0) == 0) { M->elev.visningsnamn = del.substr(5); }
        }
    }
    // CI: andra programstarten måste återställa samma pappers-id och rutdata.
    if (g_getenv("MATTE_TEST_PAPPER_RESTORE")) {
        const PapperData pap = raknepapper_hamta(M->papper);
        if (!giltigtPapperId(M->elev.papperId) ||
            pap.id != M->elev.papperId || raknepapper_rader(M->papper) < 2) {
            g_warning("Matte: previously saved working paper was not restored");
            g_application_quit(g_application_get_default());
            return;
        }
    }
    // CI-krok: högre kurser ska visa frågan utan en lång introduktion först.
    if (g_getenv("MATTE_TEST_KORT_REGEL") && M->regelHel) {
        g_warning("Matte: expected a compact initial rule for the current stage");
        g_application_quit(g_application_get_default());
        return;  // ingen PNG => CI-provet misslyckas
    }
    // CI-krok: verifiera att kursen verkligen aktiverat rätt GTK-stilklass.
    if (const char* forvantad = g_getenv("MATTE_TEST_VISUELL")) {
        if (*forvantad && !gtk_widget_has_css_class(M->win, forvantad)) {
            g_warning("Matte: expected visual CSS class '%s' was not set", forvantad);
            g_application_quit(g_application_get_default());
            return;  // ingen PNG => skärmbildsprovet misslyckas
        }
    }
    g_timeout_add_once(1500, +[](gpointer f) {
        GtkWidget* w = M->win;
        GdkPaintable* p = gtk_widget_paintable_new(w);
        int b = gtk_widget_get_width(w), h = gtk_widget_get_height(w);
        GtkSnapshot* s = gtk_snapshot_new();
        gdk_paintable_snapshot(p, GDK_SNAPSHOT(s), b, h);
        GskRenderNode* n = gtk_snapshot_free_to_node(s);
        if (n) {
            GdkTexture* t = gsk_renderer_render_texture(gtk_native_get_renderer(GTK_NATIVE(w)), n, nullptr);
            gdk_texture_save_to_png(t, static_cast<const char*>(f));
            g_object_unref(t);
            gsk_render_node_unref(n);
        }
        g_object_unref(p);
        g_application_quit(g_application_get_default());
    }, g_strdup(fil));
}

}  // namespace

// Kommandoraden – utskrift utan fönster (läraren, skript, testerna):
//   matte --arbetsblad brak-delar[,brak-addition] [--blad 4711] [--antal 8] ut.pdf
//   matte --regelbok [--elev Ali] ut.pdf        (utan --elev: hela regelboken)
//   matte --kort                                 (korten och deras id)
static int kommandorad(int argc, char** argv) {
    std::string lage, kortlista, elevnamn, ut;
    int blad = 0, antal = 8;
    bool lattlast = false, ersatt = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto nasta = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--arbetsblad") { lage = "blad"; kortlista = nasta(); }
        else if (a == "--regelbok") lage = "bok";
        else if (a == "--kort") lage = "kort";
        else if (a == "--blad") blad = atoi(nasta().c_str());
        else if (a == "--antal") antal = std::clamp(atoi(nasta().c_str()), 1, 30);
        else if (a == "--elev") elevnamn = nasta();
        else if (a == "--lattlast") lattlast = true;
        else if (a == "--exportera") { lage = "export"; elevnamn = nasta(); }
        else if (a == "--importera") { lage = "import"; ut = nasta(); }
        else if (a == "--ersatt") ersatt = true;
        else if (a == "--hjalp" || a == "--help") lage = "hjalp";
        else if (!a.empty() && a[0] != '-') ut = a;
    }
    if (lage.empty()) return -1;   // vanlig start: fönstret
    if (lage == "hjalp") {
        printf("%s", tr("matte --arbetsblad <card>[,<card>…] [--blad N] [--antal N] [--lattlast] out.pdf\n"
                        "matte --regelbok [--elev NAME] out.pdf\nmatte --kort\n"
                        "matte --exportera NAME [file.json]\nmatte --importera file.json [--ersatt]\n",
                        "matte --arbetsblad <kort>[,<kort>…] [--blad N] [--antal N] [--lattlast] ut.pdf\n"
                        "matte --regelbok [--elev NAMN] ut.pdf\nmatte --kort\n"
                        "matte --exportera NAMN [fil.json]\nmatte --importera fil.json [--ersatt]\n"));
        return 0;
    }
    std::string fel;
    if (lage == "export") {   // matte --exportera NAMN [fil.json]
        std::string namn = filnamn(elevnamn);
        if (ut.empty()) ut = utskriftsmapp() + "/" + sakertFilnamn("Matte – " + elevnamn) + ".json";
        if (!exporteraElev(namn, ut, &fel)) { fprintf(stderr, "Matte: %s\n", fel.c_str()); return 1; }
        printf("%s\n", ut.c_str());
        return 0;
    }
    if (lage == "import") {   // matte --importera fil.json [--ersatt]
        bool fanns = false;
        std::string visn, namn = importeraElev(ut, ersatt, &fel, &fanns, &visn);
        if (namn.empty()) { fprintf(stderr, "Matte: %s\n", fel.c_str()); return 1; }
        if (fanns) { fprintf(stderr, "Matte: %s %s\n", visn.c_str(), tr("already exists – add --ersatt to replace", "finns redan – lägg till --ersatt för att ersätta")); return 2; }
        printf("%s\n", visn.c_str());
        return 0;
    }
    Data d;
    if (!d.las(Data::hittaMapp(), &fel)) { fprintf(stderr, "Matte: %s\n", fel.c_str()); return 1; }
    if (lage == "kort") {
        for (auto& k : d.kort()) printf("%-34s %s\n", k.id.c_str(), k.titel.c_str());
        return 0;
    }
    std::vector<const Kort*> kort;
    std::vector<std::string> grupper;
    std::string titel, under;
    if (lage == "blad") {
        std::string del;
        for (char c : kortlista + ",") {
            if (c != ',') { del += c; continue; }
            if (del.empty()) continue;
            const Kort* k = d.kort(del);
            if (!k) { fprintf(stderr, "Matte: %s '%s'\n", tr("no card", "inget kort"), del.c_str()); return 1; }
            kort.push_back(k);
            del.clear();
        }
    } else if (!elevnamn.empty()) {
        Elev e = Elev::ladda(filnamn(elevnamn));
        for (auto& id : e.regelbok) if (const Kort* k = d.kort(id)) kort.push_back(k);
        titel = tr("My rule book", "Min regelbok");
        under = e.visningsnamn.empty() ? elevnamn : e.visningsnamn;
    } else {
        kort = kortILaroplansordning(d, &grupper);
        titel = tr("The rule book", "Regelboken");
        under = tr("All cards – from preschool to upper secondary", "Alla kort – från förskola till gymnasium");
    }
    if (kort.empty()) { fprintf(stderr, "Matte: %s\n", tr("no cards to print", "inga kort att skriva ut")); return 1; }
    if (lage == "blad" && blad <= 0) blad = g_random_int_range(1000, 10000);
    if (ut.empty())   // som knapparna i fönstret: "Arbetsblad 4711 – Klockan.pdf", "Min regelbok – Ali.pdf"
        ut = utskriftsmapp() + "/" + sakertFilnamn(lage == "blad" ? std::string(tr("Worksheet ", "Arbetsblad ")) + std::to_string(blad) + " – " + kort[0]->titel
                                                   : elevnamn.empty() ? titel : titel + " – " + under) + ".pdf";
    if (lage == "blad") {
        int nr = arbetsbladPdf(d, kort, ut, blad, antal, &fel, lattlast);
        if (!nr) { fprintf(stderr, "Matte: %s\n", fel.c_str()); return 1; }
        printf("%s %d: %s\n", tr("Sheet", "Blad"), nr, ut.c_str());
    } else {
        if (!regelbokPdf(d, kort, titel, under, ut, &fel, grupper.empty() ? nullptr : &grupper)) { fprintf(stderr, "Matte: %s\n", fel.c_str()); return 1; }
        printf("%s\n", ut.c_str());
    }
    return 0;
}

int main(int argc, char** argv) {
    signal(SIGPIPE, SIG_IGN);   // rösten (aplay, espeak) kan sluta läsa innan vi skrivit klart – det ska inte döda programmet
    laraInit(APP_ID, "MATTE_DATA");
    wenuy::i18nInit(APP_ID);
    if (int rc = kommandorad(argc, argv); rc >= 0) return rc;
    AdwApplication* app = adw_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", CB(+[](GtkApplication* a, gpointer) {
        if (M) { gtk_window_present(GTK_WINDOW(M->win)); return; }
        M = new Matte;
        M->bygg(a);
        skarmbild();
    }), nullptr);
    int rc = g_application_run(G_APPLICATION(app), argc, argv);
    tyst();
    g_object_unref(app);
    return rc;
}

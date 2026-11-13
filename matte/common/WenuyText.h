// libwenuy – formaterad text (grund för ordbehandlare, anteckningar, e-postredigerare).
//
//   wenuy::textTags()          skapar standardformaten i en GtkTextBuffer:
//                              bold, italic, underline, strike, h1, h2, quote, code, center
//   wenuy::bindTextHistory()   ångra/gör om för text OCH formatering via wenuy::History
//   wenuy::textSerialize()/textDeserialize()   filformat (.wdoc, enkel XML, läsbar i texteditor)
//   wenuy::textToPango()       Pango-markup för PDF/utskrift
//   wenuy::textPages()         sidbryt text till A4-sidor och rita en sida (PDF, utskrift, förhandsvisning)
#pragma once
#include <gtk/gtk.h>
#include <pango/pangocairo.h>

#include <memory>
#include <string>
#include <vector>

#include "WenuyDoc.h"

namespace wenuy {

void textTags(GtkTextBuffer* buf);
void bindTextHistory(GtkTextBuffer* buf, History& history);

std::string textSerialize(GtkTextBuffer* buf);                       // → <wdoc>…</wdoc>
bool textDeserialize(GtkTextBuffer* buf, const std::string& data);   // ersätter innehållet
std::string textToPango(GtkTextBuffer* buf);

// Växla ett format på markeringen (eller för ny text vid markören).
void textToggleTag(GtkTextBuffer* buf, const char* tag);
bool textSelectionHas(GtkTextBuffer* buf, const char* tag);

// Sidindelning för utskrift/PDF. Mått i punkter (1/72 tum); marginal på alla sidor.
// Varje stycke får egen layout (rubriker, centrering, citat), raderna fördelas på sidor.
struct TextPages {
    struct Line { int para, line; double y; };
    std::vector<PangoLayout*> paras;
    std::vector<std::vector<Line>> pages;
    double width = 595, height = 842, margin = 64;
    ~TextPages();
    int count() const { return (int)pages.size(); }
    void draw(cairo_t* cr, int page) const;   // rita sidan (origo = sidans övre vänstra hörn)
};
std::unique_ptr<TextPages> textPages(GtkTextBuffer* buf, double width = 595, double height = 842,
                                     double margin = 64, const char* font = "Inter 11");

}  // namespace wenuy

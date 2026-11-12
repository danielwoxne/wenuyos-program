// WenuyOffice – snabb läsning av dokument utan LibreOffice (Snabbtitt, 0.4.0): Word (.docx), Writer (.odt),
// Excel (.xlsx), Calc (.ods), PowerPoint (.pptx) och Impress (.odp) → enkel HTML (rubriker, stycken, listor,
// tabeller, bilder utelämnas). Bara textinnehållet – exakt utseende får man med LibreOffice. Rena funktioner
// över XML-texten (GMarkup), så att de kan provas; zip-läsningen (libarchive) sker i FileView.
#pragma once
#include <glib.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace wenuy::office {

inline std::string esc(const std::string& s) {
    gchar* e = g_markup_escape_text(s.c_str(), -1);
    std::string r = e;
    g_free(e);
    return r;
}

// ---- Word / Writer → HTML
struct DocState {
    std::string html, para, cell, row;
    int depth = 0;                 // inuti <w:p>/<text:p>
    bool inTable = false, inRow = false, inCell = false, inText = false;
    std::string heading;           // "h1"… för aktuellt stycke
    bool list = false, bold = false;
    int listDepth = 0;             // inuti text:list-item (ODT)
    std::string listOpen;          // "ul" medan en lista pågår
    std::map<std::string, std::string> odtStyles;   // ODT: stilnamn → "h1"/"li"
    bool docx = false;
};

inline std::string attr(const gchar** names, const gchar** values, const char* name) {
    for (int i = 0; names && names[i]; ++i) if (g_strcmp0(names[i], name) == 0) return values[i];
    return "";
}

inline void docStart(GMarkupParseContext*, const gchar* el, const gchar** an, const gchar** av, gpointer ud, GError**) {
    auto* s = static_cast<DocState*>(ud);
    const std::string e = el;
    if (e == "w:p" || e == "text:p" || e == "text:h") {
        s->para.clear(); s->heading.clear(); s->list = s->listDepth > 0; s->depth = 1;
        if (e == "text:h") { const std::string lvl = attr(an, av, "text:outline-level"); s->heading = "h" + (lvl.empty() ? std::string("1") : lvl.substr(0, 1)); }
        if (e == "text:p") { const std::string st = attr(an, av, "text:style-name"); auto it = s->odtStyles.find(st); if (it != s->odtStyles.end()) { if (it->second == "li") s->list = true; else s->heading = it->second; } }
    } else if (e == "w:pStyle") {
        const std::string v = attr(an, av, "w:val");
        if (g_str_has_prefix(v.c_str(), "Heading") || g_str_has_prefix(v.c_str(), "Rubrik") || g_str_has_prefix(v.c_str(), "berschrift")) {
            const char lvl = v.empty() ? '1' : v.back();
            s->heading = std::string("h") + (lvl >= '1' && lvl <= '6' ? lvl : '2');
        } else if (v == "Title" || v == "Titel") s->heading = "h1";
        else if (g_str_has_prefix(v.c_str(), "List") || g_str_has_prefix(v.c_str(), "Punktlista")) s->list = true;
    } else if (e == "w:numPr") {
        s->list = true;
    } else if (e == "text:list-item") {
        ++s->listDepth;
    } else if (e == "w:t" || e == "text:span" || e == "text:a") {
        s->inText = true;
    } else if (e == "w:tab" || e == "text:tab") {
        s->para += "&#9;";
    } else if (e == "w:br" || e == "text:line-break") {
        s->para += "<br>";
    } else if (e == "text:s") {
        const std::string c = attr(an, av, "text:c");
        s->para += std::string(c.empty() ? 1 : std::max(1, atoi(c.c_str())), ' ');
    } else if (e == "w:b" && attr(an, av, "w:val") != "0" && attr(an, av, "w:val") != "false") {
        s->bold = true;
    } else if (e == "w:tbl" || e == "table:table") {
        s->inTable = true; s->html += "<table>\n";
    } else if (e == "w:tr" || e == "table:table-row") {
        s->inRow = true; s->row.clear();
    } else if (e == "w:tc" || e == "table:table-cell") {
        s->inCell = true; s->cell.clear();
    } else if (e == "style:style") {   // ODT: automatiska stilar som ärver rubrik/lista
        const std::string name = attr(an, av, "style:name"), parent = attr(an, av, "style:parent-style-name"), lst = attr(an, av, "style:list-style-name");
        if (g_str_has_prefix(parent.c_str(), "Heading_20_")) s->odtStyles[name] = "h" + parent.substr(11, 1);
        else if (parent == "Title") s->odtStyles[name] = "h1";
        else if (!lst.empty() || g_str_has_prefix(parent.c_str(), "List")) s->odtStyles[name] = "li";
    }
    if (e == "w:r") s->bold = false;
}

inline void docText(GMarkupParseContext*, const gchar* text, gsize len, gpointer ud, GError**) {
    auto* s = static_cast<DocState*>(ud);
    if (!s->depth) return;
    if (s->docx && !s->inText) return;   // Word: bara texten i <w:t>
    std::string t = esc(std::string(text, len));
    if (s->bold) t = "<b>" + t + "</b>";
    s->para += t;
}

inline void docEnd(GMarkupParseContext*, const gchar* el, gpointer ud, GError**) {
    auto* s = static_cast<DocState*>(ud);
    const std::string e = el;
    if (e == "w:p" || e == "text:p" || e == "text:h") {
        s->depth = 0;
        std::string out;
        if (s->list) out = "<li>" + s->para + "</li>\n";
        else if (!s->heading.empty()) out = "<" + s->heading + ">" + s->para + "</" + s->heading + ">\n";
        else out = "<p>" + (s->para.empty() ? std::string("&nbsp;") : s->para) + "</p>\n";
        std::string& dst = s->inCell ? s->cell : s->html;
        if (!s->inCell) {   // listor: öppna/stäng <ul>
            if (s->list && s->listOpen.empty()) { dst += "<ul>\n"; s->listOpen = "ul"; }
            if (!s->list && !s->listOpen.empty()) { dst += "</ul>\n"; s->listOpen.clear(); }
        }
        dst += out;
        s->para.clear();
    } else if (e == "w:t" || e == "text:span" || e == "text:a") {
        s->inText = false;
    } else if (e == "text:list-item") {
        if (s->listDepth > 0) --s->listDepth;
    } else if (e == "w:tc" || e == "table:table-cell") {
        s->inCell = false;
        s->row += "<td>" + s->cell + "</td>";
    } else if (e == "w:tr" || e == "table:table-row") {
        s->inRow = false;
        s->html += "<tr>" + s->row + "</tr>\n";
    } else if (e == "w:tbl" || e == "table:table") {
        s->inTable = false;
        if (!s->listOpen.empty()) { s->html += "</ul>\n"; s->listOpen.clear(); }
        s->html += "</table>\n";
    }
}

inline std::string docToHtml(const std::string& xml, const std::string& stylesXml = "") {
    DocState s;
    s.docx = xml.find("<w:") != std::string::npos;
    GMarkupParser p = {docStart, docEnd, docText, nullptr, nullptr};
    // ODT: stilarna står i början av content.xml (automatic-styles) – samma genomgång tar med dem
    for (const std::string* x : {&stylesXml, &xml}) {
        if (x->empty()) continue;
        GMarkupParseContext* c = g_markup_parse_context_new(&p, GMarkupParseFlags(0), &s, nullptr);
        g_markup_parse_context_parse(c, x->c_str(), x->size(), nullptr);
        g_markup_parse_context_end_parse(c, nullptr);
        g_markup_parse_context_free(c);
    }
    if (!s.listOpen.empty()) s.html += "</ul>\n";
    return s.html;
}

// ---- Excel / Calc → tabeller
struct Sheet { std::string name; std::vector<std::vector<std::string>> rows; };

inline std::vector<std::string> sharedStrings(const std::string& xml) {
    struct S { std::vector<std::string> v; std::string cur; bool inT = false, inSi = false; } st;
    GMarkupParser p = {
        +[](GMarkupParseContext*, const gchar* el, const gchar**, const gchar**, gpointer ud, GError**) {
            auto* s = static_cast<S*>(ud);
            if (!g_strcmp0(el, "si")) { s->inSi = true; s->cur.clear(); }
            else if (!g_strcmp0(el, "t")) s->inT = true;
        },
        +[](GMarkupParseContext*, const gchar* el, gpointer ud, GError**) {
            auto* s = static_cast<S*>(ud);
            if (!g_strcmp0(el, "si")) { s->inSi = false; s->v.push_back(s->cur); }
            else if (!g_strcmp0(el, "t")) s->inT = false;
        },
        +[](GMarkupParseContext*, const gchar* t, gsize n, gpointer ud, GError**) { auto* s = static_cast<S*>(ud); if (s->inT && s->inSi) s->cur.append(t, n); },
        nullptr, nullptr};
    GMarkupParseContext* c = g_markup_parse_context_new(&p, GMarkupParseFlags(0), &st, nullptr);
    g_markup_parse_context_parse(c, xml.c_str(), xml.size(), nullptr);
    g_markup_parse_context_free(c);
    return st.v;
}

inline int colIndex(const std::string& ref) {   // "C12" → 2
    int col = 0;
    for (char ch : ref) { if (ch < 'A' || ch > 'Z') break; col = col * 26 + (ch - 'A' + 1); }
    return std::max(0, col - 1);
}

inline Sheet xlsxSheet(const std::string& xml, const std::vector<std::string>& shared, size_t maxCells = 4000) {
    struct S { Sheet sh; const std::vector<std::string>* shared; std::vector<std::string> row; int col = 0; std::string type, val; bool inV = false, inIs = false; size_t cells = 0, maxCells; } st;
    st.shared = &shared; st.maxCells = maxCells;
    GMarkupParser p = {
        +[](GMarkupParseContext*, const gchar* el, const gchar** an, const gchar** av, gpointer ud, GError**) {
            auto* s = static_cast<S*>(ud);
            const std::string e = el;
            if (e == "row") { s->row.clear(); }
            else if (e == "c") { s->col = colIndex(attr(an, av, "r")); s->type = attr(an, av, "t"); s->val.clear(); }
            else if (e == "v") s->inV = true;
            else if (e == "is") s->inIs = true;
            else if (e == "t" && s->inIs) s->inV = true;
        },
        +[](GMarkupParseContext*, const gchar* el, gpointer ud, GError**) {
            auto* s = static_cast<S*>(ud);
            const std::string e = el;
            if (e == "v" || e == "t") s->inV = false;
            else if (e == "is") s->inIs = false;
            else if (e == "c") {
                std::string v = s->val;
                if (s->type == "s") { const size_t i = (size_t)atoi(v.c_str()); v = i < s->shared->size() ? (*s->shared)[i] : ""; }
                else if (s->type == "b") v = v == "1" ? "SANT" : "FALSKT";
                if ((int)s->row.size() <= s->col) s->row.resize(s->col + 1);
                s->row[s->col] = v;
                ++s->cells;
            } else if (e == "row") {
                if (s->cells <= s->maxCells) s->sh.rows.push_back(s->row);
            }
        },
        +[](GMarkupParseContext*, const gchar* t, gsize n, gpointer ud, GError**) { auto* s = static_cast<S*>(ud); if (s->inV) s->val.append(t, n); },
        nullptr, nullptr};
    GMarkupParseContext* c = g_markup_parse_context_new(&p, GMarkupParseFlags(0), &st, nullptr);
    g_markup_parse_context_parse(c, xml.c_str(), xml.size(), nullptr);
    g_markup_parse_context_free(c);
    return st.sh;
}

// ODS: alla blad i content.xml (table:table), celler med text:p; upprepade kolumner (number-columns-repeated)
inline std::vector<Sheet> odsSheets(const std::string& xml, size_t maxCells = 4000) {
    struct S { std::vector<Sheet> sheets; std::vector<std::string> row; std::string cell; int rep = 1; bool inCell = false, inP = false; size_t cells = 0, maxCells; } st;
    st.maxCells = maxCells;
    GMarkupParser p = {
        +[](GMarkupParseContext*, const gchar* el, const gchar** an, const gchar** av, gpointer ud, GError**) {
            auto* s = static_cast<S*>(ud);
            const std::string e = el;
            if (e == "table:table") { s->sheets.push_back({attr(an, av, "table:name"), {}}); }
            else if (e == "table:table-row") s->row.clear();
            else if (e == "table:table-cell" || e == "table:covered-table-cell") {
                s->inCell = true; s->cell.clear();
                const std::string r = attr(an, av, "table:number-columns-repeated");
                s->rep = r.empty() ? 1 : std::min(50, std::max(1, atoi(r.c_str())));
            } else if (e == "text:p" && s->inCell) { if (!s->cell.empty()) s->cell += "\n"; s->inP = true; }
        },
        +[](GMarkupParseContext*, const gchar* el, gpointer ud, GError**) {
            auto* s = static_cast<S*>(ud);
            const std::string e = el;
            if (e == "text:p") s->inP = false;
            else if (e == "table:table-cell" || e == "table:covered-table-cell") {
                s->inCell = false;
                for (int i = 0; i < s->rep; ++i) s->row.push_back(s->cell);
                s->cells += s->rep;
            } else if (e == "table:table-row") {
                while (!s->row.empty() && s->row.back().empty()) s->row.pop_back();   // tomma svansar (upprepade tomma celler)
                if (!s->sheets.empty() && s->cells <= s->maxCells && !s->row.empty()) s->sheets.back().rows.push_back(s->row);
            }
        },
        +[](GMarkupParseContext*, const gchar* t, gsize n, gpointer ud, GError**) { auto* s = static_cast<S*>(ud); if (s->inP) s->cell.append(t, n); },
        nullptr, nullptr};
    GMarkupParseContext* c = g_markup_parse_context_new(&p, GMarkupParseFlags(0), &st, nullptr);
    g_markup_parse_context_parse(c, xml.c_str(), xml.size(), nullptr);
    g_markup_parse_context_free(c);
    return st.sheets;
}

inline std::string sheetsToHtml(const std::vector<Sheet>& sheets) {
    std::string h;
    for (auto& s : sheets) {
        if (s.rows.empty()) continue;
        if (sheets.size() > 1) h += "<h2>" + esc(s.name) + "</h2>\n";
        h += "<table class=\"sheet\">\n";
        size_t cols = 0;
        for (auto& r : s.rows) cols = std::max(cols, r.size());
        cols = std::min<size_t>(cols, 40);
        for (auto& r : s.rows) {
            h += "<tr>";
            for (size_t i = 0; i < cols; ++i) h += "<td>" + (i < r.size() ? esc(r[i]) : std::string()) + "</td>";
            h += "</tr>\n";
        }
        h += "</table>\n";
    }
    return h;
}

// ---- PowerPoint / Impress: texten per bild
inline std::vector<std::string> slideParagraphs(const std::string& xml) {   // pptx: a:p med a:t; odp: text:p/text:span
    struct S { std::vector<std::string> paras; std::string cur; bool inT = false, inP = false; } st;
    GMarkupParser p = {
        +[](GMarkupParseContext*, const gchar* el, const gchar**, const gchar**, gpointer ud, GError**) {
            auto* s = static_cast<S*>(ud);
            if (!g_strcmp0(el, "a:p") || !g_strcmp0(el, "text:p")) { s->inP = true; s->cur.clear(); }
            else if (!g_strcmp0(el, "a:t") || !g_strcmp0(el, "text:span")) s->inT = true;
        },
        +[](GMarkupParseContext*, const gchar* el, gpointer ud, GError**) {
            auto* s = static_cast<S*>(ud);
            if (!g_strcmp0(el, "a:p") || !g_strcmp0(el, "text:p")) {
                s->inP = false;
                std::string t = s->cur;
                while (!t.empty() && (t.back() == ' ' || t.back() == '\n')) t.pop_back();
                while (!t.empty() && (t.front() == ' ' || t.front() == '\n')) t.erase(0, 1);
                if (!t.empty()) s->paras.push_back(t);
            }
            else if (!g_strcmp0(el, "a:t") || !g_strcmp0(el, "text:span")) s->inT = false;
        },
        +[](GMarkupParseContext*, const gchar* t, gsize n, gpointer ud, GError**) { auto* s = static_cast<S*>(ud); if (s->inP) s->cur.append(t, n); },   // pptx: a:t; odp: text direkt i text:p/text:span
        nullptr, nullptr};
    GMarkupParseContext* c = g_markup_parse_context_new(&p, GMarkupParseFlags(0), &st, nullptr);
    g_markup_parse_context_parse(c, xml.c_str(), xml.size(), nullptr);
    g_markup_parse_context_free(c);
    return st.paras;
}

inline std::string slidesToHtml(const std::vector<std::vector<std::string>>& slides, const char* slideWord) {
    std::string h;
    int n = 0;
    for (auto& s : slides) {
        ++n;
        h += "<div class=\"slide\"><div class=\"num\">" + std::string(slideWord) + " " + std::to_string(n) + "</div>\n";
        bool first = true;
        for (auto& para : s) { h += first ? "<h2>" + esc(para) + "</h2>\n" : "<p>" + esc(para) + "</p>\n"; first = false; }
        h += "</div>\n";
    }
    return h;
}

// Sorteringsnyckel för slide1.xml, slide2.xml … slide10.xml
inline int slideNumber(const std::string& path) {
    const size_t p = path.rfind("slide");
    return p == std::string::npos ? 0 : atoi(path.c_str() + p + 5);
}

inline std::string page(const std::string& body, const std::string& title, bool dark) {
    return "<!doctype html><meta charset=\"utf-8\"><title>" + esc(title) + "</title><style>"
           "body{font-family:Inter,sans-serif;font-size:15px;line-height:1.5;max-width:820px;margin:0 auto;padding:28px 36px;"
           + std::string(dark ? "background:#1e1e1e;color:#e6e6e6" : "background:#fff;color:#222") + "}"
           "h1{font-size:1.7em}h2{font-size:1.3em}table{border-collapse:collapse;margin:10px 0}td{border:1px solid "
           + (dark ? "#444" : "#ccc") + ";padding:3px 8px;vertical-align:top}table.sheet td{white-space:nowrap;font-variant-numeric:tabular-nums}"
           ".slide{border:1px solid " + (dark ? "#444" : "#ddd") + ";border-radius:8px;padding:14px 18px;margin:14px 0}.num{font-size:.8em;opacity:.6}"
           "p{margin:.35em 0}ul{margin:.3em 0}</style><body>" + body + "</body>";
}

}  // namespace wenuy::office

#include "WenuyText.h"

#include <cstring>
#include <map>
#include <set>

#include "Wenuy.h"

namespace wenuy {

static const char* kParaTags[] = {"h1", "h2", "quote", "center", nullptr};

static bool isParaTag(const char* t) {
    for (int i = 0; kParaTags[i]; ++i) if (g_str_equal(t, kParaTags[i])) return true;
    return false;
}

void textTags(GtkTextBuffer* buf) {
    auto* tt = gtk_text_buffer_get_tag_table(buf);
    auto mk = [&](const char* name, auto... props) {
        if (!gtk_text_tag_table_lookup(tt, name)) gtk_text_buffer_create_tag(buf, name, props..., nullptr);
    };
    mk("bold", "weight", PANGO_WEIGHT_BOLD);
    mk("italic", "style", PANGO_STYLE_ITALIC);
    mk("underline", "underline", PANGO_UNDERLINE_SINGLE);
    mk("strike", "strikethrough", TRUE);
    mk("code", "family", "monospace", "background", "rgba(127,127,127,0.12)");
    mk("h1", "scale", 1.8, "weight", PANGO_WEIGHT_BOLD, "pixels-above-lines", 14, "pixels-below-lines", 6);
    mk("h2", "scale", 1.35, "weight", PANGO_WEIGHT_SEMIBOLD, "pixels-above-lines", 10, "pixels-below-lines", 4);
    mk("quote", "left-margin", 28, "style", PANGO_STYLE_ITALIC, "foreground", "#6b7280");
    mk("center", "justification", GTK_JUSTIFY_CENTER);
}

// ---------------------------------------------------------------- segment
struct Seg { std::string text; std::vector<std::string> tags; };

static std::vector<std::string> namedTags(const GtkTextIter* it) {
    std::vector<std::string> out;
    GSList* l = gtk_text_iter_get_tags(it);
    for (GSList* p = l; p; p = p->next) {
        gchar* n = nullptr;
        g_object_get(p->data, "name", &n, nullptr);
        if (n) out.emplace_back(n);
        g_free(n);
    }
    g_slist_free(l);
    return out;
}

static std::vector<Seg> segments(GtkTextBuffer* buf, const GtkTextIter* s, const GtkTextIter* e) {
    std::vector<Seg> out;
    GtkTextIter a = *s;
    while (gtk_text_iter_compare(&a, e) < 0) {
        GtkTextIter b = a;
        gtk_text_iter_forward_to_tag_toggle(&b, nullptr);
        if (gtk_text_iter_compare(&b, e) > 0) b = *e;
        if (gtk_text_iter_equal(&a, &b)) gtk_text_iter_forward_char(&b);
        gchar* t = gtk_text_buffer_get_text(buf, &a, &b, FALSE);
        out.push_back({t, namedTags(&a)});
        g_free(t);
        a = b;
    }
    return out;
}

static void insertSegs(GtkTextBuffer* buf, int offset, const std::vector<Seg>& segs) {
    GtkTextIter it;
    gtk_text_buffer_get_iter_at_offset(buf, &it, offset);
    for (const auto& s : segs) {
        const int start = gtk_text_iter_get_offset(&it);
        gtk_text_buffer_insert(buf, &it, s.text.c_str(), -1);
        GtkTextIter a;
        gtk_text_buffer_get_iter_at_offset(buf, &a, start);
        for (const auto& t : s.tags) gtk_text_buffer_apply_tag_by_name(buf, t.c_str(), &a, &it);
    }
}

// ---------------------------------------------------------------- ångra/gör om
struct TH {
    GtkTextBuffer* buf;
    History* h;
    bool replay = false;
    int inserting = 0;
    std::map<std::string, bool> pending;   // format som slagits på/av vid markören utan markering
};

static bool historyOff(GtkTextBuffer* b) { return g_object_get_data(G_OBJECT(b), "wenuy-history-off") != nullptr; }

static void range(GtkTextBuffer* b, int s, int e, GtkTextIter* a, GtkTextIter* z) {
    gtk_text_buffer_get_iter_at_offset(b, a, s);
    gtk_text_buffer_get_iter_at_offset(b, z, e);
}

static std::vector<std::pair<int, int>> tagRanges(GtkTextBuffer* b, const char* tag, int s, int e) {
    std::vector<std::pair<int, int>> out;
    auto* t = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(b), tag);
    GtkTextIter it;
    gtk_text_buffer_get_iter_at_offset(b, &it, s);
    int start = -1;
    for (int o = s; o < e; ++o) {
        const bool has = gtk_text_iter_has_tag(&it, t);
        if (has && start < 0) start = o;
        if (!has && start >= 0) { out.push_back({start, o}); start = -1; }
        gtk_text_iter_forward_char(&it);
    }
    if (start >= 0) out.push_back({start, e});
    return out;
}

void bindTextHistory(GtkTextBuffer* buf, History& history) {
    gtk_text_buffer_set_enable_undo(buf, FALSE);   // WenuyOS-historiken tar över (även formatering)
    auto* th = new TH{buf, &history};
    g_object_set_data_full(G_OBJECT(buf), "wenuy-th", th, +[](gpointer p) { delete static_cast<TH*>(p); });

    // Kör en ångra/gör om-åtgärd utan att den själv hamnar i historiken; markören följer med.
    auto replay = [th](std::function<void()> f, int cursor) {
        th->replay = true;
        f();
        th->replay = false;
        GtkTextIter it;
        gtk_text_buffer_get_iter_at_offset(th->buf, &it, cursor);
        gtk_text_buffer_place_cursor(th->buf, &it);
    };

    // --- text skrivs in
    g_signal_connect(buf, "insert-text", CB(+[](GtkTextBuffer* b, GtkTextIter* loc, char* text, int len, gpointer p) {
        auto* th = static_cast<TH*>(p);
        th->inserting++;
        if (th->replay || historyOff(b)) return;
        const int off = gtk_text_iter_get_offset(loc);
        std::string t(text, len);
        const int n = (int)g_utf8_strlen(t.c_str(), -1);
        // Ett ångra-steg per ord: bokstäver slås ihop, mellanslag/skiljetecken/radbrytning bryter
        const bool word = n == 1 && g_unichar_isalnum(g_utf8_get_char(t.c_str()));
        auto rp = static_cast<std::function<void(std::function<void()>, int)>*>(g_object_get_data(G_OBJECT(b), "wenuy-replay"));
        th->h->record(tr("Typing", "Skrivning"),
            [th, rp, off, t, n]() { (*rp)([th, off, t]() { GtkTextIter i; gtk_text_buffer_get_iter_at_offset(th->buf, &i, off); gtk_text_buffer_insert(th->buf, &i, t.c_str(), -1); }, off + n); },
            [th, rp, off, n]() { (*rp)([th, off, n]() { GtkTextIter a, z; range(th->buf, off, off + n, &a, &z); gtk_text_buffer_delete(th->buf, &a, &z); }, off); },
            word ? "type" : "");
    }), th);
    // Efter inskrivning: fortsätt föregående teckens format (+ väntande av/på), som i en ordbehandlare
    g_signal_connect_after(buf, "insert-text", CB(+[](GtkTextBuffer* b, GtkTextIter* loc, char* text, int len, gpointer p) {
        auto* th = static_cast<TH*>(p);
        th->inserting--;
        if (th->replay || historyOff(b)) return;
        const int end = gtk_text_iter_get_offset(loc);
        const int n = (int)g_utf8_strlen(text, len);
        const int start = end - n;
        std::set<std::string> tags;
        if (start > 0 && std::string(text, len) != "\n") {
            GtkTextIter prev;
            gtk_text_buffer_get_iter_at_offset(b, &prev, start - 1);
            if (gtk_text_iter_get_char(&prev) != '\n')
                for (auto& t : namedTags(&prev)) tags.insert(t);
            else {   // nytt stycke: behåll teckenformat, inte rubrik/centrering/citat
                for (auto& t : namedTags(&prev)) if (!isParaTag(t.c_str())) tags.insert(t);
            }
        }
        for (auto& [t, on] : th->pending) { if (on) tags.insert(t); else tags.erase(t); }
        if (!tags.empty()) {
            GtkTextIter a, z;
            range(b, start, end, &a, &z);
            th->inserting++;   // "mark-set" ska inte rensa pending mitt i
            for (auto& t : tags) gtk_text_buffer_apply_tag_by_name(b, t.c_str(), &a, &z);
            th->inserting--;
        }
        gtk_text_buffer_get_iter_at_offset(b, loc, end);   // giltig igen för andra lyssnare
    }), th);

    // --- text tas bort (med formatering, så att ångra återställer den)
    g_signal_connect(buf, "delete-range", CB(+[](GtkTextBuffer* b, GtkTextIter* s, GtkTextIter* e, gpointer p) {
        auto* th = static_cast<TH*>(p);
        if (th->replay || historyOff(b)) return;
        const int so = gtk_text_iter_get_offset(s), eo = gtk_text_iter_get_offset(e);
        auto segs = segments(b, s, e);
        auto rp = static_cast<std::function<void(std::function<void()>, int)>*>(g_object_get_data(G_OBJECT(b), "wenuy-replay"));
        th->h->record(tr("Delete", "Radera"),
            [th, rp, so, eo]() { (*rp)([th, so, eo]() { GtkTextIter a, z; range(th->buf, so, eo, &a, &z); gtk_text_buffer_delete(th->buf, &a, &z); }, so); },
            [th, rp, so, eo, segs]() { (*rp)([th, so, segs]() { insertSegs(th->buf, so, segs); }, eo); },
            eo - so == 1 ? "delete" : "");
    }), th);

    // --- formatering
    auto onTag = +[](GtkTextBuffer* b, GtkTextTag* tag, GtkTextIter* s, GtkTextIter* e, gpointer p, bool apply) {
        auto* th = static_cast<TH*>(p);
        if (th->replay || historyOff(b)) return;
        gchar* n = nullptr;
        g_object_get(tag, "name", &n, nullptr);
        if (!n) return;
        std::string name = n;
        g_free(n);
        const int so = gtk_text_iter_get_offset(s), eo = gtk_text_iter_get_offset(e);
        auto before = tagRanges(b, name.c_str(), so, eo);
        auto rp = static_cast<std::function<void(std::function<void()>, int)>*>(g_object_get_data(G_OBJECT(b), "wenuy-replay"));
        auto set = [th, name, so, eo](bool on) {
            GtkTextIter a, z; range(th->buf, so, eo, &a, &z);
            if (on) gtk_text_buffer_apply_tag_by_name(th->buf, name.c_str(), &a, &z);
            else gtk_text_buffer_remove_tag_by_name(th->buf, name.c_str(), &a, &z);
        };
        auto restore = [th, name, before]() {
            for (auto& [x, y] : before) { GtkTextIter a, z; range(th->buf, x, y, &a, &z); gtk_text_buffer_apply_tag_by_name(th->buf, name.c_str(), &a, &z); }
        };
        th->h->record(tr("Formatting", "Formatering"),
            [rp, set, apply, eo]() { (*rp)([set, apply]() { set(apply); }, eo); },
            [rp, set, restore, eo]() { (*rp)([set, restore]() { set(false); restore(); }, eo); },
            th->inserting ? "type" : "");
    };
    g_object_set_data(G_OBJECT(buf), "wenuy-ontag", (gpointer)onTag);
    g_signal_connect(buf, "apply-tag", CB(+[](GtkTextBuffer* b, GtkTextTag* t, GtkTextIter* s, GtkTextIter* e, gpointer p) {
        using F = void (*)(GtkTextBuffer*, GtkTextTag*, GtkTextIter*, GtkTextIter*, gpointer, bool);
        ((F)g_object_get_data(G_OBJECT(b), "wenuy-ontag"))(b, t, s, e, p, true);
    }), th);
    g_signal_connect(buf, "remove-tag", CB(+[](GtkTextBuffer* b, GtkTextTag* t, GtkTextIter* s, GtkTextIter* e, gpointer p) {
        using F = void (*)(GtkTextBuffer*, GtkTextTag*, GtkTextIter*, GtkTextIter*, gpointer, bool);
        ((F)g_object_get_data(G_OBJECT(b), "wenuy-ontag"))(b, t, s, e, p, false);
    }), th);

    // Väntande format gäller bara tills markören flyttas
    g_signal_connect(buf, "mark-set", CB(+[](GtkTextBuffer* b, GtkTextIter*, GtkTextMark* m, gpointer p) {
        auto* th = static_cast<TH*>(p);
        if (!th->inserting && m == gtk_text_buffer_get_insert(b)) th->pending.clear();
    }), th);

    auto* rp = new std::function<void(std::function<void()>, int)>(replay);
    g_object_set_data_full(G_OBJECT(buf), "wenuy-replay", rp, +[](gpointer p) { delete static_cast<std::function<void(std::function<void()>, int)>*>(p); });
}

// ---------------------------------------------------------------- formatera markering
static void paraExtend(GtkTextIter* s, GtkTextIter* e) {
    gtk_text_iter_set_line_offset(s, 0);
    if (!gtk_text_iter_ends_line(e)) gtk_text_iter_forward_to_line_end(e);
}

bool textSelectionHas(GtkTextBuffer* buf, const char* tag) {
    auto* t = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(buf), tag);
    if (!t) return false;
    GtkTextIter s, e;
    if (!gtk_text_buffer_get_selection_bounds(buf, &s, &e)) {
        auto* th = static_cast<TH*>(g_object_get_data(G_OBJECT(buf), "wenuy-th"));
        if (th && th->pending.count(tag)) return th->pending[tag];
        if (!gtk_text_iter_is_start(&s) && !isParaTag(tag)) gtk_text_iter_backward_char(&s);
        return gtk_text_iter_has_tag(&s, t);
    }
    if (isParaTag(tag)) paraExtend(&s, &e);
    for (GtkTextIter i = s; gtk_text_iter_compare(&i, &e) < 0; gtk_text_iter_forward_char(&i))
        if (!gtk_text_iter_has_tag(&i, t) && gtk_text_iter_get_char(&i) != '\n') return false;
    return true;
}

void textToggleTag(GtkTextBuffer* buf, const char* tag) {
    GtkTextIter s, e;
    const bool sel = gtk_text_buffer_get_selection_bounds(buf, &s, &e);
    const bool on = !textSelectionHas(buf, tag);
    if (!sel && !isParaTag(tag)) {   // ingen markering: gäller texten man skriver härnäst
        if (auto* th = static_cast<TH*>(g_object_get_data(G_OBJECT(buf), "wenuy-th"))) th->pending[tag] = on;
        return;
    }
    if (isParaTag(tag)) paraExtend(&s, &e);
    gtk_text_buffer_begin_user_action(buf);
    if (on && (g_str_equal(tag, "h1") || g_str_equal(tag, "h2")))   // en rubriknivå i taget
        gtk_text_buffer_remove_tag_by_name(buf, g_str_equal(tag, "h1") ? "h2" : "h1", &s, &e);
    if (on) gtk_text_buffer_apply_tag_by_name(buf, tag, &s, &e);
    else gtk_text_buffer_remove_tag_by_name(buf, tag, &s, &e);
    gtk_text_buffer_end_user_action(buf);
}

// ---------------------------------------------------------------- filformat
std::string textSerialize(GtkTextBuffer* buf) {
    GtkTextIter s, e;
    gtk_text_buffer_get_bounds(buf, &s, &e);
    std::string out = "<wdoc version=\"1\">";
    for (auto& seg : segments(buf, &s, &e)) {
        gchar* esc = g_markup_escape_text(seg.text.c_str(), -1);
        if (seg.tags.empty()) out += esc;
        else {
            std::string t;
            for (auto& x : seg.tags) t += (t.empty() ? "" : " ") + x;
            out += "<r t=\"" + t + "\">" + esc + "</r>";
        }
        g_free(esc);
    }
    return out + "</wdoc>\n";
}

struct Parse { GtkTextBuffer* buf; std::vector<std::string> tags; bool ok = false; };

bool textDeserialize(GtkTextBuffer* buf, const std::string& data) {
    textTags(buf);
    Parse ps{buf, {}};
    GMarkupParser p = {};
    p.start_element = +[](GMarkupParseContext*, const char* el, const char** names, const char** vals, gpointer d, GError**) {
        auto* ps = static_cast<Parse*>(d);
        if (g_str_equal(el, "wdoc")) ps->ok = true;
        if (!g_str_equal(el, "r")) return;
        ps->tags.clear();
        for (int i = 0; names[i]; ++i)
            if (g_str_equal(names[i], "t")) {
                gchar** v = g_strsplit(vals[i], " ", -1);
                for (int j = 0; v[j]; ++j) if (*v[j]) ps->tags.emplace_back(v[j]);
                g_strfreev(v);
            }
    };
    p.end_element = +[](GMarkupParseContext*, const char* el, gpointer d, GError**) {
        if (g_str_equal(el, "r")) static_cast<Parse*>(d)->tags.clear();
    };
    p.text = +[](GMarkupParseContext*, const char* text, gsize len, gpointer d, GError**) {
        auto* ps = static_cast<Parse*>(d);
        if (!ps->ok) return;
        GtkTextIter end;
        gtk_text_buffer_get_end_iter(ps->buf, &end);
        const int start = gtk_text_iter_get_offset(&end);
        gtk_text_buffer_insert(ps->buf, &end, text, (int)len);
        GtkTextIter a;
        gtk_text_buffer_get_iter_at_offset(ps->buf, &a, start);
        auto* tt = gtk_text_buffer_get_tag_table(ps->buf);
        for (auto& t : ps->tags) if (gtk_text_tag_table_lookup(tt, t.c_str())) gtk_text_buffer_apply_tag_by_name(ps->buf, t.c_str(), &a, &end);
    };
    g_object_set_data(G_OBJECT(buf), "wenuy-history-off", GINT_TO_POINTER(1));
    gtk_text_buffer_set_text(buf, "", 0);
    auto* ctx = g_markup_parse_context_new(&p, G_MARKUP_DEFAULT_FLAGS, &ps, nullptr);
    const bool ok = g_markup_parse_context_parse(ctx, data.data(), (gssize)data.size(), nullptr) &&
                    g_markup_parse_context_end_parse(ctx, nullptr) && ps.ok;
    g_markup_parse_context_free(ctx);
    g_object_set_data(G_OBJECT(buf), "wenuy-history-off", nullptr);
    GtkTextIter s;
    gtk_text_buffer_get_start_iter(buf, &s);
    gtk_text_buffer_place_cursor(buf, &s);
    return ok;
}

// ---------------------------------------------------------------- Pango / PDF
static std::string pangoSpan(const std::vector<std::string>& tags, const std::string& escaped) {
    std::string open, close;
    auto has = [&](const char* t) { for (auto& x : tags) if (x == t) return true; return false; };
    std::string attrs;
    if (has("bold")) attrs += " weight=\"bold\"";
    if (has("italic") || has("quote")) attrs += " style=\"italic\"";
    if (has("underline")) attrs += " underline=\"single\"";
    if (has("strike")) attrs += " strikethrough=\"true\"";
    if (has("code")) attrs += " font_family=\"monospace\"";
    if (has("h1")) attrs += " size=\"180%\" weight=\"bold\"";
    else if (has("h2")) attrs += " size=\"135%\" weight=\"600\"";
    if (has("quote")) attrs += " foreground=\"#6b7280\"";
    if (attrs.empty()) return escaped;
    return "<span" + attrs + ">" + escaped + "</span>";
}

static std::string rangeToPango(GtkTextBuffer* buf, const GtkTextIter* s, const GtkTextIter* e) {
    std::string out;
    for (auto& seg : segments(buf, s, e)) {
        gchar* esc = g_markup_escape_text(seg.text.c_str(), -1);
        out += pangoSpan(seg.tags, esc);
        g_free(esc);
    }
    return out;
}

std::string textToPango(GtkTextBuffer* buf) {
    GtkTextIter s, e;
    gtk_text_buffer_get_bounds(buf, &s, &e);
    return rangeToPango(buf, &s, &e);
}

TextPages::~TextPages() {
    for (auto* l : paras) g_object_unref(l);
}

void TextPages::draw(cairo_t* cr, int page) const {
    if (page < 0 || page >= count()) return;
    cairo_set_source_rgb(cr, 0.07, 0.08, 0.1);
    for (const auto& ln : pages[page]) {
        auto* layout = paras[ln.para];
        PangoLayoutLine* line = pango_layout_get_line_readonly(layout, ln.line);
        PangoRectangle logical;
        pango_layout_line_get_extents(line, nullptr, &logical);
        // radens x-position från layouten (centrering, indrag)
        PangoLayoutIter* it = pango_layout_get_iter(layout);
        for (int i = 0; i < ln.line; ++i) pango_layout_iter_next_line(it);
        PangoRectangle lr;
        pango_layout_iter_get_line_extents(it, nullptr, &lr);
        pango_layout_iter_free(it);
        cairo_move_to(cr, margin + GPOINTER_TO_INT(g_object_get_data(G_OBJECT(layout), "wenuy-indent")) + pango_units_to_double(lr.x), ln.y);
        pango_cairo_show_layout_line(cr, line);
    }
}

std::unique_ptr<TextPages> textPages(GtkTextBuffer* buf, double width, double height, double margin, const char* font) {
    auto tp = std::make_unique<TextPages>();
    tp->width = width; tp->height = height; tp->margin = margin;
    auto* fm = pango_cairo_font_map_get_default();
    auto* ctx = pango_font_map_create_context(fm);
    pango_cairo_context_set_resolution(ctx, 72);   // 1 enhet = 1 punkt
    auto* fd = pango_font_description_from_string(font);
    const double textW = width - 2 * margin, textH = height - 2 * margin;

    std::vector<double> spaceAbove;
    const int lines = gtk_text_buffer_get_line_count(buf);
    for (int i = 0; i < lines; ++i) {
        GtkTextIter s, e;
        gtk_text_buffer_get_iter_at_line(buf, &s, i);
        e = s;
        if (!gtk_text_iter_ends_line(&e)) gtk_text_iter_forward_to_line_end(&e);
        auto tags = namedTags(&s);
        auto has = [&](const char* t) { for (auto& x : tags) if (x == t) return true; return false; };
        auto* l = pango_layout_new(ctx);
        pango_layout_set_font_description(l, fd);
        const double indent = has("quote") ? 28 : 0;
        pango_layout_set_width(l, pango_units_from_double(textW - indent));
        pango_layout_set_wrap(l, PANGO_WRAP_WORD_CHAR);
        pango_layout_set_alignment(l, has("center") ? PANGO_ALIGN_CENTER : PANGO_ALIGN_LEFT);
        pango_layout_set_indent(l, 0);
        std::string m = rangeToPango(buf, &s, &e);
        pango_layout_set_markup(l, m.empty() ? " " : m.c_str(), -1);
        tp->paras.push_back(l);
        spaceAbove.push_back(has("h1") ? 14 : has("h2") ? 10 : 4);
        g_object_set_data(G_OBJECT(l), "wenuy-indent", GINT_TO_POINTER((int)indent));
    }
    // fördela rader på sidor
    double y = 0;
    tp->pages.emplace_back();
    for (size_t p = 0; p < tp->paras.size(); ++p) {
        if (y > 0) y += spaceAbove[p];
        PangoLayoutIter* it = pango_layout_get_iter(tp->paras[p]);
        int li = 0;
        do {
            PangoRectangle lr;
            pango_layout_iter_get_line_extents(it, nullptr, &lr);
            const double h = pango_units_to_double(lr.height);
            const double base = pango_units_to_double(pango_layout_iter_get_baseline(it) - lr.y);
            if (y + h > textH && !tp->pages.back().empty()) { tp->pages.emplace_back(); y = 0; }
            tp->pages.back().push_back({(int)p, li, margin + y + base});
            y += h;
            ++li;
        } while (pango_layout_iter_next_line(it));
        pango_layout_iter_free(it);
    }
    pango_font_description_free(fd);
    g_object_unref(ctx);
    return tp;
}

}  // namespace wenuy

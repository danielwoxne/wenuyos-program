#include "Uppl.h"

#include <gio/gio.h>
#include <glib.h>

#include <algorithm>
#include <functional>
#include <regex>
#include <string>
#include <utility>
#include <vector>

namespace matte {

namespace {
GSubprocess* pagaende = nullptr;

// Uppläsning med markering: rösten syntetiseras först till minnet, så att vi vet hur lång den är,
// och spelas sedan upp medan ett ord i taget markeras.
struct Markering {
    unsigned gen = 0;                     // ökar vid varje ny uppläsning – gamla svar och tidtagare struntar i sig
    GSubprocess* synt = nullptr;
    std::vector<guint> tidtagare;
    std::function<void(int)> vidOrd;
    std::vector<double> vikter;           // ungefär hur lång tid varje ord tar (stavelser + paus)
    bool wav = true;                      // espeak ger WAV, piper rå 16-bitars 22 050 Hz
};
Markering mk;

int stavelser(const std::string& ord) {
    int n = 0;
    bool forraVokal = false;
    for (const char* p = ord.c_str(); *p; p = g_utf8_next_char(p)) {
        gunichar c = g_unichar_tolower(g_utf8_get_char(p));
        bool v = c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y' || c == 0xe5 || c == 0xe4 || c == 0xf6 || c == 0xe9;
        if (v && !forraVokal) ++n;
        forraVokal = v;
    }
    return std::max(1, n);
}

void stoppaMarkering() {
    ++mk.gen;
    for (guint t : mk.tidtagare) g_source_remove(t);
    mk.tidtagare.clear();
    if (mk.synt) { g_subprocess_force_exit(mk.synt); g_object_unref(mk.synt); mk.synt = nullptr; }
    if (mk.vidOrd) { auto f = std::move(mk.vidOrd); mk.vidOrd = nullptr; f(-1); }
}

// Markera ord för ord under `sekunder` sekunder, fördelat efter vikterna
void schemalagg(double sekunder) {
    double summa = 0;
    for (double v : mk.vikter) summa += v;
    if (summa <= 0) return;
    double t = 0;
    unsigned gen = mk.gen;
    struct Tid { unsigned gen; int ord; guint id; };
    for (size_t i = 0; i <= mk.vikter.size(); ++i) {
        int ms = (int)(t / summa * sekunder * 1000);
        auto* d = new Tid{gen, i < mk.vikter.size() ? (int)i : -1, 0};
        d->id = g_timeout_add_full(G_PRIORITY_DEFAULT, std::max(ms, 0), +[](gpointer p) -> gboolean {
            auto* d = static_cast<Tid*>(p);
            auto& lista = mk.tidtagare;   // den här tidtagaren är klar – ta bort den ur listan
            lista.erase(std::remove(lista.begin(), lista.end(), d->id), lista.end());
            if (d->gen == mk.gen && mk.vidOrd) {
                if (d->ord < 0) { auto f = std::move(mk.vidOrd); mk.vidOrd = nullptr; f(-1); }
                else mk.vidOrd(d->ord);
            }
            return G_SOURCE_REMOVE;
        }, d, +[](gpointer p) { delete static_cast<Tid*>(p); });
        mk.tidtagare.push_back(d->id);
        if (i < mk.vikter.size()) t += mk.vikter[i];
    }
}

std::string ersatt(const std::string& s, const std::string& fran, const std::string& till) {
    std::string r;
    size_t a = 0, b;
    while ((b = s.find(fran, a)) != std::string::npos) { r += s.substr(a, b - a) + till; a = b + fran.size(); }
    return r + s.substr(a);
}

std::string piperModell() {
    const std::string mappar[] = {std::string(g_get_user_data_dir()) + "/piper", "/usr/share/piper-voices", "/usr/local/share/piper"};
    for (auto& m : mappar) {
        GDir* d = g_dir_open(m.c_str(), 0, nullptr);
        if (!d) continue;
        std::string hittad;
        while (const char* f = g_dir_read_name(d)) {
            std::string n = f;
            if (n.rfind("sv_SE-", 0) == 0 && n.size() > 5 && n.compare(n.size() - 5, 5, ".onnx") == 0) { hittad = m + "/" + n; break; }
        }
        g_dir_close(d);
        if (!hittad.empty()) return hittad;
    }
    return "";
}
}  // namespace

std::string somOrd(const std::string& text) {
    std::string t = text;
    t = std::regex_replace(t, std::regex(R"((\d)\s*/\s*(\d))"), "$1 delat med $2");
    for (auto& [f, e] : std::vector<std::pair<std::string, std::string>>{
             {"·", " gånger "}, {"×", " gånger "}, {"*", " gånger "}, {"−", " minus "}, {" - ", " minus "}, {"+", " plus "},
             {"≠", " är inte lika med "}, {"=", " är lika med "}, {"%", " procent"}, {"≈", " ungefär "}, {"√", " roten ur "},
             {"²", " i kvadrat"}, {"³", " upphöjt till tre"}, {"^", " upphöjt till "}, {"<", " är mindre än "},
             {">", " är större än "}, {"▢", " vad "}})
        t = ersatt(t, f, e);
    // Ett ensamt ? är en lucka ("3 + ? = 10") – frågetecknet efter en fråga ska inte läsas
    t = std::regex_replace(t, std::regex(R"((^|\s)\?(?=\s|$))"), "$1vad ");
    t = std::regex_replace(t, std::regex(R"((\d)\s*:\s*(\d))"), "$1 delat med $2");
    t = std::regex_replace(t, std::regex(R"(\s+)"), " ");
    return t;
}

void tyst() {
    stoppaMarkering();
    if (pagaende) {
        g_subprocess_force_exit(pagaende);
        g_object_unref(pagaende);
        pagaende = nullptr;
    }
}

int TALTAKT = 140;

bool las(const std::string& text) {
    tyst();
    std::string ord = somOrd(text);
    if (ord.empty()) return false;
    gchar* piper = g_find_program_in_path("piper");
    gchar* aplay = g_find_program_in_path("aplay");
    std::string modell = piper && aplay ? piperModell() : "";
    std::vector<std::string> argv;
    if (!modell.empty()) {
        // piper | aplay via sh, texten på stdin – inga citattecken i kommandoraden
        std::string kmd = std::string("'") + piper + "' --model '" + modell + "' --output-raw 2>/dev/null | aplay -q -r 22050 -f S16_LE -t raw - 2>/dev/null";
        argv = {"sh", "-c", kmd};
    } else {
        gchar* es = g_find_program_in_path("espeak-ng");
        if (!es) es = g_find_program_in_path("espeak");
        if (es) argv = {es, "-v", "sv", "-s", std::to_string(TALTAKT), "--stdin"};
        g_free(es);
    }
    g_free(piper); g_free(aplay);
    if (argv.empty()) return false;
    std::vector<const char*> a;
    for (auto& s : argv) a.push_back(s.c_str());
    a.push_back(nullptr);
    pagaende = g_subprocess_newv(a.data(), (GSubprocessFlags)(G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE), nullptr);
    if (!pagaende) return false;
    GBytes* b = g_bytes_new(ord.data(), ord.size());
    g_subprocess_communicate_async(pagaende, b, nullptr, nullptr, nullptr);
    g_bytes_unref(b);
    return true;
}

std::string rostNamn() {
    gchar* piper = g_find_program_in_path("piper");
    gchar* es = g_find_program_in_path("espeak-ng");
    if (!es) es = g_find_program_in_path("espeak");
    std::string modell = piper ? piperModell() : "", r;
    if (!modell.empty()) { gchar* n = g_path_get_basename(modell.c_str()); r = std::string("Piper ") + n; g_free(n); }
    else if (es) r = "espeak-ng";
    g_free(piper); g_free(es);
    return r;
}

bool lasOrd(const std::vector<std::string>& ord, std::function<void(int)> vidOrd) {
    tyst();
    std::string text;
    mk.vikter.clear();
    for (auto& o : ord) {
        std::string s = somOrd(o);
        text += (text.empty() ? "" : " ") + s;
        double v = s.find_first_not_of(' ') == std::string::npos ? 0.3 : 0;
        for (const char* p = s.c_str(); *p;) {   // "3/4" blir flera talade ord
            while (*p == ' ') ++p;
            const char* b = p;
            while (*p && *p != ' ') ++p;
            if (p > b) v += stavelser(std::string(b, p)) + 0.4;
        }
        char sist = o.empty() ? ' ' : o.back();
        if (sist == '.' || sist == '?' || sist == '!' || sist == ':') v += 2.0;   // paus efter en mening
        else if (sist == ',') v += 1.0;
        mk.vikter.push_back(v);
    }
    if (text.empty()) return false;
    gchar* piper = g_find_program_in_path("piper");
    gchar* aplay = g_find_program_in_path("aplay");
    gchar* es = g_find_program_in_path("espeak-ng");
    if (!es) es = g_find_program_in_path("espeak");
    std::string modell = piper ? piperModell() : "";
    std::vector<std::string> synt;
    if (piper && !modell.empty()) { synt = {piper, "--model", modell, "--output-raw"}; mk.wav = false; }
    else if (es) { synt = {es, "-v", "sv", "-s", std::to_string(TALTAKT), "--stdout"}; mk.wav = true; }
    bool harSpelare = aplay != nullptr;
    std::string spelare = aplay ? aplay : "";
    g_free(piper); g_free(aplay); g_free(es);
    if (synt.empty()) return false;
    mk.vidOrd = std::move(vidOrd);
    if (!harSpelare) {   // ingen aplay: rösten spelar själv, markeringen går på uppskattad takt
        if (!las(text)) { mk.vidOrd = nullptr; return false; }
        double stav = 0;
        for (double v : mk.vikter) stav += v;
        schemalagg(stav * 60.0 / (TALTAKT * 1.55));
        return true;
    }
    std::vector<const char*> a;
    for (auto& x : synt) a.push_back(x.c_str());
    a.push_back(nullptr);
    mk.synt = g_subprocess_newv(a.data(), (GSubprocessFlags)(G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                                            G_SUBPROCESS_FLAGS_STDERR_SILENCE), nullptr);
    if (!mk.synt) { mk.vidOrd = nullptr; return false; }
    GBytes* in = g_bytes_new(text.data(), text.size());
    auto* sp = new std::pair<unsigned, std::string>(mk.gen, spelare);
    g_subprocess_communicate_async(mk.synt, in, nullptr, +[](GObject* o, GAsyncResult* r, gpointer p) {
        auto* d = static_cast<std::pair<unsigned, std::string>*>(p);
        GBytes* ut = nullptr;
        gboolean ok = g_subprocess_communicate_finish(G_SUBPROCESS(o), r, &ut, nullptr, nullptr);
        if (ok && ut && d->first == mk.gen) {
            if (mk.synt) { g_object_unref(mk.synt); mk.synt = nullptr; }
            gsize n = 0;
            const guint8* b = static_cast<const guint8*>(g_bytes_get_data(ut, &n));
            double sekunder = 0;
            if (mk.wav && n > 44) {
                guint32 perSek = b[28] | (b[29] << 8) | (b[30] << 16) | ((guint32)b[31] << 24);
                if (perSek > 0) sekunder = (double)(n - 44) / perSek;
            } else if (!mk.wav) {
                sekunder = (double)n / (22050 * 2);
            }
            std::vector<std::string> argv = {d->second, "-q"};
            if (!mk.wav) for (const char* x : {"-r", "22050", "-f", "S16_LE", "-t", "raw", "-c", "1"}) argv.push_back(x);
            argv.push_back("-");
            std::vector<const char*> a;
            for (auto& x : argv) a.push_back(x.c_str());
            a.push_back(nullptr);
            pagaende = g_subprocess_newv(a.data(), (GSubprocessFlags)(G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
                                                                     G_SUBPROCESS_FLAGS_STDERR_SILENCE), nullptr);
            if (pagaende && sekunder > 0) {
                g_subprocess_communicate_async(pagaende, ut, nullptr, nullptr, nullptr);
                schemalagg(sekunder * 0.97);   // rösten slutar med lite tystnad
            } else if (mk.vidOrd) {
                auto f = std::move(mk.vidOrd); mk.vidOrd = nullptr; f(-1);
            }
        }
        if (ut) g_bytes_unref(ut);
        delete d;
    }, sp);
    g_bytes_unref(in);
    return true;
}

}  // namespace matte

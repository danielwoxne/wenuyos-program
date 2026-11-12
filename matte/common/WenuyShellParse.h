// Rena hjälpfunktioner för Start och notisloggen (0.4.0) – utan GTK, så att de kan provas (tools/panel/test).
#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <cctype>
#include <cstdlib>

namespace wenuy {

// Ett öppet fönster som skalet lämnar till Start: "id\tapp-id\ttitel" per rad
struct OpenWindow { std::string id, appId, title; };
inline std::vector<OpenWindow> parseWindowLines(const std::string& text) {
    std::vector<OpenWindow> r;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        const std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        const size_t a = line.find('\t');
        if (a == std::string::npos || a == 0) continue;
        const size_t b = line.find('\t', a + 1);
        if (b == std::string::npos) continue;
        r.push_back({line.substr(0, a), line.substr(a + 1, b - a - 1), line.substr(b + 1)});
    }
    return r;
}
// Rad i fältet: titel med tabbar/radbrytningar ersatta (de skulle dela upp raden)
inline std::string windowLine(const std::string& id, const std::string& appId, std::string title) {
    if (title.empty()) title = appId;
    for (auto& c : title) if (c == '\t' || c == '\n') c = ' ';
    return id + "\t" + appId + "\t" + title + "\n";
}
// Notisens slag i loggen: Varning (brådskande), System (WenuyOS egna), annars Notis
enum class NoteKind { Warning, System, Notice };
inline NoteKind noteKind(bool urgent, bool local) { return urgent ? NoteKind::Warning : local ? NoteKind::System : NoteKind::Notice; }

// Kommandohistoriken: en rad per kommando, nyast först
inline std::vector<std::string> parseHistory(const std::string& text) {
    std::vector<std::string> r;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        std::string l = text.substr(pos, end - pos);
        pos = end + 1;
        if (!l.empty()) r.push_back(l);
    }
    return r;
}
inline std::vector<std::string> addToHistory(std::vector<std::string> h, const std::string& cmd, size_t max) {
    if (cmd.empty() || cmd.find('\n') != std::string::npos) return h;
    for (auto it = h.begin(); it != h.end();) it = *it == cmd ? h.erase(it) : it + 1;
    h.insert(h.begin(), cmd);
    if (h.size() > max) h.resize(max);
    return h;
}
inline std::string historyText(const std::vector<std::string>& h) {
    std::string t;
    for (auto& c : h) t += c + "\n";
    return t;
}
// Förslag: tidigare kommandon som börjar med det man skrivit (men inte är exakt samma)
inline std::vector<std::string> historyMatches(const std::vector<std::string>& h, const std::string& q, size_t max) {
    std::vector<std::string> r;
    for (auto& c : h)
        if (c != q && c.rfind(q, 0) == 0 && r.size() < max) r.push_back(c);
    return r;
}

// ---- Ofta använda (0.4.0): hur ofta och hur nyligen något öppnats från Start – bara på datorn.
// Fil: en rad per sak, "nyckel<TAB>antal<TAB>senast (unix-sekunder)". Nyckel: "app:<desktop-id>".
struct Use { int count = 0; long last = 0; };
inline std::map<std::string, Use> parseUsage(const std::string& text) {
    std::map<std::string, Use> m;
    for (auto& l : parseHistory(text)) {
        const size_t a = l.find('\t'), b = a == std::string::npos ? a : l.find('\t', a + 1);
        if (b == std::string::npos || a == 0) continue;
        m[l.substr(0, a)] = {std::atoi(l.c_str() + a + 1), std::atol(l.c_str() + b + 1)};
    }
    return m;
}
inline std::string usageText(const std::map<std::string, Use>& m) {
    std::string t;
    for (auto& [k, u] : m) t += k + "\t" + std::to_string(u.count) + "\t" + std::to_string(u.last) + "\n";
    return t;
}
// Poäng: antal gånger, viktat efter hur länge sedan (idag 100 %, veckan 70 %, månaden 40 %, äldre 20 %)
inline int frecency(const Use& u, long now) {
    const long age = now - u.last;
    const int w = age < 86400 ? 10 : age < 7 * 86400 ? 7 : age < 30 * 86400 ? 4 : 2;
    return u.count * w;
}
inline void recordUse(std::map<std::string, Use>& m, const std::string& key, long now, size_t max = 200) {
    if (key.empty() || key.find_first_of("\t\n") != std::string::npos) return;
    auto& u = m[key];
    u.count = std::min(u.count + 1, 1000);
    u.last = now;
    while (m.size() > max) {   // glöm det med lägst poäng
        auto worst = m.begin();
        for (auto it = m.begin(); it != m.end(); ++it) if (frecency(it->second, now) < frecency(worst->second, now)) worst = it;
        if (worst->first == key) break;
        m.erase(worst);
    }
}
// Det oftast använda först (högst max stycken)
inline std::vector<std::string> topUsed(const std::map<std::string, Use>& m, long now, size_t max) {
    std::vector<std::pair<int, std::string>> v;
    for (auto& [k, u] : m) v.push_back({frecency(u, now), k});
    std::stable_sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.first > b.first; });
    std::vector<std::string> r;
    for (auto& [s, k] : v) { if (r.size() >= max) break; r.push_back(k); }
    return r;
}
// Vad man valde för en viss sökning ("fi" → Firefox): "sökning<TAB>nyckel" per rad
inline std::map<std::string, std::string> parseChoices(const std::string& text) {
    std::map<std::string, std::string> m;
    for (auto& l : parseHistory(text)) {
        const size_t a = l.find('\t');
        if (a != std::string::npos && a > 0) m[l.substr(0, a)] = l.substr(a + 1);
    }
    return m;
}
inline std::string choicesText(const std::map<std::string, std::string>& m) {
    std::string t;
    for (auto& [q, k] : m) t += q + "\t" + k + "\n";
    return t;
}

// ---- Fästa program: flytta (eller lägg till) ett program till plats idx (dra och släpp i bottenraden)
inline std::vector<std::string> movePin(std::vector<std::string> v, const std::string& id, size_t idx) {
    if (id.empty()) return v;
    for (size_t i = 0; i < v.size(); ++i)
        if (v[i] == id) { v.erase(v.begin() + i); if (i < idx && idx > 0) --idx; break; }
    if (idx > v.size()) idx = v.size();
    v.insert(v.begin() + idx, id);
    return v;
}

// ---- Webbsidor ur Firefox historik/bokmärken: "adress<TAB>titel<TAB>poäng<TAB>bokmärke" (wenuyos-webb --export-history)
struct Page { std::string url, title; long score = 0; bool bookmark = false; };
inline std::string hostOf(const std::string& url) {
    size_t b = url.find("://");
    b = b == std::string::npos ? 0 : b + 3;
    std::string h = url.substr(b, url.find_first_of("/?#", b) - b);
    if (h.rfind("www.", 0) == 0) h = h.substr(4);
    return h;
}
inline std::vector<Page> parsePages(const std::string& text) {
    std::vector<Page> r;
    for (auto& l : parseHistory(text)) {
        const size_t a = l.find('\t'), b = a == std::string::npos ? a : l.find('\t', a + 1),
                     c = b == std::string::npos ? b : l.find('\t', b + 1);
        if (c == std::string::npos) continue;
        r.push_back({l.substr(0, a), l.substr(a + 1, b - a - 1), std::atol(l.c_str() + b + 1), l.substr(c + 1) == "1"});
    }
    return r;
}
// Förslag för det man skrivit (gemener): värden som börjar adressen först, sedan träffar i adress/titel; en per värd
inline std::vector<Page> matchPages(const std::vector<Page>& pages, const std::string& lq, size_t max) {
    std::vector<std::pair<long, const Page*>> hits;
    std::map<std::string, bool> seen;
    for (auto& p : pages) {
        std::string host = hostOf(p.url), t = p.title;
        for (auto& ch : t) ch = (char)std::tolower((unsigned char)ch);
        long s = 0;
        if (host.rfind(lq, 0) == 0) s = 3000000;
        else if (p.url.find(lq) != std::string::npos) s = 1000000;
        else if (t.find(lq) != std::string::npos) s = 500000;
        if (!s) continue;
        if (seen.count(host)) continue;
        seen[host] = true;
        hits.push_back({s + std::min(p.score, 499999L) + (p.bookmark ? 200000 : 0), &p});
    }
    std::stable_sort(hits.begin(), hits.end(), [](auto& a, auto& b) { return a.first > b.first; });
    std::vector<Page> r;
    for (auto& [sc, p] : hits) { if (r.size() >= max) break; r.push_back(*p); }
    return r;
}

// ---- Urklippshistoriken (wenuyos-clip): "tid<TAB>text" per rad, \n \t \\ skrivna med omvänt snedstreck
struct Clip { long when = 0; std::string text; };
inline std::string unescapeClip(const std::string& t) {
    std::string o;
    for (size_t i = 0; i < t.size(); ++i) {
        if (t[i] == '\\' && i + 1 < t.size()) {
            const char n = t[++i];
            o += n == 'n' ? '\n' : n == 't' ? '\t' : n;
        } else o += t[i];
    }
    return o;
}
inline std::vector<Clip> parseClips(const std::string& text) {
    std::vector<Clip> r;
    for (auto& l : parseHistory(text)) {
        const size_t a = l.find('\t');
        if (a == std::string::npos || a == 0) continue;
        r.push_back({std::atol(l.c_str()), unescapeClip(l.substr(a + 1))});
    }
    return r;
}
// Förhandsvisning: första raden, högst n tecken (UTF-8-säkert: klipper inte mitt i ett tecken)
inline std::string clipPreview(const std::string& t, size_t n) {
    std::string first = t.substr(0, t.find('\n'));
    if (first.size() <= n) return first;
    size_t cut = n;
    while (cut > 0 && (static_cast<unsigned char>(first[cut]) & 0xC0) == 0x80) --cut;
    return first.substr(0, cut) + "…";
}

// ---- Tab-komplettering i sökfältet (kommandoläget)
inline std::string commonPrefix(const std::vector<std::string>& v) {
    if (v.empty()) return "";
    std::string p = v[0];
    for (auto& s : v) {
        size_t i = 0;
        while (i < p.size() && i < s.size() && p[i] == s[i]) ++i;
        p.resize(i);
    }
    return p;
}
// Vad Tab gör med raden: det sista ordet kompletteras (första ordet = kommandon, annars filer/mappar)
// cands = alla namn som börjar med det sista ordet. Returnerar den nya raden (oförändrad om inget finns).
inline std::string completeLine(const std::string& line, const std::vector<std::string>& cands, bool* unique = nullptr) {
    if (unique) *unique = cands.size() == 1;
    if (cands.empty()) return line;
    const size_t sp = line.find_last_of(' ');
    const std::string head = sp == std::string::npos ? "" : line.substr(0, sp + 1);
    std::string done = commonPrefix(cands);
    if (cands.size() == 1 && !done.empty() && done.back() != '/') done += ' ';
    return head + done;
}

// ---- Inställningar direkt från sökfältet (0.4.0): "volym 40", "wifi av", "mörkt läge", "skärmbild" … (sv och en)
enum class Quick { None, Volume, Mute, Unmute, WifiOn, WifiOff, BtOn, BtOff, Dark, Light, Screenshot, DndOn, DndOff,
                   Brightness, NightOn, NightOff, AirOn, AirOff, Lock, Logout, Restart, Poweroff, Sleep };
struct QuickCmd { Quick kind = Quick::None; int value = -1; };
inline QuickCmd parseQuick(std::string q) {
    for (auto& c : q) c = (char)std::tolower((unsigned char)c);   // (å/ä/ö skrivs redan med gemener i UTF-8)
    while (!q.empty() && q.back() == ' ') q.pop_back();
    while (!q.empty() && q.front() == ' ') q.erase(0, 1);
    std::string t;   // ett mellanslag mellan orden
    for (char c : q) if (!(c == ' ' && !t.empty() && t.back() == ' ')) t += c;
    auto is = [&](std::initializer_list<const char*> l) { for (auto* x : l) if (t == x) return true; return false; };
    // "<ord> <tal>" (tal 0–100, ev. %)
    auto num = [&](std::initializer_list<const char*> words) -> int {
        for (auto* w : words) {
            const std::string p = std::string(w) + " ";
            if (t.rfind(p, 0) != 0) continue;
            std::string n = t.substr(p.size());
            if (!n.empty() && n.back() == '%') n.pop_back();
            while (!n.empty() && n.back() == ' ') n.pop_back();
            if (n.empty() || n.size() > 3 || n.find_first_not_of("0123456789") != std::string::npos) return -1;
            const int v = std::atoi(n.c_str());
            return v <= 100 ? v : -1;
        }
        return -1;
    };
    int v;
    if ((v = num({"volym", "volume", "ljud", "vol"})) >= 0) return {Quick::Volume, v};
    if ((v = num({"ljusstyrka", "brightness", "ljus"})) >= 0) return {Quick::Brightness, v};
    if (is({"ljud av", "tyst", "mute", "sound off", "stäng av ljudet"})) return {Quick::Mute};
    if (is({"ljud på", "unmute", "sound on", "slå på ljudet"})) return {Quick::Unmute};
    if (is({"wifi på", "wi-fi på", "wifi on", "wi-fi on", "trådlöst på"})) return {Quick::WifiOn};
    if (is({"wifi av", "wi-fi av", "wifi off", "wi-fi off", "trådlöst av"})) return {Quick::WifiOff};
    if (is({"bluetooth på", "bluetooth on"})) return {Quick::BtOn};
    if (is({"bluetooth av", "bluetooth off"})) return {Quick::BtOff};
    if (is({"mörkt", "mörkt läge", "mörkt tema", "dark", "dark mode", "dark theme"})) return {Quick::Dark};
    if (is({"ljust", "ljust läge", "ljust tema", "light", "light mode", "light theme"})) return {Quick::Light};
    if (is({"skärmbild", "skärmdump", "screenshot", "ta en skärmbild", "take a screenshot"})) return {Quick::Screenshot};
    if (is({"stör ej", "stör ej på", "do not disturb", "do not disturb on", "dnd", "dnd on"})) return {Quick::DndOn};
    if (is({"stör ej av", "do not disturb off", "dnd off"})) return {Quick::DndOff};
    if (is({"nattljus", "nattljus på", "night light", "night light on"})) return {Quick::NightOn};
    if (is({"nattljus av", "night light off"})) return {Quick::NightOff};
    if (is({"flygplansläge", "flygplansläge på", "airplane mode", "airplane mode on", "flight mode"})) return {Quick::AirOn};
    if (is({"flygplansläge av", "airplane mode off", "flight mode off"})) return {Quick::AirOff};
    if (is({"lås", "lås skärmen", "lås datorn", "lock", "lock screen"})) return {Quick::Lock};
    if (is({"logga ut", "log out", "logout", "sign out"})) return {Quick::Logout};
    if (is({"starta om", "starta om datorn", "restart", "reboot"})) return {Quick::Restart};
    if (is({"stäng av", "stäng av datorn", "shut down", "shutdown", "power off", "poweroff"})) return {Quick::Poweroff};
    if (is({"viloläge", "vila", "sleep", "suspend"})) return {Quick::Sleep};
    return {};
}

// ---- dra och släpp i mini-terminalen (0.4.0) ----
// En sökväg som skalet läser som ett ord: '…' med ' skrivet som '\'' (som g_shell_quote).
inline std::string shellQuote(const std::string& s) {
    std::string out = "'";
    for (char c : s) out += c == '\'' ? std::string("'\\''") : std::string(1, c);
    return out + "'";
}
// Släppta filer → text att skriva in: sökvägarna med citattecken, mellanslag emellan och efter.
inline std::string dropText(const std::vector<std::string>& paths) {
    std::string t;
    for (const auto& p : paths) { if (p.empty()) continue; t += shellQuote(p) + " "; }
    return t;
}
// Ctrl + släpp: gå till mappen – en mapp själv, en fil dess mapp. Tomt om det inte går (flera, inget).
inline std::string dropCdLine(const std::vector<std::string>& paths, const std::vector<bool>& isDir) {
    if (paths.size() != 1 || paths[0].empty() || paths[0][0] != '/') return {};
    std::string d = paths[0];
    if (isDir.empty() || !isDir[0]) {
        const size_t s = d.find_last_of('/');
        d = s == 0 ? "/" : d.substr(0, s);
    }
    return "cd " + shellQuote(d) + "\r";
}

// ---- "Skicka till felrapport…" i terminalerna ----
// de sista n raderna (utan tomma rader i slutet)
inline std::string lastLines(const std::string& s, int n) {
    size_t end = s.find_last_not_of(" \n\r\t");
    if (end == std::string::npos) return {};
    size_t pos = end + 1;
    int lines = 0;
    while (pos > 0) {
        size_t nl = s.rfind('\n', pos - 1);
        if (nl == std::string::npos) { pos = 0; break; }
        if (++lines >= n) { pos = nl + 1; break; }
        pos = nl;
    }
    return s.substr(pos, end + 1 - pos);
}


// ---- fästa filer i Start (0.4.0): ~/.config/wenuyos/start-files, en adress (file://…) per rad ----
// Lägg till sist (eller ta bort); samma fil två gånger blir en, högst max – den äldsta får gå.
inline std::vector<std::string> togglePinned(std::vector<std::string> v, const std::string& item, bool on, size_t max = 24) {
    v.erase(std::remove(v.begin(), v.end(), item), v.end());
    if (on && !item.empty()) {
        v.push_back(item);
        if (v.size() > max) v.erase(v.begin(), v.begin() + (v.size() - max));
    }
    return v;
}
// ---- AI i Start (0.4.0) ----
// Kommandon ur AI-svarets kodblock (```sh, ```bash, ```shell eller utan språk): en rad per kommando, högst 4,
// inga kommentarer eller tomma rader, inga rader längre än 200 tecken – de blir "Kör i terminalen"-knappar.
inline std::vector<std::string> aiCommands(const std::string& text) {
    std::vector<std::string> out;
    bool in = false, shell = false;
    size_t a = 0;
    while (a <= text.size() && out.size() < 4) {
        size_t b = text.find('\n', a);
        if (b == std::string::npos) b = text.size();
        std::string line = text.substr(a, b - a);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        size_t ind = line.find_first_not_of(" \t");
        std::string t = ind == std::string::npos ? "" : line.substr(ind);
        if (t.rfind("```", 0) == 0) {
            if (!in) { const std::string lang = t.substr(3); in = true; shell = lang.empty() || lang == "sh" || lang == "bash" || lang == "shell" || lang == "console"; }
            else in = false;
        } else if (in && shell && !t.empty() && t[0] != '#' && t.size() <= 200) {
            if (t.rfind("$ ", 0) == 0) t = t.substr(2);   // "$ kommando" som i en terminal
            if (std::find(out.begin(), out.end(), t) == out.end()) out.push_back(t);
        }
        if (b == text.size()) break;
        a = b + 1;
    }
    return out;
}
// Hur mycket av en byte-ström som är hela UTF-8-tecken (resten väntar på nästa bit)
inline size_t utf8Complete(const std::string& s) {
    size_t i = s.size(), back = 0;
    while (i > 0 && back < 4) {
        const unsigned char c = (unsigned char)s[i - 1];
        if ((c & 0xC0) != 0x80) {   // första byten i ett tecken
            const size_t need = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
            return back + 1 >= need ? s.size() : i - 1;
        }
        --i; ++back;
    }
    return s.size();
}
}  // namespace wenuy

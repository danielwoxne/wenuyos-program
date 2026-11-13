// libwenuy – grund för dokumentprogram (ordbehandlare, kalkyl, rit-, ljud- och videoprogram).
//
//   wenuy::History    ångra/gör om, med sammanslagning av snabba ändringar (t.ex. skrivning)
//   wenuy::Document   öppna/spara säkert, "osparade ändringar", autosparning och återställning
//   wenuy::atomicWrite()   skriv till temporär fil + byt namn – inget halvt sparat dokument
//   wenuy::bindDocumentActions()  Ctrl+Z/Ctrl+Shift+Z/Ctrl+S/Ctrl+Shift+S/Ctrl+P i ett fönster
//   wenuy::exportPdf() / wenuy::print()   PDF och utskrift med samma ritfunktion (cairo)
//
// Allt är händelsestyrt: autosparningen har en timer bara när det finns osparade ändringar.
#pragma once
#include <gtk/gtk.h>

#include <functional>
#include <string>
#include <vector>

namespace wenuy {

class History {
public:
    // Gör ändringen (redo körs direkt) och lägg den i historiken.
    // mergeKey: steg med samma nyckel inom 1 s slås ihop (t.ex. "typing") → ett ångra-steg.
    void perform(const std::string& label, std::function<void()> redo, std::function<void()> undo,
                 const std::string& mergeKey = {});
    // Lägg till en ändring som redan är gjord.
    void record(const std::string& label, std::function<void()> redo, std::function<void()> undo,
                const std::string& mergeKey = {});
    bool canUndo() const { return pos_ > 0; }
    bool canRedo() const { return pos_ < steps_.size(); }
    void undo();
    void redo();
    std::string undoLabel() const { return canUndo() ? steps_[pos_ - 1].label : std::string(); }
    std::string redoLabel() const { return canRedo() ? steps_[pos_].label : std::string(); }
    void clear();
    void markSaved() { saved_ = pos_; savedValid_ = true; notify(); }
    bool dirty() const { return !savedValid_ || saved_ != pos_; }
    void onChanged(std::function<void()> f) { listeners_.push_back(std::move(f)); }
    void setLimit(size_t n) { limit_ = n; }   // standard 500 steg (sparsamt med minne)

private:
    struct Step { std::string label, key; std::function<void()> redo, undo; gint64 time; };
    void notify() { for (auto& f : listeners_) f(); }
    std::vector<Step> steps_;
    size_t pos_ = 0, saved_ = 0, limit_ = 500;
    bool savedValid_ = true;
    std::vector<std::function<void()>> listeners_;
};

bool atomicWrite(const std::string& path, const std::string& data, std::string* error = nullptr);

struct Recovery { std::string file, originalPath, title; gint64 modified; };

class Document {
public:
    // appId: t.ex. "se.exempel.Skrivare" – används för återställningsmappen.
    explicit Document(std::string appId);
    ~Document();

    // Programmet fyller i dessa två:
    std::function<std::string()> serialize;                 // dokument → bytes
    std::function<bool(const std::string& data)> deserialize;   // bytes → dokument

    History history;

    bool open(const std::string& path, std::string* error = nullptr);
    bool save(std::string* error = nullptr);                // kräver path()
    bool saveAs(const std::string& path, std::string* error = nullptr);
    void newDocument();                                      // tomt, ingen sökväg
    const std::string& path() const { return path_; }
    std::string title() const;                               // filnamn eller "Untitled"/"Namnlös"
    bool dirty() const { return history.dirty(); }

    // Autosparning till ~/.local/state/wenuyos/recovery/<appId>/ medan det finns osparade
    // ändringar. Tas bort vid sparning/stängning. 0 = av.
    void setAutosave(guint seconds);
    static std::vector<Recovery> recoverable(const std::string& appId);
    bool restore(const Recovery& r, std::string* error = nullptr);
    void discardRecovery();

    void onChanged(std::function<void()> f) { history.onChanged(std::move(f)); }

private:
    void scheduleAutosave();
    void writeRecovery();
    std::string recoveryFile() const;
    std::string appId_, path_, id_;
    guint autosave_ = 30, timer_ = 0;
};

// Ctrl+Z, Ctrl+Shift+Z/Ctrl+Y, Ctrl+S, Ctrl+Shift+S, Ctrl+O, Ctrl+P som win.undo, win.redo …
// Callbacks för spara/öppna/skriv ut ges av programmet (kan vara nullptr).
// Fönstrets titel sätts till "• Namn" när det finns osparade ändringar.
// Stänger användaren med osparade ändringar visas en fråga (Spara / Spara inte / Avbryt).
void bindDocumentActions(GtkApplicationWindow* win, Document& doc, const std::string& appName,
                         std::function<void()> onSave, std::function<void()> onOpen,
                         std::function<void()> onPrint);

// En sida i PDF/utskrift. Rita i punkter (1/72 tum); A4 = 595 × 842.
using PageDrawer = std::function<void(cairo_t* cr, int page, double width, double height)>;
bool exportPdf(const std::string& path, int pages, const PageDrawer& draw,
               double width = 595.0, double height = 842.0, std::string* error = nullptr);
void print(GtkWindow* parent, int pages, PageDrawer draw);

}  // namespace wenuy

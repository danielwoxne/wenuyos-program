// WenuyOS: "Skicka till felrapport…" i Konsol och mini-terminalen (0.4.0).
// Det markerade – annars de senaste raderna – läggs i en fil som bara användaren kan läsa och lämnas till
// wenuyos-console-report, som tvättar texten, visar den (går att ändra) och skickar den till det privata
// GitHub-förrådet först när användaren trycker Skicka. Inget skickas av sig självt.
#pragma once
#include <fcntl.h>
#include <glib/gstdio.h>
#include <unistd.h>
#include <vte/vte.h>

#include <string>

#include "WenuyShellParse.h"

namespace wenuy {

constexpr int kReportLines = 200;

inline std::string terminalReportText(VteTerminal* t) {
    if (vte_terminal_get_has_selection(t)) {
        gchar* sel = vte_terminal_get_text_selected(t, VTE_FORMAT_TEXT);
        std::string r = sel ? sel : "";
        g_free(sel);
        if (!r.empty()) return r;
    }
    GOutputStream* out = g_memory_output_stream_new_resizable();
    std::string all;
    if (vte_terminal_write_contents_sync(t, out, VTE_WRITE_DEFAULT, nullptr, nullptr)) {
        g_output_stream_close(out, nullptr, nullptr);
        auto* m = G_MEMORY_OUTPUT_STREAM(out);
        all.assign(static_cast<const char*>(g_memory_output_stream_get_data(m)), g_memory_output_stream_get_data_size(m));
    }
    g_object_unref(out);
    return lastLines(all, kReportLines);
}

// startar granskningsfönstret; false om det inte gick (då händer inget)
inline bool terminalReport(VteTerminal* t) {
    const std::string text = terminalReportText(t);
    if (text.empty()) return false;
    gchar* path = g_build_filename(g_get_user_runtime_dir(), "wenuyos-console-report-XXXXXX", nullptr);
    const int fd = g_mkstemp_full(path, O_WRONLY, 0600);
    bool ok = fd >= 0 && write(fd, text.data(), text.size()) == (ssize_t)text.size();
    if (fd >= 0) close(fd);
    if (ok) {
        const gchar* argv[] = {"/usr/lib/wenuyos/wenuyos-console-report", path, nullptr};
        ok = g_spawn_async(nullptr, (gchar**)argv, nullptr, G_SPAWN_DEFAULT, nullptr, nullptr, nullptr, nullptr);
    }
    if (!ok) g_unlink(path);
    g_free(path);
    return ok;
}

}  // namespace wenuy

#include "WenuyWindow.h"

#include <cstring>

namespace {
const char* kName = "org.wenuyos.Window";
const char* kPath = "/org/wenuyos/Window";
const char* kIface = "org.wenuyos.Window1";

// Asynkront och "skjut och glöm": programmet ska aldrig vänta på fönsterhanteraren
void call(const char* method, GVariant* args) {
    GDBusConnection* bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
    if (!bus) { g_variant_unref(g_variant_ref_sink(args)); return; }
    g_dbus_connection_call(bus, kName, kPath, kIface, method, args, nullptr, G_DBUS_CALL_FLAGS_NO_AUTO_START, 3000,
                           nullptr, nullptr, nullptr);
    g_object_unref(bus);
}

const char* title(GtkWindow* w) {
    const char* t = w ? gtk_window_get_title(w) : nullptr;
    return t ? t : "";
}
}  // namespace

gboolean wenuy_window_api_available(void) {
    GDBusConnection* bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
    if (!bus) return FALSE;
    GVariant* r = g_dbus_connection_call_sync(bus, kName, kPath, kIface, "Available", nullptr, G_VARIANT_TYPE("(b)"),
                                              G_DBUS_CALL_FLAGS_NO_AUTO_START, 1000, nullptr, nullptr);
    g_object_unref(bus);
    gboolean ok = FALSE;
    if (r) { g_variant_get(r, "(b)", &ok); g_variant_unref(r); }
    return ok;
}

void wenuy_window_set_keep_above(GtkWindow* w, gboolean on) { call("SetKeepAbove", g_variant_new("(sb)", title(w), on)); }
void wenuy_window_set_sticky(GtkWindow* w, gboolean on) { call("SetSticky", g_variant_new("(sb)", title(w), on)); }
void wenuy_window_set_picture_in_picture(GtkWindow* w, gboolean on) { call("PictureInPicture", g_variant_new("(sb)", title(w), on)); }
void wenuy_window_tile(GtkWindow* w, const char* where) { call("Tile", g_variant_new("(ss)", title(w), where ? where : "max")); }
void wenuy_window_set_opacity(GtkWindow* w, double o) { call("SetOpacity", g_variant_new("(sd)", title(w), o)); }

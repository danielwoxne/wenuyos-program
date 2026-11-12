// libwenuy – tidslinje (grund för ljudverktyg, musikprogram och videoredigerare).
//
//   wenuy::Timeline        spår och klipp (start, längd, färg, etikett), uppspelningsmarkör
//   wenuy_timeline_new()   widget som ritar tidslinjen med grafikkortet:
//                          - dra klipp i sidled och mellan spår (fäster mot andra klipps kanter
//                            och markören), dra i högerkanten för att korta/förlänga
//                          - klicka i tidslinjalen för att flytta markören
//                          - Ctrl+hjul zoomar i tid, hjul rullar, Delete tar bort markerat klipp
//                          - valfri vågform/miniatyrer per klipp via en ritfunktion
//   Varje ändring går genom wenuy::History (ångra/gör om) om en sådan kopplats in.
#pragma once
#include <gtk/gtk.h>

#include <functional>
#include <string>
#include <vector>

#include "WenuyDoc.h"

namespace wenuy {

struct Clip {
    int id = 0;
    int track = 0;
    double start = 0, length = 1;   // sekunder
    double offset = 0;              // var i källan klippet börjar (för trimning)
    std::string label;
    GdkRGBA color = {0.24f, 0.47f, 0.96f, 1};
    std::string source;             // fil eller annan källa – programmets sak
};

struct Track {
    std::string name;
    double height = 64;
    bool muted = false;
};

class Timeline {
public:
    std::vector<Track> tracks;
    std::vector<Clip> clips;
    double playhead = 0;   // sekunder
    int selected = -1;     // klipp-id

    int addClip(Clip c);   // returnerar id
    Clip* find(int id);
    double duration() const;   // slutet av sista klippet
    std::string serialize() const;
    bool deserialize(const std::string& data);
private:
    int nextId_ = 1;
};

}  // namespace wenuy

G_BEGIN_DECLS

// Rita klippets innehåll (vågform, miniatyrer) i rutan x,y,w,h. seconds_per_px för detaljnivå.
typedef void (*WenuyClipDrawFunc)(GtkSnapshot* snapshot, const wenuy::Clip* clip, float x, float y,
                                  float w, float h, double seconds_per_px, gpointer data);

GtkWidget* wenuy_timeline_new(wenuy::Timeline* model);   // modellen ägs av programmet
void wenuy_timeline_set_history(GtkWidget* timeline, wenuy::History* history);
void wenuy_timeline_set_clip_draw_func(GtkWidget* timeline, WenuyClipDrawFunc fn, gpointer data);
void wenuy_timeline_set_zoom(GtkWidget* timeline, double px_per_second);   // standard 80
void wenuy_timeline_changed(GtkWidget* timeline);                          // rita om efter ändring
// Signaler: "playhead-moved" (double sekunder), "selection-changed" (int klipp-id, -1 = inget),
//           "clips-changed" ()

G_END_DECLS

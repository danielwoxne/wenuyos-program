// WenuyCanvas – rityta med zoom och panorering, ritas av grafikkortet (GtkSnapshot).
// Grund för ritprogram, sidlayout (ordbehandlare), kalkylblad, vågformer och tidslinjer.
//
//   Zooma:      Ctrl + scrollhjul, nyp på pekplatta/skärm, Ctrl + / Ctrl − / Ctrl 0 (anpassa)
//   Panorera:   scrollhjul/två fingrar, mittenknapp eller mellanslag + dra
//   Rita:       vänsterknapp/penna ger signalerna "press", "drag", "release" i INNEHÅLLETS
//               koordinater (oberoende av zoom), med tryck 0–1 från ritplatta/penna.
//
// Programmet ritar i innehållskoordinater i sin draw-funktion; zoom och flytt sköts här.
#pragma once
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define WENUY_TYPE_CANVAS (wenuy_canvas_get_type())
G_DECLARE_FINAL_TYPE(WenuyCanvas, wenuy_canvas, WENUY, CANVAS, GtkWidget)

// Rita innehållet. snapshot är redan förflyttat/skalat: rita i innehållskoordinater.
// visible = den del av innehållet som syns (rita bara den för stora dokument).
typedef void (*WenuyCanvasDrawFunc)(WenuyCanvas* canvas, GtkSnapshot* snapshot,
                                    const graphene_rect_t* visible, double zoom, gpointer data);

GtkWidget* wenuy_canvas_new(double content_width, double content_height);
void wenuy_canvas_set_content_size(WenuyCanvas* self, double width, double height);
void wenuy_canvas_set_draw_func(WenuyCanvas* self, WenuyCanvasDrawFunc fn, gpointer data, GDestroyNotify destroy);
// TRUE (standard): innehållet visas som ett vitt papper med skugga (ritprogram, sidor).
// FALSE: innehållet fyller ytan och hörnet (0,0) stannar uppe till vänster (kalkylblad, tidslinjer).
void wenuy_canvas_set_paper(WenuyCanvas* self, gboolean paper);
// Sidläge (PDF, dokument): programmet ritar egna sidor; ytan centreras, fyller bredden och börjar överst.
void wenuy_canvas_set_pages(WenuyCanvas* self, gboolean pages);
// Papperet (centrerat) men utan vit bakgrund och skugga – t.ex. bilder med genomskinlighet.
void wenuy_canvas_set_paper_background(WenuyCanvas* self, gboolean background);
void wenuy_canvas_set_zoom(WenuyCanvas* self, double zoom, gboolean animate);   // 1.0 = 100 %
double wenuy_canvas_get_zoom(WenuyCanvas* self);
void wenuy_canvas_zoom_to_fit(WenuyCanvas* self, gboolean animate);
void wenuy_canvas_set_zoom_limits(WenuyCanvas* self, double min, double max);    // standard 0.05–32
// Skärmkoordinater i ytan → innehållskoordinater
void wenuy_canvas_to_content(WenuyCanvas* self, double x, double y, double* cx, double* cy);
// Rita om (anropa när innehållet ändrats)
void wenuy_canvas_queue_redraw(WenuyCanvas* self);

// Signaler:  "press"   (double x, double y, double pressure, guint button)
//            "drag"    (double x, double y, double pressure)
//            "release" (double x, double y)
//            "zoom-changed" ()

G_END_DECLS

// Två frivilliga laborationsytor: tallinje med mus/tangentbord och geometri.
// GTK4/Cairo, ingen extern grafikmotor, inget sparande och ingen rättning.
#include "LarandeUtforskaGtk.h"
#include <Wenuy.h>
#include <algorithm>
#include <cmath>
#include <string>

using wenuy::tr;
namespace matte13 {
namespace {
struct LinjeVy {
    Tallinje modell;
    GtkWidget* bild = nullptr;
    GtkWidget* skala = nullptr;
    GtkWidget* resultat = nullptr;
};
struct GeoVy {
    GtkWidget* bild = nullptr;
    GtkWidget* bredd = nullptr;
    GtkWidget* hojd = nullptr;
    GtkWidget* form = nullptr;
    GtkWidget* resultat = nullptr;
};
void ritTallinje(GtkDrawingArea*, cairo_t* cr, int w, int h, gpointer ptr) {
    auto* v = static_cast<LinjeVy*>(ptr);
    if (!giltig(v->modell) || w < 140 || h < 75) return;
    const double x0 = 32.0, x1 = w - 32.0, y = h * 0.50;
    // Hela modellen ritas på samma ljusa bakgrund även i mörkt tema.
    cairo_set_source_rgb(cr, 0.975, 0.98, 1.0);
    cairo_paint(cr);
    cairo_set_source_rgb(cr, 0.18, 0.22, 0.33);
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, x0, y); cairo_line_to(cr, x1, y); cairo_stroke(cr);
    const int antal = (int)std::floor((v->modell.max-v->modell.min)/v->modell.steg+0.00001);
    const int etikettVar = std::max(1, (int)std::ceil((antal+1) * 35.0/(x1-x0)));
    for (int i=0;i<=antal;++i) {
        const double tal = v->modell.min + i*v->modell.steg;
        const double x = x0+(x1-x0)*andel(v->modell,tal);
        const bool stor = i%etikettVar==0 || i==antal;
        cairo_set_source_rgb(cr,0.30,0.33,0.42);
        cairo_set_line_width(cr,stor?1.8:1.0);
        cairo_move_to(cr,x,y-(stor?9:5)); cairo_line_to(cr,x,y+(stor?9:5)); cairo_stroke(cr);
        if (stor) {
            const std::string t=svTal(tal);
            cairo_set_font_size(cr,11);
            cairo_move_to(cr,x-std::min(16.0,4.0*t.size()),y+26);
            cairo_show_text(cr,t.c_str());
        }
    }
    const double x = x0+(x1-x0)*andel(v->modell,v->modell.markerat);
    cairo_set_source_rgb(cr,0.36,0.34,0.78);
    cairo_arc(cr,x,y,7.5,0,2*std::acos(-1.0)); cairo_fill(cr);
    cairo_set_source_rgb(cr,1,1,1);
    cairo_arc(cr,x,y,3.0,0,2*std::acos(-1.0)); cairo_fill(cr);
}
void uppdateraLinje(LinjeVy* v) {
    v->modell.markerat=snappa(v->modell,gtk_range_get_value(GTK_RANGE(v->skala)));
    std::string text=std::string(tr("Selected number: ","Valt tal: "))+svTal(v->modell.markerat);
    gtk_label_set_text(GTK_LABEL(v->resultat),text.c_str());
    gtk_widget_queue_draw(v->bild);
}
void ritGeometri(GtkDrawingArea*,cairo_t* cr,int w,int h,gpointer ptr) {
    auto* v=static_cast<GeoVy*>(ptr);
    Geometri m;
    m.bredd=gtk_spin_button_get_value(GTK_SPIN_BUTTON(v->bredd));
    m.hojd=gtk_spin_button_get_value(GTK_SPIN_BUTTON(v->hojd));
    m.form=gtk_drop_down_get_selected(GTK_DROP_DOWN(v->form))==1
        ?Geometriform::RatvinkligTriangel:Geometriform::Rektangel;
    if (!giltig(m) || w<150 || h<100) return;
    cairo_set_source_rgb(cr,0.975,0.98,1.0); cairo_paint(cr);
    const double scale=std::min((w-100.0)/m.bredd,(h-62.0)/m.hojd);
    const double ww=std::max(1.0,m.bredd*scale), hh=std::max(1.0,m.hojd*scale);
    const double x=(w-ww)*0.50, y=(h-hh)*0.50;
    cairo_new_path(cr);
    if (m.form==Geometriform::Rektangel) cairo_rectangle(cr,x,y,ww,hh);
    else {cairo_move_to(cr,x,y);cairo_line_to(cr,x,y+hh);cairo_line_to(cr,x+ww,y+hh);cairo_close_path(cr);}
    cairo_set_source_rgba(cr,0.38,0.37,0.77,0.24);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr,0.25,0.26,0.57);
    cairo_set_line_width(cr,3.0);cairo_stroke(cr);
    // Måttangivelser som inte förlitar sig på enbart färg.
    cairo_set_source_rgb(cr,0.20,0.23,0.34);
    cairo_set_font_size(cr,14);
    const std::string bs=svTal(m.bredd), hs=svTal(m.hojd);
    cairo_move_to(cr,x+ww/2-7,y+hh+20);cairo_show_text(cr,bs.c_str());
    cairo_move_to(cr,std::max(4.0,x-25),y+hh/2);cairo_show_text(cr,hs.c_str());
}
void uppdateraGeometri(GeoVy* v) {
    Geometri m;
    m.bredd=gtk_spin_button_get_value(GTK_SPIN_BUTTON(v->bredd));
    m.hojd=gtk_spin_button_get_value(GTK_SPIN_BUTTON(v->hojd));
    m.form=gtk_drop_down_get_selected(GTK_DROP_DOWN(v->form))==1
        ?Geometriform::RatvinkligTriangel:Geometriform::Rektangel;
    std::string text=std::string(tr("Area: ","Area: "))+svTal(area(m))+" "+tr("square units","rutenheter")+
       "  ·  "+tr("Perimeter: ","Omkrets: ")+svTal(omkrets(m))+" "+tr("units","längdenheter");
    if (m.form==Geometriform::RatvinkligTriangel)
        text+="\n"+std::string(tr("Hypotenuse computed using Pythagoras.","Hypotenusan beräknas med Pythagoras sats."));
    gtk_label_set_text(GTK_LABEL(v->resultat),text.c_str());
    gtk_widget_queue_draw(v->bild);
}
} // namespace

GtkWidget* skapaTallinjepanel() {
    GtkWidget* exp=gtk_expander_new(tr("Explore the number line","Utforska tallinjen"));
    gtk_widget_add_css_class(exp,"matte13-laboration");
    gtk_expander_set_expanded(GTK_EXPANDER(exp),FALSE);
    auto* v=new LinjeVy;
    g_object_set_data_full(G_OBJECT(exp),"matte13-linje",v,+[](gpointer p){delete static_cast<LinjeVy*>(p);});
    GtkWidget* box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    gtk_widget_set_margin_top(box,10);
    GtkWidget* info=gtk_label_new(tr("Tap the line, drag the slider or use the arrow keys. This does not change your answer.",
                                   "Tryck på linjen, dra reglaget eller använd piltangenterna. Ditt svar ändras inte."));
    gtk_label_set_wrap(GTK_LABEL(info),TRUE);gtk_label_set_xalign(GTK_LABEL(info),0);
    gtk_box_append(GTK_BOX(box),info);
    v->bild=gtk_drawing_area_new();
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(v->bild),115);
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(v->bild),300);
    gtk_widget_set_hexpand(v->bild,TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(v->bild),ritTallinje,v,nullptr);
    gtk_box_append(GTK_BOX(box),v->bild);
    GtkGesture* klick=gtk_gesture_click_new();
    g_signal_connect(klick,"pressed",G_CALLBACK(+[](GtkGestureClick*,int,double x,double,gpointer p){
        auto* v=static_cast<LinjeVy*>(p);
        int w=gtk_widget_get_width(v->bild);
        if(w<140 || !giltig(v->modell))return;
        double a=std::clamp((x-32.0)/(w-64.0),0.0,1.0);
        gtk_range_set_value(GTK_RANGE(v->skala),snappa(v->modell,v->modell.min+a*(v->modell.max-v->modell.min)));
    }),v);
    gtk_widget_add_controller(v->bild,GTK_EVENT_CONTROLLER(klick));
    v->skala=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0,20,1);
    gtk_scale_set_draw_value(GTK_SCALE(v->skala),FALSE);
    gtk_accessible_update_property(GTK_ACCESSIBLE(v->skala),GTK_ACCESSIBLE_PROPERTY_LABEL,
        tr("Select a number on the number line","Välj ett tal på tallinjen"),-1);
    gtk_box_append(GTK_BOX(box),v->skala);
    v->resultat=gtk_label_new("");
    gtk_widget_add_css_class(v->resultat,"matte13-labresultat");
    gtk_box_append(GTK_BOX(box),v->resultat);
    gtk_expander_set_child(GTK_EXPANDER(exp),box);
    g_signal_connect(v->skala,"value-changed",G_CALLBACK(+[](GtkRange*,gpointer p){uppdateraLinje(static_cast<LinjeVy*>(p));}),v);
    stallInTallinjepanel(exp,Tallinje{});
    return exp;
}
void stallInTallinjepanel(GtkWidget* panel,Tallinje m){
    if(!panel||!giltig(m))return;
    auto* v=static_cast<LinjeVy*>(g_object_get_data(G_OBJECT(panel),"matte13-linje"));
    if(!v)return;
    v->modell=m;
    gtk_range_set_range(GTK_RANGE(v->skala),m.min,m.max);
    gtk_range_set_increments(GTK_RANGE(v->skala),m.steg,m.steg);
    gtk_range_set_value(GTK_RANGE(v->skala),m.markerat);
    uppdateraLinje(v);
}
GtkWidget* skapaGeometripanel(){
    GtkWidget* exp=gtk_expander_new(tr("Explore geometry","Utforska geometri"));
    gtk_widget_add_css_class(exp,"matte13-laboration");
    gtk_expander_set_expanded(GTK_EXPANDER(exp),FALSE);
    auto* v=new GeoVy;
    g_object_set_data_full(G_OBJECT(exp),"matte13-geo",v,+[](gpointer p){delete static_cast<GeoVy*>(p);});
    GtkWidget* box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
    gtk_widget_set_margin_top(box,10);
    GtkWidget* info=gtk_label_new(tr("Change the dimensions to see area and perimeter. This is a separate example.",
                                   "Ändra måtten och undersök area och omkrets. Det är ett separat exempel."));
    gtk_label_set_wrap(GTK_LABEL(info),TRUE);gtk_label_set_xalign(GTK_LABEL(info),0);
    gtk_box_append(GTK_BOX(box),info);
    GtkWidget* grid=gtk_grid_new();gtk_grid_set_row_spacing(GTK_GRID(grid),8);gtk_grid_set_column_spacing(GTK_GRID(grid),12);
    GtkWidget* lblForm=gtk_label_new(tr("Shape","Form"));gtk_label_set_xalign(GTK_LABEL(lblForm),0);
    const char* forms[]={tr("Rectangle","Rektangel"),tr("Right triangle","Rätvinklig triangel"),nullptr};
    v->form=gtk_drop_down_new_from_strings(forms);
    gtk_accessible_update_property(GTK_ACCESSIBLE(v->form),GTK_ACCESSIBLE_PROPERTY_LABEL,tr("Shape","Form"),-1);
    gtk_grid_attach(GTK_GRID(grid),lblForm,0,0,1,1);
    gtk_grid_attach(GTK_GRID(grid),v->form,1,0,1,1);
    GtkWidget* lblB=gtk_label_new(tr("Width","Bredd")); gtk_label_set_xalign(GTK_LABEL(lblB),0);
    v->bredd=gtk_spin_button_new_with_range(1,12,1);
    gtk_accessible_update_property(GTK_ACCESSIBLE(v->bredd),GTK_ACCESSIBLE_PROPERTY_LABEL,tr("Width in units","Bredd i enheter"),-1);
    gtk_grid_attach(GTK_GRID(grid),lblB,0,1,1,1);
    gtk_grid_attach(GTK_GRID(grid),v->bredd,1,1,1,1);
    GtkWidget* lblH=gtk_label_new(tr("Height","Höjd"));gtk_label_set_xalign(GTK_LABEL(lblH),0);
    v->hojd=gtk_spin_button_new_with_range(1,12,1);
    gtk_accessible_update_property(GTK_ACCESSIBLE(v->hojd),GTK_ACCESSIBLE_PROPERTY_LABEL,tr("Height in units","Höjd i enheter"),-1);
    gtk_grid_attach(GTK_GRID(grid),lblH,0,2,1,1);
    gtk_grid_attach(GTK_GRID(grid),v->hojd,1,2,1,1);
    gtk_box_append(GTK_BOX(box),grid);
    v->bild=gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(v->bild),320);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(v->bild),210);
    gtk_widget_set_hexpand(v->bild,TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(v->bild),ritGeometri,v,nullptr);
    gtk_box_append(GTK_BOX(box),v->bild);
    v->resultat=gtk_label_new("");
    gtk_label_set_wrap(GTK_LABEL(v->resultat),TRUE);
    gtk_widget_add_css_class(v->resultat,"matte13-labresultat");
    gtk_box_append(GTK_BOX(box),v->resultat);
    gtk_expander_set_child(GTK_EXPANDER(exp),box);
    for(GtkWidget* widget:{v->bredd,v->hojd})
        g_signal_connect(widget,"value-changed",G_CALLBACK(+[](GtkSpinButton*,gpointer p){uppdateraGeometri(static_cast<GeoVy*>(p));}),v);
    g_signal_connect(v->form,"notify::selected",G_CALLBACK(+[](GtkDropDown*,GParamSpec*,gpointer p){uppdateraGeometri(static_cast<GeoVy*>(p));}),v);
    stallInGeometripanel(exp);
    return exp;
}
void stallInGeometripanel(GtkWidget* panel){
    if(!panel)return;
    auto* v=static_cast<GeoVy*>(g_object_get_data(G_OBJECT(panel),"matte13-geo"));
    if(!v)return;
    gtk_drop_down_set_selected(GTK_DROP_DOWN(v->form),0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(v->bredd),4);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(v->hojd),3);
    uppdateraGeometri(v);
}
} // namespace matte13

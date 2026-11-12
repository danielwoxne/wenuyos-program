#include "SteghjalpGtk.h"
#include <Wenuy.h>
#include <algorithm>
#include <utility>
using wenuy::tr;

namespace matte13 {
namespace {
struct Hjalp {
    std::vector<std::string> steg;
    std::size_t visade=0;
    GtkWidget* text=nullptr;
    GtkWidget* nasta=nullptr;
    GtkWidget* las=nullptr;
    std::function<void(const std::string&)> upplasning;
};
void ritaHjalp(Hjalp* h){
    std::string t;
    for(std::size_t i=0;i<h->visade;++i){
        if(!t.empty())t+="\n\n";
        t+=std::to_string(i+1)+". "+h->steg[i];
    }
    if(t.empty())t=tr("One step at a time. Press the button to start.",
                         "Ett steg i taget. Tryck på knappen när du vill börja.");
    gtk_label_set_text(GTK_LABEL(h->text),t.c_str());
    bool kvar=h->visade<h->steg.size();
    gtk_button_set_label(GTK_BUTTON(h->nasta),kvar?tr("Show next step","Visa nästa steg"):
                              tr("Start over","Börja om"));
    gtk_widget_set_sensitive(h->las,h->visade>0);
}
}
GtkWidget* skapaSteghjalp(std::function<void(const std::string&)> lasUpp){
    GtkWidget* exp=gtk_expander_new(tr("Help, step by step","Hjälp steg för steg"));
    gtk_widget_add_css_class(exp,"matte13-steghjalp");
    gtk_expander_set_expanded(GTK_EXPANDER(exp),FALSE);
    Hjalp* h=new Hjalp;
    h->upplasning=std::move(lasUpp);
    g_object_set_data_full(G_OBJECT(exp),"matte13-hjalp",h,+[](gpointer p){delete static_cast<Hjalp*>(p);});
    GtkWidget* box=gtk_box_new(GTK_ORIENTATION_VERTICAL,12);
    gtk_widget_set_margin_top(box,12);
    h->text=gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(h->text),0);
    gtk_label_set_wrap(GTK_LABEL(h->text),TRUE);
    gtk_label_set_selectable(GTK_LABEL(h->text),TRUE);
    gtk_widget_add_css_class(h->text,"matte13-hjalptext");
    gtk_box_append(GTK_BOX(box),h->text);
    GtkWidget* rad=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);
    h->nasta=gtk_button_new_with_label(tr("Show next step","Visa nästa steg"));
    gtk_widget_add_css_class(h->nasta,"suggested-action");
    gtk_box_append(GTK_BOX(rad),h->nasta);
    h->las=gtk_button_new_with_label(tr("Read last step","Läs upp senaste steget"));
    gtk_box_append(GTK_BOX(rad),h->las);
    gtk_box_append(GTK_BOX(box),rad);
    gtk_expander_set_child(GTK_EXPANDER(exp),box);
    g_signal_connect(h->nasta,"clicked",G_CALLBACK(+[](GtkButton*,gpointer ptr){
        auto* h=static_cast<Hjalp*>(ptr);
        if(h->visade<h->steg.size())++h->visade;
        else h->visade=0;
        ritaHjalp(h);
    }),h);
    g_signal_connect(h->las,"clicked",G_CALLBACK(+[](GtkButton*,gpointer ptr){
        auto* h=static_cast<Hjalp*>(ptr);
        if(h->visade && h->upplasning)h->upplasning(h->steg[h->visade-1]);
    }),h);
    ritaHjalp(h);
    return exp;
}
void stallInSteghjalp(GtkWidget* panel,const std::vector<std::string>& steg){
    if(!panel)return;
    auto* h=static_cast<Hjalp*>(g_object_get_data(G_OBJECT(panel),"matte13-hjalp"));
    if(!h)return;
    h->steg.clear();
    for(const auto& s:steg)
        if(!s.empty() && std::find(h->steg.begin(),h->steg.end(),s)==h->steg.end())h->steg.push_back(s);
    h->visade=0;
    gtk_expander_set_expanded(GTK_EXPANDER(panel),FALSE);
    gtk_widget_set_visible(panel,!h->steg.empty());
    ritaHjalp(h);
}
}

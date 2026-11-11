// SPDX-License-Identifier: GPL-3.0-or-later
// Tiny, independent bridge to WenuyOS native window functions or GTK fallback.
#pragma once
#include <adwaita.h>
#ifdef HAVE_WENUY
#include <Wenuy.h>
#include <WenuyWidgets.h>
#endif
namespace app {
inline bool sv() {
#ifdef HAVE_WENUY
 return wenuy::swedish();
#else
 const char* locale=g_getenv("LANGUAGE");
 if(!locale || !*locale)locale=g_getenv("LANG");
 return locale && g_str_has_prefix(locale,"sv");
#endif
}
inline const char* tr(const char* en,const char* swedish){return sv()?swedish:en;}
inline void loadStyle(const char* applicationId){
#ifdef HAVE_WENUY
 wenuy::i18nInit(applicationId);wenuy::loadStyle();
#else
 (void)applicationId;
#endif
}
inline GtkWidget* windowNew(GtkApplication* application,const char* title){
#ifdef HAVE_WENUY
 return wenuy_app_window_new(application,title);
#else
 GtkWidget* window=adw_application_window_new(application);
 gtk_window_set_title(GTK_WINDOW(window),title);
 gtk_window_set_default_size(GTK_WINDOW(window),1000,680);
 gtk_widget_set_size_request(window,360,240);
 GtkWidget* toolbar=adw_toolbar_view_new();
 GtkWidget* header=adw_header_bar_new();
 adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar),header);
 GtkWidget* status=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,10);
 gtk_widget_set_margin_start(status,15);gtk_widget_set_margin_end(status,15);
 gtk_widget_set_margin_top(status,5);gtk_widget_set_margin_bottom(status,5);
 GtkWidget* left=gtk_label_new(""),*right=gtk_label_new("");
 gtk_widget_set_hexpand(left,TRUE);gtk_label_set_xalign(GTK_LABEL(left),0);
 gtk_box_append(GTK_BOX(status),left);gtk_box_append(GTK_BOX(status),right);
 adw_toolbar_view_add_bottom_bar(ADW_TOOLBAR_VIEW(toolbar),status);
 GtkWidget* overlay=adw_toast_overlay_new();
 adw_toast_overlay_set_child(ADW_TOAST_OVERLAY(overlay),toolbar);
 adw_application_window_set_content(ADW_APPLICATION_WINDOW(window),overlay);
 g_object_set_data(G_OBJECT(window),"studio-toolbar",toolbar);
 g_object_set_data(G_OBJECT(window),"studio-left",left);
 g_object_set_data(G_OBJECT(window),"studio-right",right);
 g_object_set_data(G_OBJECT(window),"studio-overlay",overlay);
 return window;
#endif
}
inline void windowAddTopBar(GtkWidget* window,GtkWidget* child){
#ifdef HAVE_WENUY
 wenuy_app_window_add_top_bar(window,child);
#else
 adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(g_object_get_data(G_OBJECT(window),"studio-toolbar")),child);
#endif
}
inline void windowSetContent(GtkWidget* window,GtkWidget* child){
#ifdef HAVE_WENUY
 wenuy_app_window_set_content(window,child);
#else
 adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(g_object_get_data(G_OBJECT(window),"studio-toolbar")),child);
#endif
}
inline void windowSetStatus(GtkWidget* window,const char* left,const char* right){
#ifdef HAVE_WENUY
 wenuy_app_window_set_status(window,left,right);
#else
 gtk_label_set_text(GTK_LABEL(g_object_get_data(G_OBJECT(window),"studio-left")),left?left:"");
 gtk_label_set_text(GTK_LABEL(g_object_get_data(G_OBJECT(window),"studio-right")),right?right:"");
#endif
}
inline void windowToast(GtkWidget* window,const char* message){
#ifdef HAVE_WENUY
 wenuy_app_window_toast(window,message);
#else
 adw_toast_overlay_add_toast(ADW_TOAST_OVERLAY(g_object_get_data(G_OBJECT(window),"studio-overlay")),adw_toast_new(message));
#endif
}
}

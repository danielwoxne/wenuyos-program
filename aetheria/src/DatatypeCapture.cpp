#include "DatatypeCapture.h"
#include "ScreenshotPixels.h"
#include <wenuy/WenuyDatatype.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <iostream>
namespace aetheria {
bool saveScreenshotDatatypes(const std::string& path,const uint32_t* pixels,
                             int width,int height) {
 if(!pixels||width<1||height<1||width>1920||height>1080)return false;
 GdkPixbuf* image=gdk_pixbuf_new(GDK_COLORSPACE_RGB,TRUE,8,width,height);
 if(!image)return false;
 const int stride=gdk_pixbuf_get_rowstride(image);
 if(!screenshotPixelsRGBA(pixels,width,height,gdk_pixbuf_get_pixels(image),
                          static_cast<size_t>(stride))) {
   g_object_unref(image);return false;
 }
 GError* error=nullptr;
 const gboolean saved=wenuy_datatype_save_pixbuf(image,path.c_str(),&error);
 if(error) {
   std::cerr<<"Datatyper: "<<error->message<<"\n";g_error_free(error);
 }
 g_object_unref(image);
 return saved!=FALSE;
}
}

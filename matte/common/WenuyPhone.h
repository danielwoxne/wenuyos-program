// Telefoner via kabel – känner igen en telefon bland GIO:s enheter (mtp://, afc://, gphoto2://).
// Ren C++ utan GTK, så att panelen, Filer och Telefon delar samma regel och den kan provas (PhoneTest.cpp).
// Plan: claude/WenuyOS-telefon.md (del 1).
#pragma once
#include <algorithm>
#include <cctype>
#include <string>

namespace wenuyphone {

enum class Kind { None, Android, IPhone, Camera };

inline std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

inline bool startsWith(const std::string& s, const char* p) { return s.rfind(p, 0) == 0; }

// uri  = enhetens aktiveringsadress eller rot (t.ex. "mtp://Google_Pixel_7_2A1B/", "afc://00008110…/")
// name = namnet GIO visar ("Pixel 7", "Daniels iPhone", "Canon EOS")
inline Kind kind(const std::string& uri, const std::string& name) {
    const std::string u = lower(uri), n = lower(name);
    const bool apple = n.find("iphone") != std::string::npos || n.find("ipad") != std::string::npos
                    || n.find("ipod") != std::string::npos || u.find("apple") != std::string::npos;
    if (startsWith(u, "afc://")) return Kind::IPhone;                      // Apples filprotokoll – alltid iOS
    if (startsWith(u, "mtp://")) return apple ? Kind::IPhone : Kind::Android;   // MTP = i praktiken Android
    if (startsWith(u, "gphoto2://")) return apple ? Kind::IPhone : Kind::Camera;   // iPhone utan usbmuxd syns som kamera (PTP)
    return Kind::None;
}

inline bool isPhone(Kind k) { return k == Kind::Android || k == Kind::IPhone; }

// Ikonnamn i WenuyOS ikontema (theme/color-icons/install.py: alias till färgikonen phone)
inline const char* icon(Kind k) {
    switch (k) {
        case Kind::Android: return "phone-android";
        case Kind::IPhone:  return "phone-apple-iphone";   // samma namn som gvfs använder
        case Kind::Camera:  return "camera-photo";
        default:            return "drive-removable-media";
    }
}

}  // namespace wenuyphone

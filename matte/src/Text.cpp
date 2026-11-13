#include "Text.h"

#include <glib.h>

namespace matte {

std::string gemener(const std::string& s) {
    gchar* g = g_utf8_strdown(s.c_str(), -1);
    std::string r = g;
    g_free(g);
    return r;
}

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\n\r"), b = s.find_last_not_of(" \t\n\r");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

}  // namespace matte

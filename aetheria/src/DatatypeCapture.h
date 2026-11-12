#pragma once
#include <cstdint>
#include <string>
namespace aetheria {
// Save image using the system Datatyper plugin registry (PNG, Amiga IFF, etc.).
// Called only for an explicit user-supplied screenshot path. No network.
bool saveScreenshotDatatypes(const std::string& path,const uint32_t* pixels,
                             int width,int height);
}

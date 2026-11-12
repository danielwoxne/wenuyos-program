// Flytta en elev mellan datorer: allt om eleven i en enda fil (t.ex. på ett USB-minne).
// Framstegen, händelseloggen och teckningarna. Inget skickas över nätet. (Del av wenuy-lara.)
#pragma once
#include <string>

namespace matte {

bool exporteraElev(const std::string& namn, const std::string& fil, std::string* fel = nullptr);
// Läser filen och lägger in eleven. Finns eleven redan och ersatt = false händer inget och *fanns blir true.
// Returnerar elevens namn (filnamnet), tomt vid fel.
std::string importeraElev(const std::string& fil, bool ersatt, std::string* fel = nullptr, bool* fanns = nullptr,
                          std::string* visningsnamn = nullptr);

}  // namespace matte

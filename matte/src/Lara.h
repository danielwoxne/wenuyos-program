// wenuy-lara – den ämnesoberoende grunden i Matte, tänkt att återanvändas för fler ämnen (Svenska …).
//
// Hit hör: kort och uppgifter (Data), JSON, framsteg och repetition (Framsteg), händelseloggen (Logg),
// uppläsningen (Uppl) och texthjälp (Text). Det som är matematik – tal och bråk (Tal), uttryck (Algebra),
// rättningen av tal och uttryck (Bedom), figurerna (Rita) – ligger utanför. Se docs/NYTT-AMNE.md.
#pragma once
#include <string>

namespace matte {

// Programmets id – bestämmer var elevdata (~/.local/share/<id>/) och innehållet (share/<id>/) ligger.
// Anropas först i main(). Standard: se.wenuyos.app.matte
void laraInit(const std::string& appId, const std::string& datamiljo = "MATTE_DATA");
const std::string& appId();
const std::string& datamiljo();     // miljövariabeln som pekar ut innehållet vid utveckling och tester

}  // namespace matte

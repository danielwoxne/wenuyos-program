#include "SystemIntegration.h"
#include <Wenuy.h>
#include <clocale>
#include <cstdlib>
#include <cstring>

namespace schack_system {
void initI18n() {
 std::setlocale(LC_ALL,"");
 wenuy::i18nInit("se.wenuyos.app.schack");
}
const char* translate(const char* english,const char* swedish) {
 return wenuy::tr(english,swedish);
}
void applyStyle() {
 // WenuyOS central theme, reduced motion and light/dark preferences.
 wenuy::loadStyle();
}
void playMoveSound(bool gameFinished) {
 // The system sound helper respects the user's WenuyOS sound preferences.
 wenuy::playSound(gameFinished ? "done" : "step");
}
bool allowBoardMotion() {
 // Explicitly disabled motion and WenuyOS low profile always use instant moves.
 // WENUYOS_PERF mirrors the runtime low-profile environment in test sessions.
 const char* perf=std::getenv("WENUYOS_PERF");
 return wenuy::userSetting("motion","on")!="off" &&
        !wenuy::lowProfile() && !(perf && std::strcmp(perf,"low")==0);
}

}

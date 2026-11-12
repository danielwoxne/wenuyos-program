#include "SystemIntegration.h"
#include <Wenuy.h>
#include <clocale>

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
}

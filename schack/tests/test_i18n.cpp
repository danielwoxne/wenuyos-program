#include "SystemIntegration.h"
#include <clocale>
#include <cstdlib>
#include <cstring>
#include <iostream>

// Smoke test against the actual system language API, not the game's former wrapper.
int main() {
 ::setenv("WENUY_LOCALEDIR","/tmp/wenuy-schack-no-translations",1);
 std::setlocale(LC_ALL,"");
 schack_system::initI18n();

 ::setenv("LANG","sv_SE.UTF-8",1);
 if(std::strcmp(schack_system::translate("New game","Nytt parti"),"Nytt parti")!=0){
  std::cerr<<"Swedish fallback missing\n";return 1;
 }
 ::setenv("LANG","en_US.UTF-8",1);
 if(std::strcmp(schack_system::translate("New game","Nytt parti"),"New game")!=0){
  std::cerr<<"English fallback missing\n";return 2;
 }
 ::setenv("LANG","de_DE.UTF-8",1);
 if(std::strcmp(schack_system::translate("New game","Nytt parti"),"New game")!=0){
  std::cerr<<"Missing catalog must fall back to English\n";return 3;
 }
 std::cout<<"OK: common WenuyOS i18n API with sv/en and missing-language fallback\n";
}

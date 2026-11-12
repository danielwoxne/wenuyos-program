#pragma once
// Adapter isolating the GTK4 runtime ABI wrapper from the WenuyOS SDK headers.
// In particular, RuntimeGtk.cpp must not include Wenuy.h directly.
namespace schack_system {
void initI18n();
const char* translate(const char* english,const char* swedish);
void applyStyle();
void playMoveSound(bool gameFinished);
// Honour WenuyOS motion preference and low-memory/CPU performance profile.
bool allowBoardMotion();
}

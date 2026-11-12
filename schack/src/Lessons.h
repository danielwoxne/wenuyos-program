#pragma once
#include "Chess.h"
#include <cstddef>
#include <array>
#include <cstdint>
#include <string>

namespace chess {
struct Lesson {
    const char* titleEn;
    const char* titleSv;
    const char* taskEn;
    const char* taskSv;
    const char* hintEn;
    const char* hintSv;
    const char* successEn;
    const char* successSv;
    const char* fen;
    const char* solutionUci;
};
// A deliberately tiny, offline and completely deterministic curriculum.
// Callers must not persist pointers to individual lessons across versions.
std::size_t lessonCount();
const Lesson& lesson(std::size_t index);
bool correctLessonMove(std::size_t index, const Move& move);
// One-ply lessons expose one scripted move. Multi-ply lessons expose
// player move, deterministic opponent reply, then player move.
std::size_t lessonLineLength(std::size_t index);
const char* lessonLineMove(std::size_t index,std::size_t ply);
bool correctLessonMoveAt(std::size_t index,std::size_t ply,const Move& move);
// Preserve completion bits from 0.4; track missed attempts independently.
std::size_t recommendedLesson(std::uint64_t completed,
                              const std::array<unsigned,64>& mistakes);
std::size_t recommendedLesson(std::uint64_t completed,
                              const std::array<unsigned,64>& mistakes,
                              const std::array<unsigned,64>& successes);
// Teaching groups are stable for saved progression.
std::size_t lessonGroup(std::size_t index);
std::size_t lessonGroupCount();
const char* lessonLevelEn(std::size_t index);
const char* lessonLevelSv(std::size_t index);
bool validateLessons(std::string* error = nullptr);
}

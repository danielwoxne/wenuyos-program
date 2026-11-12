#pragma once
// Restored Aetheria 0.6 gameplay helpers. No SDL, graphics driver, network,
// disk access or change to the persistent World model.
#include "Model.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace aetheria {
enum class GoalKind : uint8_t { None, Crystal, Guard, Shrine, Elder, Herb };

struct QuestTarget {
  GoalKind kind=GoalKind::None;
  legends::Vec2 point{};
  float distance=0.f;
  bool valid() const noexcept {return kind!=GoalKind::None;}
};

// Closest outstanding target for the current story chapter, with a side-quest
// fallback when no main objective is available. All positions are values, not
// references into vectors which may move during simulation or save loading.
inline QuestTarget nearestQuestTarget(const legends::World& w) noexcept {
  QuestTarget best{};
  auto consider=[&](GoalKind kind,legends::Vec2 point) {
    if(!std::isfinite(w.hero.pos.x)||!std::isfinite(w.hero.pos.y)||
       !std::isfinite(point.x)||!std::isfinite(point.y))return;
    const float distance=std::hypot(point.x-w.hero.pos.x,point.y-w.hero.pos.y);
    if(std::isfinite(distance)&&(!best.valid()||distance<best.distance))
      best={kind,point,distance};
  };
  if(!w.won && !w.lost) {
    switch(w.questStage()) {
      case 0:
        for(const auto& crystal:w.crystals)
          if(!crystal.taken)consider(GoalKind::Crystal,crystal.pos);
        break;
      case 1:
        for(const auto& enemy:w.enemies)
          if(enemy.alive)consider(GoalKind::Guard,enemy.pos);
        break;
      case 2:
        consider(GoalKind::Shrine,w.shrine);
        break;
      default:break;
    }
  }
  if(best.valid()||w.won||w.lost)return best;
  if(w.sideQuest==0||w.sideQuest==2) {
    for(const auto& npc:w.npcs)
      if(npc.kind==0)consider(GoalKind::Elder,npc.pos);
  }else if(w.sideQuest==1) {
    for(const auto& herb:w.herbs)
      if(!herb.taken)consider(GoalKind::Herb,herb.pos);
  }
  return best;
}

inline float directionToTarget(legends::Vec2 from,legends::Vec2 to,
                               float cameraYaw) noexcept {
  if(!std::isfinite(from.x)||!std::isfinite(from.y)||
     !std::isfinite(to.x)||!std::isfinite(to.y)||!std::isfinite(cameraYaw))
    return 0.f;
  const float dx=to.x-from.x,dy=to.y-from.y;
  if(dx==0.f&&dy==0.f)return 0.f;
  return std::remainder(std::atan2(dy,dx)-cameraYaw,6.283185307179586f);
}

class CameraControl {
public:
  float zoom()const noexcept{return zoom_;}
  // Wheel up zooms in; hard limits avoid near-plane and clipping surprises.
  void wheel(int steps) noexcept {
    const int bounded=std::clamp(steps,-6,6);
    zoom_=std::clamp(zoom_*std::pow(.90f,static_cast<float>(bounded)),
                     .70f,1.65f);
  }
  static float dragYaw(float yaw,float relativeMouseX) noexcept {
    if(!std::isfinite(yaw))return 0.f;
    if(!std::isfinite(relativeMouseX))return yaw;
    return std::remainder(yaw+std::clamp(relativeMouseX,-160.f,160.f)*.0075f,
                          6.283185307179586f);
  }
private:
  float zoom_=1.f;  // identical framing to Aetheria 0.5 until user zooms
};

// 90 seconds of *active* game time; never advance in pause, dialogue,
// victory, defeat, or screenshot previews. Saving stays in World::save,
// which already uses a temporary file and atomic rename.
class AutoSaveClock {
public:
  static constexpr float IntervalSeconds=90.f;
  static constexpr float RetrySeconds=15.f;
  bool advance(float dt,bool active) noexcept {
    if(!active||!std::isfinite(dt)||dt<=0.f)return false;
    seconds_=std::min(IntervalSeconds,seconds_+std::min(dt,.05f));
    return seconds_>=IntervalSeconds;
  }
  void complete(bool succeeded) noexcept {
    seconds_=succeeded?0.f:IntervalSeconds-RetrySeconds;
  }
  void reset() noexcept {seconds_=0.f;}
  float elapsed() const noexcept {return seconds_;}
private:
  float seconds_=0.f;
};
} // namespace aetheria

#pragma once
// Aetheria UI: logical 960x600 coordinates shared by drawing, mouse hit tests,
// and headless regressions. No SDL or graphics driver dependency.
#include <cstdint>

namespace aetheria { namespace ui {
constexpr int Width=960, Height=600;
struct Rect {
 int x,y,w,h;
 constexpr bool contains(int px,int py)const {
  return px>=x && py>=y && px<x+w && py<y+h;
 }
 constexpr bool insideScreen()const {
  return x>=0 && y>=0 && w>0 && h>0 && x+w<=Width && y+h<=Height;
 }
};
inline constexpr Rect status{16,14,287,115};
inline constexpr Rect quest{16,137,322,68};
inline constexpr Rect sideQuest{16,211,322,60};
inline constexpr Rect inventoryTab{351,17,128,36};
inline constexpr Rect mapTab{486,17,143,36};
inline constexpr Rect helpTab{636,17,133,36};
inline constexpr Rect minimapButton{808,18,136,150};
inline constexpr Rect elementalAction{30,529,216,46};
inline constexpr Rect attackAction{259,529,216,46};
inline constexpr Rect skillAction{488,529,216,46};
inline constexpr Rect potionAction{717,529,211,46};
inline constexpr Rect interactPrompt{341,462,278,39};
inline constexpr Rect inventoryClose{733,76,91,33};
inline constexpr Rect inventorySword{483,175,315,95};
inline constexpr Rect inventoryCharm{483,284,315,116};
inline constexpr Rect dialogueClose{792,341,58,36};
inline constexpr Rect dialogueFirst{103,449,350,45};
inline constexpr Rect dialogueSecond{476,449,350,45};
inline constexpr Rect helpClose{658,89,114,36};
inline constexpr Rect mapClose{689,48,128,38};
inline constexpr Rect pauseResume{329,269,302,51};
enum class Action : uint8_t {
 None, Inventory, Map, Help, QuestToggle, Element, Attack, Skill, Potion,
 Interact, Close, CraftSword, CraftCharm, DialogueFirst, DialogueSecond, Resume
};
struct State {
 bool inventory=false,help=false,map=false,dialogue=false,paused=false,ended=false;
 bool nearNpc=false;
};
constexpr Action hitTest(int px,int py,State state) {
 if(px<0||py<0||px>=Width||py>=Height)return Action::None;
 // An open modal never leaks a click through to combat or to the HUD.
 if(state.inventory) {
  if(inventoryClose.contains(px,py))return Action::Close;
  if(inventorySword.contains(px,py))return Action::CraftSword;
  if(inventoryCharm.contains(px,py))return Action::CraftCharm;
  return Action::None;
 }
 if(state.dialogue) {
  if(dialogueClose.contains(px,py))return Action::Close;
  if(dialogueFirst.contains(px,py))return Action::DialogueFirst;
  if(dialogueSecond.contains(px,py))return Action::DialogueSecond;
  return Action::None;
 }
 if(state.help)return helpClose.contains(px,py)?Action::Close:Action::None;
 if(state.map)return mapClose.contains(px,py)?Action::Close:Action::None;
 if(state.ended)return Action::None;
 if(state.paused)return pauseResume.contains(px,py)?Action::Resume:Action::None;
 if(inventoryTab.contains(px,py))return Action::Inventory;
 if(mapTab.contains(px,py)||minimapButton.contains(px,py))return Action::Map;
 if(helpTab.contains(px,py))return Action::Help;
 if(sideQuest.contains(px,py))return Action::QuestToggle;
 if(elementalAction.contains(px,py))return Action::Element;
 if(attackAction.contains(px,py))return Action::Attack;
 if(skillAction.contains(px,py))return Action::Skill;
 if(potionAction.contains(px,py))return Action::Potion;
 if(state.nearNpc&&interactPrompt.contains(px,py))return Action::Interact;
 return Action::None;
}
}} // namespace aetheria::ui

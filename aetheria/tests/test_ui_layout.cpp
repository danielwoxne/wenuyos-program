#include "UiLayout.h"
#include <iostream>
#include <stdexcept>
using namespace aetheria::ui;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){
 try{
  for(auto r:{status,quest,sideQuest,inventoryTab,mapTab,helpTab,minimapButton,
              elementalAction,attackAction,skillAction,potionAction,interactPrompt,
              inventoryClose,inventorySword,inventoryCharm,dialogueClose,
              dialogueFirst,dialogueSecond,helpClose,mapClose,pauseResume})
   check(r.insideScreen(),"all interactive regions fit inside 960x600");
  check(hitTest(377,31,{})==Action::Inventory,"inventory tab");
  check(hitTest(525,32,{})==Action::Map,"map tab");
  check(hitTest(825,90,{})==Action::Map,"map click");
  check(hitTest(700,31,{})==Action::Help,"help tab");
  check(hitTest(110,244,{})==Action::QuestToggle,"quest tracker collapse");
  check(hitTest(110,547,{})==Action::Element,"element");
  check(hitTest(349,547,{})==Action::Attack,"attack");
  check(hitTest(580,547,{})==Action::Skill,"skill");
  check(hitTest(816,547,{})==Action::Potion,"potion");
  check(hitTest(450,475,{})==Action::None,"interaction needs NPC");
  State near;near.nearNpc=true;
  check(hitTest(450,475,near)==Action::Interact,"near NPC interact");
  State inventory;inventory.inventory=true;
  check(hitTest(548,197,inventory)==Action::CraftSword,"craft sword");
  check(hitTest(548,309,inventory)==Action::CraftCharm,"craft charm");
  check(hitTest(778,86,inventory)==Action::Close,"close bag");
  check(hitTest(353,551,inventory)==Action::None,"no clickthrough from bag to attack");
  State dialogue;dialogue.dialogue=true;
  check(hitTest(150,466,dialogue)==Action::DialogueFirst,"first dialogue option");
  check(hitTest(520,466,dialogue)==Action::DialogueSecond,"second dialogue option");
  check(hitTest(816,356,dialogue)==Action::Close,"close dialogue");
  check(hitTest(353,551,dialogue)==Action::None,"no dialogue combat clickthrough");
  State help;help.help=true;
  check(hitTest(700,99,help)==Action::Close,"close help");
  State map;map.map=true;
  check(hitTest(747,67,map)==Action::Close,"close map");
  State pause;pause.paused=true;
  check(hitTest(440,292,pause)==Action::Resume,"resume from pause");
  check(hitTest(353,551,pause)==Action::None,"pause prevents attack");
  State end;end.ended=true;
  check(hitTest(353,551,end)==Action::None,"end game prevents combat");
  check(hitTest(-1,0,{})==Action::None&&hitTest(960,1,{})==Action::None
        &&hitTest(10,600,{})==Action::None,"reject outside bounds");
  std::cout<<"Aetheria UI hit boxes and modal safety OK\n";
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}

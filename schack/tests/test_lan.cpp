#include "LanChess.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <cstdlib>

using chess::lan::Peer;
void check(bool value,const char* message){
 if(!value){std::cerr<<"LAN test failed: "<<message<<"\n";std::exit(1);}
}
int main(){
 check(chess::lan::localIPv4("127.0.0.1"),"loopback");
 check(chess::lan::localIPv4("192.168.1.5"),"home LAN");
 check(chess::lan::localIPv4("10.3.4.5"),"private LAN");
 check(chess::lan::localIPv4("172.31.1.3"),"private 172 block");
 check(!chess::lan::localIPv4("172.32.1.3"),"reject public 172 block");
 check(!chess::lan::localIPv4("8.8.8.8"),"reject internet");
 check(!chess::lan::localIPv4("example.com"),"reject DNS");
 constexpr unsigned short port=48791;
 std::atomic<bool> success{true};
 std::string serverError;
 std::thread host([&]{
  Peer white;
  if(!white.host(port,3000,&serverError)){success=false;return;}
  if(!white.hostSide()||!white.myTurn()){success=false;return;}
  if(white.sendMove("e2e5",&serverError)){success=false;return;}
  if(!white.sendMove("e2e4",&serverError)){success=false;return;}
  if(!white.receiveMove(3000,&serverError)){success=false;return;}
  if(!white.sendMove("g1f3",&serverError)){success=false;return;}
  if(white.game().moves.size()!=3){success=false;return;}
 });
 std::this_thread::sleep_for(std::chrono::milliseconds(70));
 Peer black;std::string error;
 if(!black.join("127.0.0.1",port,3000,&error)){
  host.join();std::cerr<<error<<"\n";check(false,"join");}
 check(!black.myTurn(),"host plays white");
 check(black.receiveMove(3000,&error),"received e4");
 check(black.game().moves.size()==1,"one move");
 check(black.sendMove("e7e5",&error),"send e5");
 check(black.receiveMove(3000,&error),"receive Nf3");
 check(black.game().moves.size()==3,"three moves");
 check(black.game().board().fen()=="rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2","same position");
 host.join();
 check(success.load(),serverError.c_str());
 check(!black.sendMove("h2h4",&error),"reject wrong side move");
 black.close();check(!black.connected(),"disconnect");
 std::cout<<"OK: private-address filter, explicit socket, legal moves, sync and disconnect\n";
}

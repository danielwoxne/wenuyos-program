#include "LanChess.h"
#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
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
 check(chess::lan::validPairingCode("004259"),"valid leading-zero code");
 check(!chess::lan::validPairingCode("4259"),"short pairing code rejected");
 check(!chess::lan::validPairingCode("12345x"),"non-digit pairing code rejected");
 {// Claude review: a bad pairing code must say so, not blame the address
  Peer p;std::string err;
  check(!p.join("192.168.1.5",48790,100,&err,"123")&&err=="Six-digit pairing code required","join reports bad pairing code");
  err.clear();check(!p.join("8.8.8.8",48790,100,&err,"123456")&&err=="Use a private IPv4 network address","join reports public address");
 }
 constexpr const char* pairing="004259";
 constexpr unsigned short port=48791;
 std::atomic<bool> success{true};
 std::string serverError;
 std::thread host([&]{
  Peer white;
  if(!white.host(port,3000,&serverError,nullptr,pairing)){success=false;return;}
  if(!white.hostSide()||!white.myTurn()){success=false;return;}
  if(white.sendMove("e2e5",&serverError)){success=false;return;}
  if(!white.sendMove("e2e4",&serverError)){success=false;return;}
  if(!white.receiveMove(3000,&serverError)){success=false;return;}
  if(!white.sendMove("g1f3",&serverError)){success=false;return;}
  if(white.game().moves.size()!=3){success=false;return;}
 });
 std::this_thread::sleep_for(std::chrono::milliseconds(70));
 Peer black;std::string error;
 if(!black.join("127.0.0.1",port,3000,&error,pairing)){
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
 // A client with an incorrect pairing code must not enter a game.
 std::atomic<bool> rejected{false};
 std::thread guard([&] {
  Peer hostPeer;std::string rejection;
  rejected=!hostPeer.host(port,3000,&rejection,nullptr,pairing);
 });
 std::this_thread::sleep_for(std::chrono::milliseconds(70));
 Peer stranger;
 std::string mismatch;
 check(!stranger.join("127.0.0.1",port,3000,&mismatch,"999999"),"reject mismatched pairing code");
 guard.join();
 check(rejected.load(),"host also rejects mismatched pairing code");
 check(!stranger.connected(),"stranger never enters a game");
 // A connected but silent peer must not defeat the visible Cancel button.
 // This test explicitly reaches the handshake and then cancels the host.
 std::atomic<bool> cancelHandshake{false}, rejectedSilentPeer{false};
 std::thread stalledHost([&]{
  Peer listener;std::string stalledError;
  rejectedSilentPeer=!listener.host(static_cast<std::uint16_t>(port+1),
                                     5000,&stalledError,&cancelHandshake,pairing) &&
                     !listener.connected();
 });
 std::this_thread::sleep_for(std::chrono::milliseconds(70));
 const int silent=::socket(AF_INET,SOCK_STREAM|SOCK_CLOEXEC,0);
 check(silent>=0,"create silent loopback peer");
 sockaddr_in target{};
 target.sin_family=AF_INET;target.sin_port=htons(port+1);
 target.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 check(::connect(silent,reinterpret_cast<sockaddr*>(&target),sizeof(target))==0,
       "connect silent peer");
 pollfd waitGreeting{silent,POLLIN,0};
 check(::poll(&waitGreeting,1,1000)>0 && (waitGreeting.revents&POLLIN),
       "host entered pairing handshake");
 char greeting[16]{};
 check(::recv(silent,greeting,sizeof(greeting),0)>0,"host sent protocol greeting");
 const auto cancelledAt=std::chrono::steady_clock::now();
 cancelHandshake.store(true);
 stalledHost.join();
 const auto cancelLatency=std::chrono::duration_cast<std::chrono::milliseconds>(
     std::chrono::steady_clock::now()-cancelledAt).count();
 ::shutdown(silent,SHUT_RDWR);::close(silent);
 check(rejectedSilentPeer.load(),"silent handshake does not become a game");
 check(cancelLatency<1500,"cancel interrupts silent pairing in bounded time");
 std::cout<<"OK: private-address filter, explicit pairing, legal moves, sync, disconnect and handshake cancel\n";
}

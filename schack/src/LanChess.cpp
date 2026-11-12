#include "LanChess.h"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <chrono>
#include <string>

namespace chess::lan {
namespace {
constexpr const char* hello = "WCHESS/1\n";
void fail(std::string* error, const char* message) { if(error)*error=message; }
void shutdownFd(int& fd) {
 if(fd>=0){::shutdown(fd,SHUT_RDWR);::close(fd);fd=-1;}
}
bool writable(int fd,int timeoutMs) {
 pollfd p{fd,POLLOUT,0};
 return ::poll(&p,1,timeoutMs)>0 && (p.revents&POLLOUT);
}
bool sendAll(int fd,const std::string& data,int timeoutMs) {
 std::size_t offset=0;
 while(offset<data.size()) {
  if(!writable(fd,timeoutMs))return false;
  const auto n=::send(fd,data.data()+offset,data.size()-offset,MSG_NOSIGNAL|MSG_DONTWAIT);
  if(n>0){offset+=static_cast<std::size_t>(n);continue;}
  if(n<0&&(errno==EINTR||errno==EAGAIN))continue;
  return false;
 }
 return true;
}
bool readLine(int fd,std::string& line,int timeoutMs) {
 line.clear();
 const auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(std::max(0,timeoutMs));
 for(;;) {
  auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(until-std::chrono::steady_clock::now()).count();
  if(remaining<0)return false;
  pollfd p{fd,POLLIN,0};
  if(::poll(&p,1,static_cast<int>(remaining))<=0||!(p.revents&POLLIN))return false;
  char c=0;const auto n=::recv(fd,&c,1,0);
  if(n!=1)return false;
  if(c=='\n')return true;
  if(c=='\r'||static_cast<unsigned char>(c)<32||line.size()>=80)return false;
  line.push_back(c);
 }
}
}
bool localIPv4(const std::string& address) {
 in_addr addr{};
 if(::inet_pton(AF_INET,address.c_str(),&addr)!=1)return false;
 const std::uint32_t v=ntohl(addr.s_addr);
 return (v>>24)==10 || (v>>20)==0xAC1 || (v>>16)==0xC0A8 ||
        (v>>16)==0xA9FE || (v>>24)==127;
}
Peer::~Peer(){close();}
void Peer::close(){shutdownFd(socket_);game_.reset();host_=false;}
bool Peer::begin(int fd,bool hosting,int timeoutMs,std::string* error){
 if(fd<0){fail(error,"Socket unavailable");return false;}
 std::string incoming;
 if(!sendAll(fd,hello,timeoutMs)||!readLine(fd,incoming,timeoutMs)||incoming!="WCHESS/1"){
  fail(error,"Protocol handshake failed");::shutdown(fd,SHUT_RDWR);::close(fd);return false;
 }
 socket_=fd;host_=hosting;game_.reset();return true;
}
bool Peer::host(std::uint16_t port,int timeoutMs,std::string* error){
 close();
 if(port==0||timeoutMs<=0){fail(error,"Invalid port or timeout");return false;}
 const int server=::socket(AF_INET,SOCK_STREAM|SOCK_CLOEXEC,0);
 if(server<0){fail(error,"Cannot create listener");return false;}
 int yes=1;::setsockopt(server,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
 sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons(port);
 address.sin_addr.s_addr=htonl(INADDR_ANY);
 if(::bind(server,reinterpret_cast<sockaddr*>(&address),sizeof(address))<0||::listen(server,1)<0){
  fail(error,"Could not listen on port");::close(server);return false;
 }
 pollfd p{server,POLLIN,0};
 const bool ready=::poll(&p,1,timeoutMs)>0&&(p.revents&POLLIN);
 sockaddr_in remote{};socklen_t remoteLen=sizeof(remote);\n int fd=-1;if(ready)fd=::accept4(server,reinterpret_cast<sockaddr*>(&remote),&remoteLen,SOCK_CLOEXEC);\n if(fd>=0){char ip[INET_ADDRSTRLEN]{};::inet_ntop(AF_INET,&remote.sin_addr,ip,sizeof(ip));\n  if(!localIPv4(ip)){::close(fd);fd=-1;fail(error,"Only local IPv4 peers are allowed");}}
 ::close(server);
 if(fd<0){fail(error,"Waiting for a player timed out");return false;}
 return begin(fd,true,timeoutMs,error);
}
bool Peer::join(const std::string& address,std::uint16_t port,int timeoutMs,std::string* error){
 close();
 if(!localIPv4(address)||port==0||timeoutMs<=0){fail(error,"Use a private IPv4 network address");return false;}
 const int fd=::socket(AF_INET,SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0);
 if(fd<0){fail(error,"Cannot create socket");return false;}
 sockaddr_in remote{};remote.sin_family=AF_INET;remote.sin_port=htons(port);
 ::inet_pton(AF_INET,address.c_str(),&remote.sin_addr);
 if(::connect(fd,reinterpret_cast<sockaddr*>(&remote),sizeof(remote))<0&&errno!=EINPROGRESS){
  fail(error,"Connection failed");::close(fd);return false;
 }
 if(!writable(fd,timeoutMs)){fail(error,"Connection timed out");::close(fd);return false;}
 int socketError=0;socklen_t size=sizeof(socketError);
 if(::getsockopt(fd,SOL_SOCKET,SO_ERROR,&socketError,&size)<0||socketError){
  fail(error,"Peer unavailable");::close(fd);return false;
 }
 return begin(fd,false,timeoutMs,error);
}
bool Peer::sendMove(const std::string& notation,std::string* error){
 if(!connected()||!myTurn()){fail(error,"Not your turn");return false;}
 Move move{};
 if(!parseUci(notation,move)){fail(error,"Invalid move notation");return false;}
 // Validate before transmitting. Never apply an untrusted move.
 Game next=game_;
 if(game_.result()!="ongoing"||!next.play(move)){fail(error,"Illegal move");return false;}
 const std::string line="MOVE "+std::to_string(game_.moves.size())+" "+notation+"\n";
 if(!sendAll(socket_,line,5000)){fail(error,"Connection lost");close();return false;}
 game_=std::move(next);return true;
}
bool Peer::receiveMove(int timeoutMs,std::string* error){
 if(!connected()||myTurn()){fail(error,"No remote move expected");return false;}
 std::string line;
 if(!readLine(socket_,line,timeoutMs)){
  fail(error,"No move received or peer disconnected");close();return false;
 }
 const std::string expected="MOVE "+std::to_string(game_.moves.size())+" ";
 if(line.compare(0,expected.size(),expected)!=0||line.size()>expected.size()+5){
  fail(error,"Unexpected network message");close();return false;
 }
 Game next=game_;
 if(game_.result()!="ongoing"||!next.playUci(line.substr(expected.size()))){
  fail(error,"Remote player sent an illegal move");close();return false;
 }
 game_=std::move(next);return true;
}
} // namespace chess::lan

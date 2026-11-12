#include "LanChess.h"
#include <arpa/inet.h>
#include <cerrno>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <chrono>
#include <string>

namespace chess::lan {
namespace {
constexpr const char* hello = "WCHESS/2\n";
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
bool readLine(int fd,std::string& line,int timeoutMs,const std::atomic<bool>* cancel=nullptr) {
 line.clear();
 const auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(std::max(0,timeoutMs));
 for(;;) {
  if(cancel&&cancel->load())return false;
  auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(until-std::chrono::steady_clock::now()).count();
  if(remaining<=0)return false;
  // Poll in short slices so Cancel in the GUI also aborts an unfinished pairing
  // handshake, not just the earlier wait-for-peer loop.
  pollfd p{fd,POLLIN,0};
  const int ready=::poll(&p,1,static_cast<int>(std::min<long long>(100,remaining)));
  if(ready<0){if(errno==EINTR)continue;return false;}
  if(ready==0)continue;
  if(!(p.revents&POLLIN))return false;
  char c=0;const auto n=::recv(fd,&c,1,MSG_DONTWAIT);
  if(n<0&&(errno==EINTR||errno==EAGAIN||errno==EWOULDBLOCK))continue;
  if(n!=1)return false;
  if(c=='\n')return true;
  if(c=='\r'||static_cast<unsigned char>(c)<32||line.size()>=80)return false;
  line.push_back(c);
 }
}
} // namespace

bool validPairingCode(const std::string& code) {
 return code.size()==6 && std::all_of(code.begin(),code.end(),[](char c){return c>='0'&&c<='9';});
}
bool localIPv4(const std::string& address) {
 in_addr addr{};
 if(::inet_pton(AF_INET,address.c_str(),&addr)!=1)return false;
 const std::uint32_t v=ntohl(addr.s_addr);
 return (v>>24)==10 || (v>>20)==0xAC1 || (v>>16)==0xC0A8 ||
        (v>>16)==0xA9FE || (v>>24)==127;
}
Peer::~Peer(){close();}
void Peer::close(){shutdownFd(socket_);game_.reset();host_=false;incoming_.clear();peerClosed_=false;}
bool Peer::begin(int fd,bool hosting,int timeoutMs,const std::string& pairingCode,
                 std::string* error,const std::atomic<bool>* cancel){
 if(fd<0){fail(error,"Socket unavailable");return false;}
 if(!validPairingCode(pairingCode)){fail(error,"Six-digit pairing code required");::close(fd);return false;}
 std::string incoming;
 if((cancel&&cancel->load())||!sendAll(fd,hello,timeoutMs)||
    !readLine(fd,incoming,timeoutMs,cancel)||incoming!="WCHESS/2"){
  fail(error,"Protocol handshake failed");::shutdown(fd,SHUT_RDWR);::close(fd);return false;
 }
 if(hosting){
  if(!readLine(fd,incoming,timeoutMs,cancel)||incoming.size()!=11||incoming.compare(0,5,"PAIR ")!=0){
   fail(error,"Invalid pairing request");::shutdown(fd,SHUT_RDWR);::close(fd);return false;
  }
  unsigned diff=0;
  for(std::size_t i=0;i<6;i++)diff|=static_cast<unsigned char>(incoming[5+i]^pairingCode[i]);
  const bool correct=diff==0;
  if(!sendAll(fd,correct?"ACCEPT\n":"REJECT\n",timeoutMs)||!correct){
   fail(error,"Pairing code rejected");::shutdown(fd,SHUT_RDWR);::close(fd);return false;
  }
 }else{
  if(!sendAll(fd,"PAIR "+pairingCode+"\n",timeoutMs)||
     !readLine(fd,incoming,timeoutMs,cancel)||incoming!="ACCEPT"){
   fail(error,"Pairing code rejected or peer disconnected");
   ::shutdown(fd,SHUT_RDWR);::close(fd);return false;
  }
 }
 if(cancel&&cancel->load()){
  fail(error,"Cancelled");
  ::shutdown(fd,SHUT_RDWR);::close(fd);return false;
 }
 socket_=fd;host_=hosting;incoming_.clear();peerClosed_=false;game_.reset();return true;
}
bool Peer::host(std::uint16_t port,int timeoutMs,std::string* error,const std::atomic<bool>* cancel,const std::string& pairingCode){
 close();
 if(port==0||timeoutMs<=0||!validPairingCode(pairingCode)){fail(error,"Invalid port, timeout or pairing code");return false;}
 const int server=::socket(AF_INET,SOCK_STREAM|SOCK_CLOEXEC,0);
 if(server<0){fail(error,"Cannot create listener");return false;}
 int yes=1;::setsockopt(server,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
 sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons(port);
 address.sin_addr.s_addr=htonl(INADDR_ANY);
 if(::bind(server,reinterpret_cast<sockaddr*>(&address),sizeof(address))<0||::listen(server,1)<0){
  fail(error,"Could not listen on port");::close(server);return false;
 }
 pollfd p{server,POLLIN,0};
 sockaddr_in remote{};socklen_t remoteLen=sizeof(remote);
 int fd=-1;
 for(int elapsed=0;elapsed<timeoutMs && !(cancel&&cancel->load());elapsed+=100){
  const int wait=std::min(100,timeoutMs-elapsed);
  if(::poll(&p,1,wait)>0&&(p.revents&POLLIN)){
   fd=::accept4(server,reinterpret_cast<sockaddr*>(&remote),&remoteLen,SOCK_CLOEXEC);
   break;
  }
 }
 ::close(server);
 if(fd<0){fail(error,"Waiting for a player timed out or cancelled");return false;}
 char ip[INET_ADDRSTRLEN]{};
 ::inet_ntop(AF_INET,&remote.sin_addr,ip,sizeof(ip));
 if(!localIPv4(ip)){fail(error,"Only private IPv4 peers are allowed");::close(fd);return false;}
 if(cancel&&cancel->load()){::close(fd);fail(error,"Cancelled");return false;}
 // The listener may wait up to 60 s by user choice; a connected but silent
 // unauthenticated client must not consume that entire period.
 return begin(fd,true,std::min(timeoutMs,5000),pairingCode,error,cancel);
}
bool Peer::join(const std::string& address,std::uint16_t port,int timeoutMs,std::string* error,const std::string& pairingCode){
 close();
 if(!localIPv4(address)||port==0||timeoutMs<=0||!validPairingCode(pairingCode)){
  fail(error,"Use a private IPv4 network address");return false;
 }
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
 return begin(fd,false,timeoutMs,pairingCode,error);
}
bool Peer::sendMove(const std::string& notation,std::string* error){
 if(!connected()||!myTurn()){fail(error,"Not your turn");return false;}
 Move move{};
 if(!parseUci(notation,move)){fail(error,"Invalid move notation");return false;}
 Game next=game_;
 if(game_.result()!="ongoing"||!next.play(move)){fail(error,"Illegal move");return false;}
 const std::string line="MOVE "+std::to_string(game_.moves.size())+" "+notation+"\n";
 if(!sendAll(socket_,line,5000)){fail(error,"Connection lost");close();return false;}
 game_=std::move(next);return true;
}
Peer::Incoming Peer::pollMove(std::string& notation,std::string* error){
 notation.clear();
 if(peerClosed_){fail(error,"Peer disconnected");close();return Incoming::Closed;}
 if(!connected()){fail(error,"Not connected");return Incoming::Closed;}
 char chunk[128];
 for(;;){
  const auto n=::recv(socket_,chunk,sizeof(chunk),MSG_DONTWAIT);
  if(n>0){
   incoming_.append(chunk,static_cast<std::size_t>(n));
   if(incoming_.size()>128){fail(error,"Network message too long");close();return Incoming::Closed;}
  } else if(n==0){peerClosed_=true;break;}
  else if(errno==EINTR)continue;
  else if(errno==EAGAIN||errno==EWOULDBLOCK)break;
  else{fail(error,"Connection lost");close();return Incoming::Closed;}
 }
 const auto pos=incoming_.find('\n');
 if(pos==std::string::npos){
  if(peerClosed_){fail(error,"Peer disconnected");close();return Incoming::Closed;}
  return Incoming::Waiting;
 }
 const std::string line=incoming_.substr(0,pos);
 incoming_.erase(0,pos+1);
 const std::string expected="MOVE "+std::to_string(game_.moves.size())+" ";
 if(myTurn()||line.compare(0,expected.size(),expected)!=0||
    line.size()>expected.size()+5||line.size()<=expected.size()){
  fail(error,"Unexpected network message");close();return Incoming::Closed;
 }
 const std::string move=line.substr(expected.size());
 Game next=game_;
 if(game_.result()!="ongoing"||!next.playUci(move)){
  fail(error,"Remote player sent an illegal move");close();return Incoming::Closed;
 }
 game_=std::move(next);notation=move;return Incoming::Move;
}
bool Peer::receiveMove(int timeoutMs,std::string* error){
 if(!connected()||myTurn()){fail(error,"No remote move expected");return false;}
 const auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(std::max(0,timeoutMs));
 std::string notation;
 while(std::chrono::steady_clock::now()<until){
  const auto status=pollMove(notation,error);
  if(status==Incoming::Move)return true;
  if(status==Incoming::Closed)return false;
  pollfd p{socket_,POLLIN,0};
  const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(until-std::chrono::steady_clock::now()).count();
  if(remaining<=0)break;
  ::poll(&p,1,static_cast<int>(std::min<long long>(100,remaining)));
 }
 fail(error,"Waiting for move timed out");return false;
}
} // namespace chess::lan

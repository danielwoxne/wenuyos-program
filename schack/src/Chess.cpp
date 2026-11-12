#include "Chess.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <sstream>
#include <random>
#include <limits>
#include <unordered_map>

namespace chess {
namespace {
constexpr int file(int sq){ return sq&7; }
constexpr int rank(int sq){ return sq>>3; }
constexpr bool inside(int f,int r){ return f>=0&&f<8&&r>=0&&r<8; }
constexpr int at(int f,int r){ return r*8+f; }
bool isWhite(char p){return p>='A'&&p<='Z';}
char kind(char p){ return static_cast<char>(std::tolower(static_cast<unsigned char>(p))); }
char pawnFor(bool white){return white?'P':'p';}
bool owns(char p,bool white){return p && isWhite(p)==white;}
bool enemy(char p,bool white){return p && isWhite(p)!=white && kind(p)!='k';}
const int knightSteps[8][2]={{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}};
const int kingSteps[8][2]={{1,1},{1,0},{1,-1},{0,1},{0,-1},{-1,1},{-1,0},{-1,-1}};
const int bishopSteps[4][2]={{1,1},{1,-1},{-1,1},{-1,-1}};
const int rookSteps[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
void promote(std::vector<Move>& out,int from,int to){
    for(char p : {'q','r','b','n'})out.push_back({from,to,p});
}
void pushPawn(std::vector<Move>& out,int from,int to,bool white){
    if(rank(to)==(white?7:0))promote(out,from,to); else out.push_back({from,to,0});
}
int value(char p){switch(kind(p)){case 'p':return 100;case 'n':return 310;case 'b':return 330;case 'r':return 500;case 'q':return 900;case 'k':return 20000;default:return 0;}}
}
std::string squareName(int sq){if(sq<0||sq>=64)return "-";return std::string(1,char('a'+file(sq)))+char('1'+rank(sq));}
std::string uci(const Move& m){return squareName(m.from)+squareName(m.to)+(m.promotion?std::string(1,m.promotion):"");}
bool parseUci(const std::string& s, Move& m){
    if(s.size()!=4&&s.size()!=5)return false;
    for(int i : {0,2})if(s[i]<'a'||s[i]>'h'||s[i+1]<'1'||s[i+1]>'8')return false;
    char promo = s.size()==5?char(std::tolower(static_cast<unsigned char>(s[4]))):0;
    if(promo && promo!='q'&&promo!='r'&&promo!='b'&&promo!='n')return false;
    m={at(s[0]-'a',s[1]-'1'),at(s[2]-'a',s[3]-'1'),promo};return true;
}
Board Board::initial(){Board b;const char* back="RNBQKBNR";for(int f=0;f<8;f++){
    b.squares[at(f,0)]=back[f];b.squares[at(f,1)]='P';
    b.squares[at(f,6)]='p';b.squares[at(f,7)]=char(std::tolower(back[f]));
}return b;}
bool Board::fromFen(const std::string& fen,Board& out){
    std::istringstream in(fen);std::string placement,side,castles,epos,half,full,extra;
    if(!(in>>placement>>side>>castles>>epos>>half>>full)||(in>>extra))return false;
    Board temp;temp.castle=0;temp.ep=-1;temp.squares.fill(0);
    int r=7,f=0,nwhite=0,nblack=0;
    for(char c:placement){
        if(c=='/') {if(f!=8||r==0)return false;--r;f=0;continue;}
        if(c>='1'&&c<='8'){f+=c-'0';if(f>8)return false;continue;}
        if(std::string("PNBRQKpnbrqk").find(c)==std::string::npos||f>=8)return false;
        temp.squares[at(f++,r)]=c;
        if(c=='K')++nwhite;
        if(c=='k')++nblack;
    }
    if(r!=0||f!=8||nwhite!=1||nblack!=1||(side!="w"&&side!="b"))return false;
    temp.white=side=="w";
    if(castles!="-")for(char c:castles){unsigned bit=c=='K'?1:c=='Q'?2:c=='k'?4:c=='q'?8:0;if(!bit||(temp.castle&bit))return false;temp.castle|=bit;}
    if(epos!="-"){
        if(epos.size()!=2||epos[0]<'a'||epos[0]>'h'||(epos[1]!=(temp.white?'6':'3')))return false;
        temp.ep=at(epos[0]-'a',epos[1]-'1');
    }
    try {
        std::size_t used=0;long h=std::stol(half,&used);
        if(used!=half.size()||h<0||h>100000)return false;
        temp.halfmove=int(h);used=0;
        long fm=std::stol(full,&used);
        if(used!=full.size()||fm<1||fm>100000)return false;
        temp.fullmove=int(fm);
    } catch(...) {return false;}
    // A FEN must not place a king beside the other king or pawns on back rank.
    for(int i=0;i<8;++i)if(kind(temp.squares[i])=='p'||kind(temp.squares[56+i])=='p')return false;
    // No legal position can have the non-moving side in check.
    if(temp.inCheck(!temp.white))return false;
    out=temp;return true;
}
std::string Board::fen() const{
    std::string s;
    for(int r=7;r>=0;--r){int count=0;for(int f=0;f<8;++f){char p=squares[at(f,r)];
        if(!p){count++;continue;}if(count){s+=char('0'+count);count=0;}s+=p;
    }if(count)s+=char('0'+count);if(r)s+='/';}
    s+=white?" w ":" b "; if(!castle)s+='-';else {if(castle&1)s+='K';if(castle&2)s+='Q';if(castle&4)s+='k';if(castle&8)s+='q';}
    s+=' ';s+=squareName(ep);s+=' ';s+=std::to_string(halfmove);s+=' ';s+=std::to_string(fullmove);return s;
}
std::string Board::key() const {
    // FIDE repetition treats e.p. rights as equal when no *legal* e.p. capture exists.
    Board normalized=*this;
    if(ep>=0){
        bool possible=false;
        const int targetFile=file(ep), originRank=rank(ep)+(white?-1:1);
        for(int delta : {-1,1}){
            int fromFile=targetFile+delta;
            if(!inside(fromFile,originRank))continue;
            int start=at(fromFile,originRank);
            if(squares[start]!=pawnFor(white))continue;
            Board tmp=*this;
            if(tmp.apply({start,ep,0})&&!tmp.inCheck(white)){possible=true;break;}
        }
        if(!possible)normalized.ep=-1;
    }
    std::string f=normalized.fen();
    int separators=0;
    for(std::size_t i=0;i<f.size();++i){if(f[i]==' '&&++separators==4)return f.substr(0,i);}
    return f;
}
bool Board::isAttacked(int sq,bool byWhite) const{
    const int f=file(sq),r=rank(sq);
    // Pawn attackers originate one rank behind their target (for white).
    const int fromRank=r+(byWhite?-1:1);
    for(int df : {-1,1}) if(inside(f+df,fromRank)&&squares[at(f+df,fromRank)]==pawnFor(byWhite))return true;
    for(const auto& step:knightSteps){int x=f+step[0],y=r+step[1];if(inside(x,y)&&owns(squares[at(x,y)],byWhite)&&kind(squares[at(x,y)])=='n')return true;}
    for(const auto& step:kingSteps){int x=f+step[0],y=r+step[1];if(inside(x,y)&&owns(squares[at(x,y)],byWhite)&&kind(squares[at(x,y)])=='k')return true;}
    for(int d=0;d<8;++d){const auto& step=d<4?bishopSteps[d]:rookSteps[d-4];int x=f+step[0],y=r+step[1];while(inside(x,y)){
        char p=squares[at(x,y)];if(p){if(owns(p,byWhite)&&(kind(p)=='q'||(d<4?kind(p)=='b':kind(p)=='r')))return true;break;}
        x+=step[0];y+=step[1];
    }}return false;
}
bool Board::inCheck(bool w) const{
    auto it=std::find(squares.begin(),squares.end(),w?'K':'k');
    return it==squares.end()||isAttacked(int(it-squares.begin()),!w);
}
std::vector<Move> Board::legalMoves() const{
    std::vector<Move> pseudo;pseudo.reserve(80);
    for(int s=0;s<64;++s){char p=squares[s];if(!owns(p,white))continue;
        int f=file(s),r=rank(s);char k=kind(p);
        if(k=='p'){
            int dr=white?1:-1,nr=r+dr;
            if(inside(f,nr)&&!squares[at(f,nr)]){
                pushPawn(pseudo,s,at(f,nr),white);
                int n2=r+2*dr;if(r==(white?1:6)&&!squares[at(f,n2)])pseudo.push_back({s,at(f,n2),0});
            }
            for(int df : {-1,1}){int nf=f+df;if(!inside(nf,nr))continue;
                int t=at(nf,nr);
                if(enemy(squares[t],white))pushPawn(pseudo,s,t,white);
                if(ep==t&&!squares[t]&&squares[at(nf,r)]==pawnFor(!white))pseudo.push_back({s,t,0});
            }
        }else if(k=='n'||k=='k'){
            const int (*steps)[2] = k=='n'?knightSteps:kingSteps;
            for(int d=0;d<8;++d){int nf=f+steps[d][0],nr=r+steps[d][1];if(inside(nf,nr)&&!owns(squares[at(nf,nr)],white)&&kind(squares[at(nf,nr)])!='k')pseudo.push_back({s,at(nf,nr),0});}
            if(k=='k' && !inCheck(white) && f==4 && r==(white?0:7)){
                int cr=white?0:7;
                if((castle&(white?1:4))&&squares[at(7,cr)]==(white?'R':'r')&&!squares[at(5,cr)]&&!squares[at(6,cr)]&&!isAttacked(at(5,cr),!white)&&!isAttacked(at(6,cr),!white))pseudo.push_back({s,at(6,cr),0});
                if((castle&(white?2:8))&&squares[at(0,cr)]==(white?'R':'r')&&!squares[at(3,cr)]&&!squares[at(2,cr)]&&!squares[at(1,cr)]&&!isAttacked(at(3,cr),!white)&&!isAttacked(at(2,cr),!white))pseudo.push_back({s,at(2,cr),0});
            }
        }else{
            for(int d=0;d<8;++d){if(k=='b'&&d>=4)continue;if(k=='r'&&d<4)continue;
                const auto& step=d<4?bishopSteps[d]:rookSteps[d-4];int nf=f+step[0],nr=r+step[1];
                while(inside(nf,nr)){int t=at(nf,nr);if(owns(squares[t],white))break;
                    if(kind(squares[t])=='k')break;
                    pseudo.push_back({s,t,0});if(squares[t])break;
                    nf+=step[0];nr+=step[1];
                }
            }
        }
    }
    std::vector<Move> legal;legal.reserve(pseudo.size());
    for(const auto& m:pseudo){Board next=*this;if(next.apply(m)&&!next.inCheck(white))legal.push_back(m);}return legal;
}
bool Board::apply(const Move& m){
    if(m.from<0||m.from>=64||m.to<0||m.to>=64||m.from==m.to)return false;
    char p=squares[m.from],taken=squares[m.to];if(!owns(p,white)||owns(taken,white)||kind(taken)=='k')return false;
    bool pawn=kind(p)=='p';
    if(m.promotion && (!pawn||rank(m.to)!=(white?7:0)||std::string("qrbn").find(m.promotion)==std::string::npos))return false;
    if(pawn&&rank(m.to)==(white?7:0)&&!m.promotion)return false;
    int oldEp=ep;ep=-1;
    if(pawn && m.to==oldEp && file(m.from)!=file(m.to) && !taken){int victim=m.to+(white?-8:8);if(squares[victim]!=pawnFor(!white))return false;squares[victim]=0;taken=pawnFor(!white);}
    if(kind(p)=='k'){
        castle &= white?~3u:~12u;
        if(std::abs(file(m.to)-file(m.from))==2){
            int r=rank(m.from);if(rank(m.to)!=r)return false;
            bool kingSide=file(m.to)==6;int rookFrom=at(kingSide?7:0,r),rookTo=at(kingSide?5:3,r);
            if(squares[rookFrom]!=(white?'R':'r'))return false;
            squares[rookTo]=squares[rookFrom];squares[rookFrom]=0;
        }
    }
    if(m.from==0||m.to==0)castle&=~2u;
    if(m.from==7||m.to==7)castle&=~1u;
    if(m.from==56||m.to==56)castle&=~8u;
    if(m.from==63||m.to==63)castle&=~4u;
    squares[m.to]=m.promotion?(white?char(std::toupper(m.promotion)):m.promotion):p;
    squares[m.from]=0;
    if(pawn&&std::abs(rank(m.to)-rank(m.from))==2)ep=(m.from+m.to)/2;
    halfmove=(pawn||taken)?0:halfmove+1;
    if(!white)fullmove++;
    white=!white;return true;
}
std::string san(const Board& b,const Move& m){
    char p=b.squares[m.from];if(!p)return "?";
    bool kingCastle=kind(p)=='k'&&std::abs(file(m.to)-file(m.from))==2;
    std::string out;
    if(kingCastle)out=file(m.to)==6?"O-O":"O-O-O";
    else{
        char k=kind(p);bool capture=b.squares[m.to]||(k=='p'&&file(m.from)!=file(m.to));
        if(k!='p'){
            out+=char(std::toupper(k));bool needFile=false,needRank=false,amb=false;
            for(const Move& other:b.legalMoves()){
                if(other.to==m.to&&other.from!=m.from&&kind(b.squares[other.from])==k){amb=true;
                    if(file(other.from)==file(m.from))needRank=true;
                    if(rank(other.from)==rank(m.from))needFile=true;
                }
            }
            if(amb){if(!needRank)out+=char('a'+file(m.from));else if(!needFile)out+=char('1'+rank(m.from));else {out+=char('a'+file(m.from));out+=char('1'+rank(m.from));}}
        }else if(capture)out+=char('a'+file(m.from));
        if(capture)out+='x';
        out+=squareName(m.to);
        if(m.promotion){out+='=';out+=char(std::toupper(m.promotion));}
    }
    Board n=b;if(n.apply(m)&&n.inCheck(n.white))out+=n.legalMoves().empty()?'#':'+';return out;
}
Game::Game(){reset();}
void Game::reset(){positions={Board::initial()};moves.clear();sans.clear();}
bool Game::loadFen(const std::string& f){Board b;if(!Board::fromFen(f,b))return false;positions={b};moves.clear();sans.clear();return true;}
bool Game::play(Move m){auto available=board().legalMoves();if(std::find(available.begin(),available.end(),m)==available.end())return false;
    std::string notation=san(board(),m);Board n=board();n.apply(m);positions.push_back(n);moves.push_back(m);sans.push_back(notation);return true;
}
bool Game::playUci(const std::string& text){Move m;return parseUci(text,m)&&play(m);}
bool Game::undo(){if(moves.empty())return false;positions.pop_back();moves.pop_back();sans.pop_back();return true;}
std::string Game::result() const{
    auto legals=board().legalMoves();if(legals.empty())return board().inCheck(board().white)?(board().white?"checkmate-black":"checkmate-white"):"stalemate";
    if(board().halfmove>=100)return "fifty";
    int repetitions=0;const std::string needle=board().key();for(const auto& b:positions)if(b.key()==needle)++repetitions;
    if(repetitions>=3)return "repetition";
    // Conservative insufficient material: never declare a possible mating position a draw.
    int minors[2]={0,0},bishopColors[2]={-1,-1};
    for(int i=0;i<64;++i){char p=board().squares[i],k=kind(p);if(!p||k=='k')continue;
        if(k!='n'&&k!='b')return "ongoing";
        int side=isWhite(p)?0:1;if(++minors[side]>1)return "ongoing";
        if(k=='b')bishopColors[side]=(file(i)+rank(i))&1;
    }
    if(minors[0]==0&&minors[1]==0)return "material";
    if(minors[0]+minors[1]==1)return "material";
    if(minors[0]==1&&minors[1]==1&&bishopColors[0]>=0&&bishopColors[0]==bishopColors[1])return "material";
    return "ongoing";
}
std::string Game::pgn() const{
    std::ostringstream out;out<<"[Event \"Wenuy Schack\"]\n[Site \"WenuyOS\"]\n[Result \"";
    const auto r=result();std::string resultText=r=="checkmate-white"?"1-0":r=="checkmate-black"?"0-1":r=="ongoing"?"*":"1/2-1/2";
    out<<resultText<<"\"]\n";
    if(positions[0].fen()!=Board::initial().fen())out<<"[SetUp \"1\"]\n[FEN \""<<positions[0].fen()<<"\"]\n";
    out<<"\n";
    int moveNumber=positions[0].fullmove;bool whiteStart=positions[0].white;
    for(size_t i=0;i<sans.size();++i){bool w=((i%2)==0)==whiteStart;
        if(w)out<<moveNumber<<". ";else if(i==0)out<<moveNumber<<"... ";
        out<<sans[i]<<" ";if(!w)++moveNumber;
    }
    out<<resultText<<"\n";return out.str();
}
MoveReason explainMove(const Board& board,const Move& move){
 const auto moves=board.legalMoves();
 if(std::find(moves.begin(),moves.end(),move)==moves.end())return MoveReason::Quiet;
 const char piece=board.squares[move.from];
 const char captured=board.squares[move.to];
 Board next=board;
 if(!next.apply(move))return MoveReason::Quiet;
 if(next.inCheck(next.white)){
  if(next.legalMoves().empty())return MoveReason::Checkmate;
  return MoveReason::Check;
 }
 if(move.promotion)return MoveReason::Promotion;
 if(kind(piece)=='k'&&std::abs(file(move.to)-file(move.from))==2)return MoveReason::Castle;
 if(captured||(kind(piece)=='p'&&file(move.from)!=file(move.to)))return MoveReason::Capture;
 if((kind(piece)=='n'||kind(piece)=='b') &&
    (rank(move.from)==(board.white?0:7)))return MoveReason::Develop;
 if((file(move.to)==3||file(move.to)==4) &&
    (rank(move.to)==3||rank(move.to)==4))return MoveReason::Center;
 return MoveReason::Quiet;
}
namespace {
int evaluate(const Board& b){int score=0;
    for(int i=0;i<64;i++){char p=b.squares[i];if(!p)continue;bool w=isWhite(p);int f=file(i),r=rank(i);int cf=3-std::abs(3-f),cr=3-std::abs(3-r);int positional=0;
        switch(kind(p)){
            case 'p':positional=8*(w?r:7-r)+cf*2;break;
            case 'n':positional=6*(cf+cr);break;
            case 'b':positional=3*(cf+cr);break;
            case 'q':positional=cf+cr;break;
            default:break;
        }
        score+=(w?1:-1)*(value(p)+positional);
    }
    return b.white?score:-score;
}
int moveScore(const Board& b,const Move& m){int v=value(b.squares[m.to]);if(b.squares[m.from]=='P'||b.squares[m.from]=='p'){
        if(file(m.from)!=file(m.to)&&!b.squares[m.to])v=100;
    }
    return 10*v-value(b.squares[m.from])/10+(m.promotion?800:0);
}
// Search is deliberately bounded. An incomplete deeper iteration must never
// replace a fully searched position; this matters on low-power WenuyOS PCs.
int negamax(const Board& b,int depth,int alpha,int beta,
            uint64_t& visited,uint64_t limit,bool& exhausted){
    if(exhausted)return 0;
    if(visited>=limit){exhausted=true;return 0;}
    ++visited;
    auto legal=b.legalMoves();
    if(legal.empty())return b.inCheck(b.white)?(-30000-depth):0;
    if(b.halfmove>=100)return 0;
    if(depth<=0)return evaluate(b);
    std::stable_sort(legal.begin(),legal.end(),[&](Move a,Move c){return moveScore(b,a)>moveScore(b,c);});
    int best=-31000;
    for(const Move& m:legal){
        Board next=b;next.apply(m);
        int score=-negamax(next,depth-1,-beta,-alpha,visited,limit,exhausted);
        if(exhausted)break;
        best=std::max(score,best);alpha=std::max(alpha,score);
        if(alpha>=beta)break;
    }
    return best;
}
}
SearchResult bestMove(const Board& b,int skill,uint64_t maxNodes){
    SearchResult answer;auto ms=b.legalMoves();if(ms.empty())return answer;
    answer.move=ms.front();
    if(skill<=1){
        // Beginner: use a cheap one-ply evaluation, choosing randomly among
        // the four strongest candidates. Still fallible, but not arbitrary.
        // Avoid extra search nodes/threads on a 2 GB soft-rendering machine.
        std::stable_sort(ms.begin(),ms.end(),[&](const Move& a,const Move& c){
            Board x=b,y=b;x.apply(a);y.apply(c);
            return evaluate(x)<evaluate(y);
        });
        std::mt19937 rng(uint32_t(std::random_device{}()));
        const unsigned n=static_cast<unsigned>(std::min<std::size_t>(4,ms.size()));
        std::uniform_int_distribution<unsigned> choose(0,n-1);
        answer.move=ms[choose(rng)];return answer;
    }
    const int targetDepth=std::clamp(skill,2,5)-1+(skill>=5?1:0);
    if(!maxNodes)return answer;
    std::stable_sort(ms.begin(),ms.end(),[&](Move a,Move c){return moveScore(b,a)>moveScore(b,c);});
    // Iterative deepening means we always have a legal, fully searched move
    // even when the maximum position budget is reached in a deeper iteration.
    for(int depth=1;depth<=targetDepth;++depth){
        const auto previous=answer.move;
        std::stable_sort(ms.begin(),ms.end(),[&](Move a,Move c){
            if(a==c)return false;
            if(a==previous)return true;
            if(c==previous)return false;
            return moveScore(b,a)>moveScore(b,c);
        });
        bool exhausted=false;int bestScore=-31000,alpha=-31000;
        Move candidate=ms.front();
        for(const auto& m:ms){
            Board next=b;next.apply(m);
            const int score=-negamax(next,depth-1,-31000,-alpha,answer.nodes,maxNodes,exhausted);
            if(exhausted)break;
            if(score>bestScore){bestScore=score;candidate=m;}
            alpha=std::max(alpha,score);
        }
        if(exhausted)break;
        answer.move=candidate;answer.depth=depth;
        if(bestScore>=29900)break; // proven forced mate: no point spending more CPU
    }
    return answer;
}
}

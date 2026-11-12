#include "Chess.h"
#include "Lessons.h"
#include <cassert>
#include <iostream>
#include <string>
#include <algorithm>
#include <cstdlib>
using namespace chess;
namespace {
void test(bool ok,const char* msg){if(!ok){std::cerr<<"FAIL: "<<msg<<'\n';std::exit(1);}}
uint64_t perft(const Board& b,int depth){if(depth==0)return 1;uint64_t result=0;for(const auto& m:b.legalMoves()){Board n=b;test(n.apply(m),"apply perft");result+=perft(n,depth-1);}return result;}
}
int main(){
    std::string lessonError;test(validateLessons(&lessonError),lessonError.c_str());
    test(lessonCount()==20,"twenty beginner lessons");
    for(size_t i=0;i<lessonCount();++i){
        Board lessonBoard;test(Board::fromFen(lesson(i).fen,lessonBoard),"lesson FEN");
        Move expected;test(parseUci(lesson(i).solutionUci,expected),"lesson UCI");
        test(correctLessonMove(i,expected),"lesson accepts correct move");
        test(!correctLessonMove(i,{0,0,0}),"lesson rejects wrong move");
        const auto legal=lessonBoard.legalMoves();
        test(std::find(legal.begin(),legal.end(),expected)!=legal.end(),"lesson move legal");
    }
    // Saved progress and lesson selector only use a 64-bit mask: guard the curriculum size.
    test(lessonCount()<=64,"lesson progress capacity");
    Board b=Board::initial();test(b.fen()=="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1","initial FEN");
    test(b.legalMoves().size()==20,"20 legal start moves");
    test(perft(b,1)==20,"perft 1");test(perft(b,2)==400,"perft 2");test(perft(b,3)==8902,"perft 3");test(perft(b,4)==197281,"perft 4");
    Game g;test(g.playUci("e2e4"),"e4");test(g.board().ep==20,"en passant target");test(g.playUci("e7e5"),"e5");test(g.playUci("g1f3"),"Nf3");
    test(g.sans[2]=="Nf3","SAN knight");test(g.undo(),"undo");test(g.board().white,"undo turn");
    test(!g.playUci("e1e8"),"reject illegal");test(!g.playUci("bad move"),"reject corrupted input");
    Board parsed;test(Board::fromFen(g.board().fen(),parsed)&&parsed.fen()==g.board().fen(),"FEN roundtrip");
    test(!Board::fromFen("bad FEN",parsed),"reject bad FEN");
    test(!Board::fromFen("8/8/8/8/8/8/8/K6k w - -",parsed),"reject missing FEN fields");
    test(!Board::fromFen("8/8/8/8/8/8/8/8 w - - 0 1",parsed),"reject missing kings");
    g.reset();for(auto m:{"f2f3","e7e5","g2g4","d8h4"})test(g.playUci(m),"fool's mate");
    test(g.result()=="checkmate-black","fool's mate recognized");test(g.sans.back()=="Qh4#","mate SAN");test(g.pgn().find("0-1")!=std::string::npos,"PGN result");
    test(Board::fromFen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1",parsed),"stalemate fen");
    test(!parsed.inCheck(false)&&parsed.legalMoves().empty(),"stalemate legal moves");
    test(Board::fromFen("8/8/8/3pP3/8/8/8/K6k w - d6 0 1",parsed),"ep fen");
    auto ep=parsed.legalMoves();test(std::any_of(ep.begin(),ep.end(),[](const Move& m){return uci(m)=="e5d6";}),"en passant legal");
    test(Board::fromFen("4r2k/8/8/3pP3/8/8/8/4K3 w - d6 0 1",parsed),"ep pinned fen");
    auto pinned=parsed.legalMoves();
    test(std::none_of(pinned.begin(),pinned.end(),[](Move m){return uci(m)=="e5d6";}),"pinned ep disallowed");
    test(Board::fromFen("4kr2/8/8/8/8/8/8/4K2R w K - 0 1",parsed),"castling under attack fen");
    auto blocked=parsed.legalMoves();
    test(std::none_of(blocked.begin(),blocked.end(),[](Move m){return uci(m)=="e1g1";}),"castling through attacked f1 disallowed");
    test(Board::fromFen("7k/8/8/8/8/8/7N/K3N3 w - - 0 1",parsed),"SAN ambiguous fen");
    test(san(parsed,{4,21,0})=="Nef3","SAN knight disambiguation");
    test(Board::fromFen("4k3/P7/8/8/8/8/8/4K3 w - - 0 1",parsed),"promotion fen");
    int promos=0;for(auto m:parsed.legalMoves())if(m.from==48&&m.to==56)promos++;
    test(promos==4,"all promotion pieces");
    test(Board::fromFen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",parsed),"castle fen");
    auto castle=parsed.legalMoves();test(std::any_of(castle.begin(),castle.end(),[](Move m){return uci(m)=="e1g1";}),"castle kingside");
    test(std::any_of(castle.begin(),castle.end(),[](Move m){return uci(m)=="e1c1";}),"castle queenside");
    test(Board::fromFen("7k/8/8/8/8/8/8/K7 w - - 0 1",parsed),"material fen");
    test(g.loadFen(parsed.fen())&&g.result()=="material","bare kings draw");
    g.reset();for(auto m:{"g1f3","g8f6","f3g1","f6g8","g1f3","g8f6","f3g1","f6g8"})test(g.playUci(m),"repetition move");
    test(g.result()=="repetition","threefold repetition");
    // Explanations classify actual legal moves, without inventing tactical claims.
    Move hintMove;
    test(parseUci("e2e4",hintMove),"parse central move");
    test(explainMove(Board::initial(),hintMove)==MoveReason::Center,"central square");
    test(parseUci("g1f3",hintMove),"parse development");
    test(explainMove(Board::initial(),hintMove)==MoveReason::Develop,"knight development");
    test(parseUci("a1a8",hintMove),"parse illegal");
    test(explainMove(Board::initial(),hintMove)==MoveReason::Quiet,"illegal moves never get tactical explanations");
    Board sample;
    test(Board::fromFen("7k/8/8/8/8/8/4q3/4K3 w - - 0 1",sample),"capture test FEN");
    test(parseUci("e1e2",hintMove),"capture test move");
    test(explainMove(sample,hintMove)==MoveReason::Capture,"king capture");
    test(Board::fromFen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",sample),"castle explanation FEN");
    test(parseUci("e1g1",hintMove) && explainMove(sample,hintMove)==MoveReason::Castle,"castle explanation");
    test(Board::fromFen("4k3/P7/8/8/8/8/8/4K3 w - - 0 1",sample),"promotion explanation FEN");
    test(parseUci("a7a8q",hintMove) && explainMove(sample,hintMove)==MoveReason::Promotion,"promotion explanation");
        SearchResult sr=bestMove(Board::initial(),3,3000);test(sr.move.from>=0&&sr.nodes>0&&sr.depth>=1,"AI search");
    // Low budgets still return a legal move; incomplete iterations do not corrupt it.
    auto tiny=bestMove(Board::initial(),5,8);
    const auto initialMoves=Board::initial().legalMoves();
    test(std::find(initialMoves.begin(),initialMoves.end(),tiny.move)!=initialMoves.end(),"AI budget");
    Board mateBoard;test(Board::fromFen(lesson(14).fen,mateBoard),"queen mate setup");
    auto aiMate=bestMove(mateBoard,5,120000);
    Board afterMate=mateBoard;test(afterMate.apply(aiMate.move)&&afterMate.inCheck(false)
      &&afterMate.legalMoves().empty(),"AI finds mate in one");
    std::cout<<"OK: twenty lessons, rules, FEN, SAN, PGN, perft(1..4), castling, e.p., promotion, 50-move/draw, AI\n";
}

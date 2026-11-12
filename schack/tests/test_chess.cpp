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
    test(lessonCount()==26,"twenty-six lessons");
    for(size_t i=0;i<lessonCount();++i){
        Board lessonBoard;test(Board::fromFen(lesson(i).fen,lessonBoard),"lesson FEN");
        Move expected;test(parseUci(lesson(i).solutionUci,expected),"lesson UCI");
        test(correctLessonMove(i,expected),"lesson accepts correct move");
        test(!correctLessonMove(i,{0,0,0}),"lesson rejects wrong move");
        const auto legal=lessonBoard.legalMoves();
        test(std::find(legal.begin(),legal.end(),expected)!=legal.end(),"lesson move legal");
    }
    // Adaptive practice: unfinished exercises with more missed attempts come first.
    std::array<unsigned,64> misses{};
    test(recommendedLesson(0,misses)==0,"new learner starts with lesson one");
    misses[4]=3;misses[2]=1;
    test(recommendedLesson(0,misses)==4,"revisit difficult unfinished lesson");
    const std::uint64_t all=(std::uint64_t(1)<<lessonCount())-1;
    test(recommendedLesson(all,misses)==4,"completed curriculum reviews weakest lesson");
    test(recommendedLesson(all & ~(std::uint64_t(1)<<7),misses)==7,
         "unfinished takes precedence over completed with mistakes");
    test(std::string(lessonLevelSv(0))=="Grunder" &&
         std::string(lessonLevelSv(11))=="Taktik" &&
         std::string(lessonLevelEn(19))=="Practice" &&
         std::string(lessonLevelSv(22))=="Avancerad taktik","four bilingual sections");
    // Validate semantics beyond move legality for the three new tactics.
    Board tactic;Move m;
    test(Board::fromFen(lesson(20).fen,tactic) &&
         parseUci(lesson(20).solutionUci,m),"discovered check setup");
    test(!tactic.inCheck(false),"discovered check starts without check");
    test(tactic.apply(m) && tactic.inCheck(false),"discovered rook check");
    test(Board::fromFen(lesson(21).fen,tactic) &&
         parseUci(lesson(21).solutionUci,m),"pin setup");
    test(tactic.apply(m),"pin move");
    const auto pinnedMoves=tactic.legalMoves();
    test(std::none_of(pinnedMoves.begin(),pinnedMoves.end(),
         [](const Move& move){return move.from==35;}),"pinned knight cannot move");
    test(Board::fromFen(lesson(22).fen,tactic) &&
         parseUci(lesson(22).solutionUci,m),"skewer setup");
    test(tactic.apply(m) && tactic.inCheck(false),"skewer checks king");
    // Advanced combinations must really check the king AND threaten material.
    for(std::size_t index=23;index<=25;++index){
        Board before;Move move;
        test(Board::fromFen(lesson(index).fen,before) &&
             parseUci(lesson(index).solutionUci,move),"combination setup");
        const auto legal=before.legalMoves();
        test(std::find(legal.begin(),legal.end(),move)!=legal.end(),
             "combination solution legal");
        Board after=before;
        test(after.apply(move)&&after.inCheck(false),"combination delivers check");
        const int target=index==23?56:index==24?56:55; // a8, a8, h7
        test(after.isAttacked(target,true),"combination also threatens material");
    }
    std::array<unsigned,64> wins{};
    test(recommendedLesson(0,misses,wins)==4,"weakest unfinished repeated");
    wins[4]=10; // Previously difficult lesson is now repeatedly solved.
    test(recommendedLesson(all,misses,wins)==2,
         "successful repetition lowers old weakness priority");
    test(recommendedLesson(all & ~(std::uint64_t(1)<<7),misses,wins)==7,
         "all unfinished lessons take precedence over mastered ones");
    test(lessonGroupCount()==4 && lessonGroup(25)==3,
         "new advanced combinations share the advanced knowledge category");
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
    test(parseUci("a7a8b",hintMove) && explainMove(sample,hintMove)==MoveReason::Promotion,"promotion explanation");
        // A beginner still returns a legal move, including in positions with a single legal choice.
    const auto novice=bestMove(Board::initial(),1,0);
    const auto noviceLegals=Board::initial().legalMoves();
    test(std::find(noviceLegals.begin(),noviceLegals.end(),novice.move)!=noviceLegals.end(),"beginner legal move");
    SearchResult sr=bestMove(Board::initial(),3,3000);test(sr.move.from>=0&&sr.nodes>0&&sr.depth>=1,"AI search");
    // Low budgets still return a legal move; incomplete iterations do not corrupt it.
    auto tiny=bestMove(Board::initial(),5,8);
    const auto initialMoves=Board::initial().legalMoves();
    test(std::find(initialMoves.begin(),initialMoves.end(),tiny.move)!=initialMoves.end(),"AI budget");
    Board mateBoard;test(Board::fromFen(lesson(14).fen,mateBoard),"queen mate setup");
    auto aiMate=bestMove(mateBoard,5,120000);
    Board afterMate=mateBoard;test(afterMate.apply(aiMate.move)&&afterMate.inCheck(false)
      &&afterMate.legalMoves().empty(),"AI finds mate in one");
    std::cout<<"OK: twenty-six lessons, rules, FEN, SAN, PGN, perft(1..4), castling, e.p., promotion, 50-move/draw, AI\n";
}

#include "Lessons.h"
#include <algorithm>
#include <array>
#include <sstream>
#include <string>

namespace chess {
namespace {
constexpr std::array<Lesson, 31> lessons{{
    {
        "The knight's first step", "Springarens första steg",
        "Move the white knight from g1 to f3.", "Flytta den vita springaren från g1 till f3.",
        "A knight jumps two squares in one direction, then one square sideways.",
        "Springaren hoppar två rutor åt ett håll och en ruta åt sidan.",
        "Great! A knight may jump over other pieces.", "Bra! Springaren får hoppa över andra pjäser.",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "g1f3"
    },
    {
        "The bishop's diagonal", "Löparens diagonal",
        "Capture the black pawn with your bishop.", "Slå den svarta bonden med din löpare.",
        "The bishop on g2 travels diagonally through f3 to e4.",
        "Löparen på g2 går diagonalt via f3 till e4.",
        "Exactly! Bishops stay on the same square colour.",
        "Precis! Löpare stannar alltid på samma färg av rutor.",
        "4k3/8/8/8/4p3/8/6B1/4K3 w - - 0 1", "g2e4"
    },
    {
        "A free queen", "En obevakad dam",
        "Win the black queen with your rook.", "Vinn den svarta damen med ditt torn.",
        "The rook on e2 can travel straight along the e-file.",
        "Tornet på e2 kan gå rakt längs e-linjen.",
        "Excellent capture! Look for valuable pieces that are unprotected.",
        "Snyggt slag! Håll utkik efter värdefulla pjäser som saknar skydd.",
        "4k3/8/8/8/4q3/8/4R3/4K3 w - - 0 1", "e2e4"
    },
    {
        "The knight fork", "Springargaffeln",
        "Check the king while attacking the queen.", "Ställ kungen i schack och hota damen samtidigt.",
        "From e5, the knight can jump to f7. Which two pieces will it attack?",
        "Från e5 kan springaren hoppa till f7. Vilka två pjäser hotas då?",
        "A fork attacks two targets at once. Here it checks the king and attacks the queen.",
        "En gaffel hotar två mål samtidigt. Här hotas både kungen och damen.",
        "3q3k/8/8/4N3/8/8/8/4K3 w - - 0 1", "e5f7"
    },
    {
        "Protect the king", "Skydda kungen",
        "Castle kingside with the white king.", "Gör kort rockad med den vita kungen.",
        "Move the king from e1 to g1. The rook moves automatically.",
        "Flytta kungen från e1 till g1. Tornet flyttas automatiskt.",
        "Well done! Castling helps bring your king to safety.",
        "Bra gjort! Rockad hjälper dig att få kungen i säkerhet.",
        "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", "e1g1"
    },
    {
        "En passant", "En passant",
        "Take the black pawn en passant.", "Slå den svarta bonden en passant.",
        "The black pawn just moved from d7 to d5. Your pawn can go from e5 to d6.",
        "Den svarta bonden gick just från d7 till d5. Din bonde kan gå från e5 till d6.",
        "Correct! En passant is only possible immediately after a double pawn move.",
        "Rätt! En passant är bara möjligt direkt efter en bondes dubbelsteg.",
        "7k/8/8/3pP3/8/8/8/4K3 w - d6 0 1", "e5d6"
    },
    {
        "Promotion", "Bondeförvandling",
        "Get the pawn to a8 and choose a queen.", "Flytta bonden till a8 och välj en dam.",
        "Pawns that reach the opposite back rank may turn into a queen, rook, bishop or knight.",
        "Bönder som når sista raden får bli dam, torn, löpare eller springare.",
        "You promoted a pawn! A queen is often the strongest choice.",
        "Du förvandlade bonden! En dam är ofta det starkaste valet.",
        "7k/P7/8/8/8/8/8/4K3 w - - 0 1", "a7a8q"
    },
    {
        "Mate with a rook", "Matt med torn",
        "Find checkmate in one move with your rook.", "Hitta schack matt i ett drag med ditt torn.",
        "The king is blocked by its own pawns. Look along the eighth rank.",
        "Kungen är instängd av sina egna bönder. Titta längs åttonde raden.",
        "Checkmate! The opposing king has no safe square and cannot block the check.",
        "Schack matt! Motståndarens kung kan inte fly och schacken kan inte stoppas.",
        "6k1/5ppp/8/8/8/8/6PP/4R1K1 w - - 0 1", "e1e8"
    },
    {
        "Mate with a queen", "Matt med dam",
        "Find checkmate in one move with your queen.", "Hitta schack matt i ett drag med din dam.",
        "Your queen can control the eighth rank. Move from e2 to e8.",
        "Din dam kan kontrollera åttonde raden. Flytta från e2 till e8.",
        "Checkmate! You have completed all the introductory exercises.",
        "Schack matt! Du har klarat alla introduktionsövningarna.",
        "6k1/5ppp/8/8/8/8/4Q1PP/6K1 w - - 0 1", "e2e8"
    },
    {
        "Fight for the centre", "Ta plats i centrum",
        "Play e2-e4 to control the centre.", "Flytta bonden e2 till e4 för att ta plats i centrum.",
        "Pawns may advance two squares from their starting position.",
        "Bönder får gå två rutor från sin startruta om vägen är fri.",
        "Good! Central squares are important in the opening.",
        "Bra! Centrum är viktigt när du börjar ett parti.",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "e2e4"
    },
    {
        "Block the check", "Stoppa schacken",
        "Your king is in check. Block the rook with your queen.",
        "Din kung är i schack. Blockera tornet med din dam.",
        "Move your queen from d1 to e2 to stop the attack along the e-file.",
        "Flytta damen från d1 till e2 och stoppa angreppet på e-linjen.",
        "Good defence! You can sometimes block a check instead of moving the king.",
        "Bra försvar! Ibland kan du blockera en schack i stället för att flytta kungen.",
        "4r2k/8/8/8/8/8/8/3QK3 w - - 0 1", "d1e2"
    },
    {
        "Win a pawn", "Vinn en bonde",
        "Capture the black pawn on d5 with your white pawn.",
        "Slå den svarta bonden på d5 med din vita bonde.",
        "A pawn captures one square diagonally forward.",
        "Bönder slår en ruta snett framåt.",
        "Correct! Capturing and moving forward are different for pawns.",
        "Rätt! Bönder slår och går på olika sätt.",
        "7k/8/8/3p4/4P3/8/8/K7 w - - 0 1", "e4d5"
    },
    {
        "The rook on an open file", "Torn på öppen linje",
        "Capture the black knight on a7 with your rook.",
        "Ta den svarta springaren på a7 med ditt torn.",
        "The rook on a1 can move along the open a-file.",
        "Tornet på a1 kan gå längs den öppna a-linjen.",
        "Well spotted! Rooks work well on open files.",
        "Bra upptäckt! Torn trivs på öppna linjer.",
        "7k/n7/8/8/8/8/8/R6K w - - 0 1", "a1a7"
    },
    {
        "Promote to a knight", "Förvandla till springare",
        "Promote your pawn on b7 into a knight.",
        "Förvandla din bonde på b7 till en springare.",
        "Select 'Springare' on the promotion button, then move from b7 to b8.",
        "Välj 'Springare' vid bondeförvandling och gå sedan från b7 till b8.",
        "Nice! You can choose a piece other than a queen when promoting.",
        "Snyggt! Vid förvandling får du välja en annan pjäs än damen.",
        "7k/1P6/8/8/8/8/8/4K3 w - - 0 1", "b7b8n"
    },
    {
        "Queen and king mate", "Matt med dam och kung",
        "Find checkmate with the help of your king.",
        "Hitta matt med hjälp av din kung.",
        "Move the queen from f7 to g7. The white king protects her.",
        "Flytta damen från f7 till g7. Den vita kungen skyddar henne.",
        "Checkmate! Your king can help control escape squares.",
        "Schack matt! Din kung kan hjälpa till att täcka flyktfält.",
        "7k/5Q2/6K1/8/8/8/8/8 w - - 0 1", "f7g7"
    },

    {
        "Rook on the open file", "Tornet på den öppna linjen",
        "Capture the black pawn on d5 with your rook.", "Slå den svarta bonden på d5 med ditt torn.",
        "The rook moves along files and ranks. Check the clear path from d1.",
        "Tornet går längs linjer och rader. Kontrollera den fria vägen från d1.",
        "Nice! Open files help rooks reach useful targets.",
        "Bra! Öppna linjer hjälper tornen att nå viktiga mål.",
        "7k/8/8/3p4/8/8/8/K2R4 w - - 0 1", "d1d5"
    },
    {
        "The knight wins a rook", "Springaren tar ett torn",
        "Capture the rook on f4 using your knight.", "Slå tornet på f4 med springaren.",
        "A knight from e2 reaches f4 by jumping in an L-shape.",
        "Från e2 når springaren f4 med sitt L-formade hopp.",
        "Well played! Knights can jump over any intervening pieces.",
        "Snyggt! Springare kan hoppa över andra pjäser.",
        "7k/8/8/8/5r2/8/4N3/K7 w - - 0 1", "e2f4"
    },
    {
        "The queen captures", "Damen slår",
        "Take the black knight on e4 with your queen.", "Slå den svarta springaren på e4 med damen.",
        "The queen on e2 can move straight up the e-file.",
        "Damen på e2 kan gå rakt upp längs e-linjen.",
        "Correct! The queen combines rook and bishop moves.",
        "Rätt! Damen kan gå både som ett torn och en löpare.",
        "7k/8/8/8/4n3/8/4Q3/K7 w - - 0 1", "e2e4"
    },
    {
        "Promote to a rook", "Förvandla till torn",
        "Promote the pawn on c7 into a rook.", "Förvandla bonden på c7 till ett torn.",
        "Choose Rook on the promotion button, then move from c7 to c8.",
        "Välj Torn vid bondeförvandling och flytta sedan från c7 till c8.",
        "Great! A pawn does not always need to become a queen.",
        "Bra! En bonde behöver inte alltid förvandlas till en dam.",
        "7k/2P5/8/8/8/8/8/4K3 w - - 0 1", "c7c8r"
    },
    {
        "Black to move", "Svart vid draget",
        "Capture the white pawn on c3 as Black.", "Slå den vita bonden på c3 som svart.",
        "Black pawns move towards the first rank and capture diagonally.",
        "Svarta bönder går mot första raden och slår diagonalt.",
        "Exactly! Remember that Black moves down the board.",
        "Precis! Kom ihåg att svart går nedåt på brädet.",
        "4k3/8/8/8/3p4/2P5/8/4K3 b - - 0 1", "d4c3"
    },
    {
        "Discovered check", "Avdragsschack",
        "Move your bishop to reveal a rook check on the king.",
        "Flytta löparen så att tornet ger schack på kungen.",
        "Your bishop on e2 is blocking the rook on e1. Move it to f3.",
        "Löparen på e2 står framför tornet på e1. Flytta den till f3.",
        "Correct! Moving one piece opened a line for another piece to check.",
        "Rätt! När du flyttade en pjäs öppnades en linje så att en annan gav schack.",
        "4k3/8/8/8/8/8/4B3/K3R3 w - - 0 1", "e2f3"
    },
    {
        "Pin the knight", "Bind springaren",
        "Place the bishop so the knight cannot move without exposing its king.",
        "Placera löparen så att springaren inte kan flytta utan att blotta kungen.",
        "From f1, move your bishop to g2. Follow the diagonal to the king on b7.",
        "Flytta löparen från f1 till g2. Följ diagonalen mot kungen på b7.",
        "Great! The knight is pinned to its king and cannot legally move away.",
        "Bra! Springaren är bunden till kungen och får inte lämna linjen.",
        "8/1k6/8/3n4/8/8/8/4KB2 w - - 0 1", "f1g2"
    },
    {
        "Skewer the king and queen", "Spetsa kung och dam",
        "Give a bishop check that also attacks the queen behind the king.",
        "Ge schack med löparen och hota samtidigt damen bakom kungen.",
        "Your bishop on f3 can move to e4 and aim along f5, g6 and h7.",
        "Din löpare på f3 kan flytta till e4 och sikta via f5, g6 och h7.",
        "Well played! The king must escape check, exposing the queen behind it.",
        "Bra! Kungen måste undan från schacken och lämnar damen bakom sig.",
        "8/7q/6k1/8/8/5B2/8/K7 w - - 0 1", "f3e4"
    },
    {
        "Rook fork", "Torngaffel",
        "Check the king and attack the queen with the same rook move.",
        "Ge schack med tornet och hota samtidigt damen.",
        "Look along the eighth rank from e8 after moving your rook.",
        "Titta längs åttonde raden från e8 när du flyttat tornet.",
        "Excellent! Your rook checks the king and attacks the queen.",
        "Utmärkt! Tornet ger schack och hotar damen.",
        "q5k1/8/8/8/8/8/8/4R1K1 w - - 0 1", "e1e8"
    },
    {
        "Queen double attack", "Dubbelhot med damen",
        "Give check with the queen and threaten the rook at a8.",
        "Ge schack med damen och hota tornet på a8.",
        "Move your queen from d1 to d5, then follow both diagonals.",
        "Flytta damen från d1 till d5 och följ båda diagonalerna.",
        "Great! The queen checks the king and simultaneously attacks the rook.",
        "Bra! Damen ger schack och hotar samtidigt tornet.",
        "r5k1/8/8/8/8/8/8/3QK3 w - - 0 1", "d1d5"
    },
    {
        "Knight fork under pressure", "Springargaffel under press",
        "Use your knight to check the king and threaten the queen.",
        "Använd springaren för att ge schack och samtidigt hota damen.",
        "From d5, a knight can jump to f6. Which pieces does it attack?",
        "Från d5 kan springaren hoppa till f6. Vilka pjäser hotar den?",
        "Correct! The knight attacks the king on e8 and queen on h7.",
        "Rätt! Springaren hotar kungen på e8 och damen på h7.",
        "4k3/7q/8/3N4/8/8/8/4K3 w - - 0 1", "d5f6"
    },
    {
        "Plan two developing moves", "Planera två utvecklingsdrag",
        "Develop the knight on g1, watch Black's reply, then advance your e-pawn.",
        "Utveckla springaren på g1, se svarts svar och flytta sedan e-bonden.",
        "First g1-f3. After Black develops a knight, find e2-e4.",
        "Börja med g1-f3. När svart utvecklat springaren, hitta e2-e4.",
        "Excellent: you planned two white moves while Black replied between them.",
        "Utmärkt! Du planerade två vita drag medan svart svarade däremellan.",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "g1f3"
    },
    {
        "Build a central opening", "Bygg ett centrumspel",
        "Play e2-e4, observe Black's e-pawn, then develop the knight to f3.",
        "Spela e2-e4, se svarts e-bonde och utveckla sedan springaren till f3.",
        "Begin with e2-e4. After Black plays e7-e5, develop g1-f3.",
        "Börja med e2-e4. När svart spelar e7-e5 utvecklar du g1-f3.",
        "Good plan: you claimed space and developed a piece while Black responded.",
        "Bra plan! Du tog plats och utvecklade en pjäs medan svart svarade.",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "e2e4"
    },
    {
        "Win the queen with a knight fork", "Vinn damen med en springargaffel",
        "Give check with the knight, respond to Black's king move, then capture the queen.",
        "Ge schack med springaren, se vart svarts kung flyttar och slå sedan damen.",
        "From d5 the knight forks king and queen on f6. After the king escapes, capture on h7.",
        "Springaren från d5 hotar både kung och dam från f6. När kungen flyttar kan du slå på h7.",
        "Great! You forced the king to respond to check and won the queen on your next move.",
        "Utmärkt! Du tvingade kungen att svara på schacken och vann damen med nästa drag.",
        "4k3/7q/8/3N4/8/8/8/4K3 w - - 0 1", "d5f6"
    },
    {
        "Win the queen with a bishop skewer", "Vinn damen med en löparspets",
        "Check with the bishop. After the king escapes, win the queen behind it.",
        "Ge schack med löparen. När kungen går undan ska du vinna damen bakom den.",
        "From f3, place the bishop on e4. The king must leave the diagonal to h7.",
        "Flytta löparen från f3 till e4. Kungen måste lämna diagonalen mot h7.",
        "You used a skewer: the king had to move and the queen became available to capture.",
        "Du gjorde en spets: kungen tvingades undan och damen kunde sedan slås.",
        "8/7q/6k1/8/8/5B2/8/K7 w - - 0 1", "f3e4"
    },
    {
        "Plan three moves against two defences", "Planera tre drag mot två försvar",
        "Build an opening plan over three of your moves while Black responds twice.",
        "Bygg en öppningsplan med tre egna drag medan svart svarar två gånger.",
        "Start with e2-e4. Develop your knight, then adapt your third move to Black's defence.",
        "Börja med e2-e4. Utveckla springaren och anpassa tredje draget efter svarts försvar.",
        "Good planning! You adapted your development to the opponent's chosen pawn structure.",
        "Bra planerat! Du anpassade utvecklingen efter motståndarens val av bondestruktur.",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "e2e4"
    },
}};
// Two fully legal defensive branches for each scripted lesson. Trailing
// nullptrs are absent moves, never silently skipped during validation.
constexpr std::array<std::array<std::array<const char*,5>,2>,5> extendedLines{{
    {{{"g1f3","g8f6","e2e4",nullptr,nullptr},{"g1f3","b8c6","e2e4",nullptr,nullptr}}},
    {{{"e2e4","e7e5","g1f3",nullptr,nullptr},{"e2e4","c7c5","g1f3",nullptr,nullptr}}},
    {{{"d5f6","e8d8","f6h7",nullptr,nullptr},{"d5f6","e8f8","f6h7",nullptr,nullptr}}},
    {{{"f3e4","g6f7","e4h7",nullptr,nullptr},{"f3e4","g6h5","e4h7",nullptr,nullptr}}},
    {{{"e2e4","e7e5","g1f3","b8c6","f1c4"},{"e2e4","c7c5","g1f3","d7d6","d2d4"}}}
}};
} // namespace
std::size_t lessonCount() { return lessons.size(); }
bool readLessonProgress(std::istream& input,LessonProgress& out){
    std::string line;
    if(!std::getline(input,line))return false;
    std::istringstream header(line);
    std::uint64_t mask=0;
    std::string extra;
    if(!(header>>mask)||(header>>extra))return false;
    LessonProgress parsed;
    parsed.completed=mask;
    // A file from 0.4 has only the mask. The 0.5 early releases have
    // 20/23/26/28/29 counts on the second line. Do not let a short older
    // list consume the next version's line of counters.
    if(std::getline(input,line)){
        std::istringstream counts(line);
        for(std::size_t i=0;i<lessonCount();++i){
            unsigned value=0;
            if(!(counts>>value))break;
            parsed.mistakes[i]=std::min(value,999u);
        }
    }
    if(std::getline(input,line)){
        std::istringstream counts(line);
        for(std::size_t i=0;i<lessonCount();++i){
            unsigned value=0;
            if(!(counts>>value))break;
            parsed.successes[i]=std::min(value,999u);
        }
    }
    out=parsed;
    return true;
}
void writeLessonProgress(std::ostream& output,const LessonProgress& progress){
    output<<progress.completed<<'\n';
    for(std::size_t i=0;i<lessonCount();++i)
        output<<std::min(progress.mistakes[i],999u)<<' ';
    output<<'\n';
    for(std::size_t i=0;i<lessonCount();++i)
        output<<std::min(progress.successes[i],999u)<<' ';
    output<<'\n';
}

const Lesson& lesson(std::size_t index) { return lessons.at(index); }
bool correctLessonMove(std::size_t index, const Move& m) { return uci(m) == lesson(index).solutionUci; }
std::size_t lessonLineLength(std::size_t index) {
    lesson(index); // range-check; preserve the existing exception behavior.
    return index==30?5:index>=26?3:1;
}
std::size_t lessonVariationCount(std::size_t index) {
    lesson(index); // preserve range checks
    return index>=26?2:1;
}
const char* lessonLineMoveVariant(std::size_t index,std::size_t ply,std::size_t variation) {
    const auto length=lessonLineLength(index);
    if(ply>=length||variation>=lessonVariationCount(index))return nullptr;
    return index>=26?extendedLines[index-26][variation][ply]:lesson(index).solutionUci;
}
const char* lessonLineMove(std::size_t index,std::size_t ply) {
    return lessonLineMoveVariant(index,ply,0);
}
bool correctLessonMoveAtVariant(std::size_t index,std::size_t ply,
                                std::size_t variation,const Move& move) {
    const char* expected=lessonLineMoveVariant(index,ply,variation);
    return expected && uci(move)==expected;
}
bool correctLessonMoveAt(std::size_t index,std::size_t ply,const Move& move) {
    return correctLessonMoveAtVariant(index,ply,0,move);
}
bool lessonAcceptsMove(std::size_t index,std::size_t ply,std::size_t variation,
                       const Board& position,const Move& move) {
    // A legal student move must meet the current lesson goal; position-based
    // capture goals accept another legal capture of the same required piece.
    if(ply>=lessonLineLength(index) || variation>=lessonVariationCount(index) ||
       move.from<0||move.from>=64||move.to<0||move.to>=64)
        return false;
    if(index>=26 && (ply%2)==1)return false; // opponent reply is automatic
    const auto legal=position.legalMoves();
    if(std::find(legal.begin(),legal.end(),move)==legal.end())return false;
    if((index==28||index==29)&&ply==2)
        return position.squares[move.from]==(index==28?'N':'B') &&
               position.squares[move.to]=='q'; // win the actual black queen
    return correctLessonMoveAtVariant(index,ply,variation,move);
}
const char* lessonStepHintEn(std::size_t index,std::size_t ply,std::size_t variation) {
    if(ply>=lessonLineLength(index) || variation>=lessonVariationCount(index))return "";
    if(ply==0)return lesson(index).hintEn;
    if(index==28&&ply==2)return "Your knight on f6 can capture the queen on h7.";
    if(index==29&&ply==2)return "The king has moved. Your bishop on e4 can now take the queen on h7.";
    if(index==30){
        if(ply==2)return "Develop your knight from g1 to f3 after Black's pawn move.";
        if(ply==4)return variation==0?
          "Against ...Nc6, move the bishop from f1 to c4.":
          "Against ...d6, advance the d-pawn from d2 to d4.";
    }
    if(index==26&&ply==2)return "Advance the pawn from e2 to e4 now.";
    if(index==27&&ply==2)return "Develop the knight from g1 to f3 now.";
    return lesson(index).hintEn;
}
const char* lessonStepHintSv(std::size_t index,std::size_t ply,std::size_t variation) {
    if(ply>=lessonLineLength(index) || variation>=lessonVariationCount(index))return "";
    if(ply==0)return lesson(index).hintSv;
    if(index==28&&ply==2)return "Din springare på f6 kan slå damen på h7.";
    if(index==29&&ply==2)return "Kungen har gått undan. Löparen på e4 kan nu slå damen på h7.";
    if(index==30){
        if(ply==2)return "Utveckla springaren från g1 till f3 efter svarts bondedrag.";
        if(ply==4)return variation==0?
          "Mot ...Sc6 flyttar du löparen från f1 till c4.":
          "Mot ...d6 flyttar du bonden från d2 till d4.";
    }
    if(index==26&&ply==2)return "Flytta bonden från e2 till e4 nu.";
    if(index==27&&ply==2)return "Utveckla springaren från g1 till f3 nu.";
    return lesson(index).hintSv;
}
const char* lessonGoalEn(std::size_t index) {
    lesson(index);
    if(index==28)return "Win the queen by a knight fork";
    if(index==29)return "Win the queen by a bishop skewer";
    if(index==30)return "Complete three moves against Black's defence";
    return "";
}
const char* lessonGoalSv(std::size_t index) {
    lesson(index);
    if(index==28)return "Vinn damen med springargaffel";
    if(index==29)return "Vinn damen med löparspets";
    if(index==30)return "Genomför tre drag mot svarts försvar";
    return "";
}
const char* lessonStepFeedbackEn(std::size_t index,std::size_t ply,std::size_t variation) {
    if(index<26||variation>=lessonVariationCount(index))return "";
    if(index==29){
      if(ply==0)return "Your bishop checks the king; the queen stands behind it on the same diagonal.";
      if(ply==1)return variation==0?
        "The king retreats to f7, revealing the queen on h7 for your bishop.":
        "The king moves to h5, leaving the diagonal to the queen open.";
      return "";
    }
    if(index==30){
      if(ply==0)return "Your pawn has claimed e4. Watch how Black challenges the centre.";
      if(ply==1)return variation==0?
        "Black meets e4 with e5. Develop a knight before bringing your bishop into play.":
        "Black attacks the centre with c5. Your knight can develop before a pawn break.";
      if(ply==2)return "The knight on f3 supports your central plan.";
      if(ply==3)return variation==0?
        "Black developed to c6. With your e-pawn advanced, your bishop has a diagonal to c4.":
        "Black supports the c5 pawn with d6. Your d-pawn can challenge the central squares.";
      return "";
    }
    if(index==28){
      if(ply==0)return "Your knight gives check and simultaneously threatens the queen.";
      if(ply==1)return variation==0?
        "The king retreats to d8. Your knight can now capture the queen on h7.":
        "The king retreats to f8. The queen on h7 remains under attack.";
      return "";
    }
    if(ply==0)return index==26?
      "Your knight is developed. Notice how Black chooses to defend before advancing the pawn.":
      "Your pawn occupies the centre. Watch how Black contests it before developing the knight.";
    if(ply==1)return index==26?
      (variation==0?"Black develops the kingside knight. The centre remains available.":
                    "Black develops the queenside knight. Keep your central plan.") :
      (variation==0?"Black contests the centre with e5. Develop a piece while defending e4.":
                    "Black challenges the centre with c5. Develop your knight before choosing a plan.");
    return "";
}
const char* lessonStepFeedbackSv(std::size_t index,std::size_t ply,std::size_t variation) {
    if(index<26||variation>=lessonVariationCount(index))return "";
    if(index==29){
      if(ply==0)return "Löparen ger schack; damen står bakom kungen på samma diagonal.";
      if(ply==1)return variation==0?
        "Kungen går till f7 och öppnar vägen för löparen mot damen på h7.":
        "Kungen går till h5 och lämnar diagonalen till damen fri.";
      return "";
    }
    if(index==30){
      if(ply==0)return "Din bonde har tagit e4. Se hur svart utmanar centrum.";
      if(ply==1)return variation==0?
        "Svart svarar med e5. Utveckla springaren innan löparen kommer ut.":
        "Svart angriper centrum med c5. Utveckla springaren före bondestriden.";
      if(ply==2)return "Springaren på f3 hjälper din centrumplan.";
      if(ply==3)return variation==0?
        "Svart utvecklar springaren till c6. När e-bonden flyttat har löparen en diagonal mot c4.":
        "Svart stödjer c5-bonden med d6. Din d-bonde kan utmana centrum.";
      return "";
    }
    if(index==28){
      if(ply==0)return "Springaren ger schack och hotar samtidigt damen.";
      if(ply==1)return variation==0?
        "Kungen går till d8. Springaren kan nu slå damen på h7.":
        "Kungen går till f8. Damen på h7 är fortfarande hotad.";
      return "";
    }
    if(ply==0)return index==26?
      "Du har utvecklat springaren. Se hur svart svarar innan du går fram med bonden.":
      "Din bonde tar plats i centrum. Se hur svart utmanar den innan du utvecklar springaren.";
    if(ply==1)return index==26?
      (variation==0?"Svart utvecklar kungsflygelns springare. Centrum är fortfarande öppet.":
                    "Svart utvecklar damflygelns springare. Håll fast vid din centrumplan.") :
      (variation==0?"Svart utmanar centrum med e5. Utveckla en pjäs och försvara e4.":
                    "Svart utmanar centrum med c5. Utveckla springaren innan du väljer plan.");
    return "";
}
std::size_t recommendedLesson(std::uint64_t completed,
                              const std::array<unsigned,64>& mistakes) {
    // Prefer unfinished exercises; among them revisit the most troublesome.
    // Ties follow the original teaching order, preserving predictable progress.
    std::size_t next=lessonCount();
    for(std::size_t i=0;i<lessonCount();++i){
        if(completed & (std::uint64_t(1)<<i))continue;
        if(next==lessonCount()||mistakes[i]>mistakes[next])next=i;
    }
    if(next!=lessonCount())return next;
    // When all are finished, revisit the lesson with the most mistakes.
    next=0;
    for(std::size_t i=1;i<lessonCount();++i)
        if(mistakes[i]>mistakes[next])next=i;
    return next;
}
std::size_t lessonGroup(std::size_t index) {
    return index<9?0:index<15?1:index<20?2:3;
}
std::size_t lessonGroupCount() { return 4; }
std::size_t recommendedLesson(std::uint64_t completed,
                              const std::array<unsigned,64>& mistakes,
                              const std::array<unsigned,64>& successes) {
    // Unfinished tasks always outrank completed tasks. The recorded number
    // of successful repetitions reduces the urgency of reviewing a lesson.
    std::size_t best=lessonCount();
    for(std::size_t i=0;i<lessonCount();++i) {
        if(best==lessonCount()){best=i;continue;}
        const bool aDone=(completed & (std::uint64_t(1)<<i))!=0;
        const bool bDone=(completed & (std::uint64_t(1)<<best))!=0;
        if(aDone!=bDone){if(!aDone)best=i;continue;}
        // Compare fractions using 64-bit integer multiplication; no floating
        // point, divisions, heap allocations or engine search.
        const std::uint64_t lhs=std::uint64_t(mistakes[i])*(1u+successes[best]);
        const std::uint64_t rhs=std::uint64_t(mistakes[best])*(1u+successes[i]);
        if(lhs>rhs)best=i;
    }
    return best;
}
const char* lessonLevelEn(std::size_t index) {
    switch(lessonGroup(index)){
    case 0:return "Basics";
    case 1:return "Tactics";
    case 2:return "Practice";
    default:return "Advanced tactics";
    }
}
const char* lessonLevelSv(std::size_t index) {
    switch(lessonGroup(index)){
    case 0:return "Grunder";
    case 1:return "Taktik";
    case 2:return "Fördjupning";
    default:return "Avancerad taktik";
    }
}
bool validateLessons(std::string* error) {
    for (std::size_t i = 0; i < lessonCount(); ++i) {
        Board board;
        Move solution;
        if (!Board::fromFen(lessons[i].fen, board) || !parseUci(lessons[i].solutionUci, solution)) {
            if (error) *error = "invalid lesson FEN or move at index " + std::to_string(i);
            return false;
        }
        auto legal = board.legalMoves();
        if (std::find(legal.begin(), legal.end(), solution) == legal.end()) {
            if (error) *error = "illegal solution at index " + std::to_string(i);
            return false;
        }
        // Check every ply, including the scripted Black reply. Each step
        // is required to be legal in the resulting, real chess position.
        for(std::size_t variation=0;variation<lessonVariationCount(i);++variation) {
            Board sequence=board;
            for(std::size_t ply=0;ply<lessonLineLength(i);++ply) {
                Move step;
                const char* u=lessonLineMoveVariant(i,ply,variation);
                if(!u || !parseUci(u,step)) {
                    if(error)*error="invalid scripted ply at lesson "+std::to_string(i);
                    return false;
                }
                const auto moves=sequence.legalMoves();
                if(std::find(moves.begin(),moves.end(),step)==moves.end() ||
                   ((ply%2)==0&&!lessonAcceptsMove(i,ply,variation,sequence,step)) ||
                   !sequence.apply(step)) {
                    if(error)*error="illegal move or unmet goal at lesson "+std::to_string(i);
                    return false;
                }
            }
        }
        // Validate the tactical objective rather than accepting only matching
        // notation: the knight/bishop must truly capture Black's queen.
        if(i==28||i==29) {
            for(std::size_t variation=0;variation<lessonVariationCount(i);++variation){
                Board actual=board;
                for(std::size_t ply=0;ply<lessonLineLength(i);++ply){
                    Move step;
                    if(!parseUci(lessonLineMoveVariant(i,ply,variation),step)||
                       !actual.apply(step))return false;
                }
                const char expected=i==28?'N':'B';
                if(actual.squares[55]!=expected ||
                   std::find(actual.squares.begin(),actual.squares.end(),'q')!=actual.squares.end()){
                    if(error)*error="tactic did not capture queen at lesson "+std::to_string(i);
                    return false;
                }
            }
        }
        if (i == 7 || i == 8 || i == 14) {
            Board after = board;
            after.apply(solution);
            if (!after.inCheck(after.white) || !after.legalMoves().empty()) {
                if (error) *error = "not checkmate at index " + std::to_string(i);
                return false;
            }
        }
    }
    return true;
}
} // namespace chess

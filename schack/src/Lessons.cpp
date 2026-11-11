#include "Lessons.h"
#include <algorithm>
#include <array>

namespace chess {
namespace {
constexpr std::array<Lesson, 15> lessons{{
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
}};
} // namespace
std::size_t lessonCount() { return lessons.size(); }
const Lesson& lesson(std::size_t index) { return lessons.at(index); }
bool correctLessonMove(std::size_t index, const Move& m) { return uci(m) == lesson(index).solutionUci; }
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

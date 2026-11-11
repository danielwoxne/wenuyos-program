#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace chess {
struct Move {
    int from = -1, to = -1;
    char promotion = 0; // q, r, b, n
    bool operator==(const Move& b) const { return from==b.from && to==b.to && promotion==b.promotion; }
};
struct Board {
    std::array<char,64> squares{};  // a1=0, h8=63; uppercase=white
    bool white = true;
    unsigned castle = 15; // 1 K, 2 Q, 4 k, 8 q
    int ep = -1;
    int halfmove = 0, fullmove = 1;
    static Board initial();
    static bool fromFen(const std::string& fen, Board& out);
    std::string fen() const;
    std::string key() const; // repetition key (position, side, castling, e.p.)
    bool isAttacked(int sq, bool byWhite) const;
    bool inCheck(bool whiteKing) const;
    std::vector<Move> legalMoves() const;
    bool apply(const Move& m); // caller ensures m is legal
};
std::string squareName(int sq);
std::string uci(const Move& m);
bool parseUci(const std::string& text, Move& m);
std::string san(const Board& b, const Move& m);

struct Game {
    std::vector<Board> positions;
    std::vector<Move> moves;
    std::vector<std::string> sans;
    Game();
    const Board& board() const { return positions.back(); }
    void reset();
    bool loadFen(const std::string& fen);
    bool play(Move m);
    bool playUci(const std::string& text);
    bool undo();
    std::string result() const; // ongoing, checkmate-white, checkmate-black, stalemate, fifty, repetition, material
    std::string pgn() const;
};
struct SearchResult { Move move; uint64_t nodes=0; int depth=0; };
SearchResult bestMove(const Board& board, int skill, uint64_t maxNodes = 70000);
}

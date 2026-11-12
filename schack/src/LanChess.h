#pragma once
// An explicitly opt-in, local-network-only chess transport. Never starts sockets on construction.
#include "Chess.h"
#include <cstdint>
#include <atomic>
#include <string>

namespace chess::lan {
constexpr std::uint16_t defaultPort = 47777;

// Restrict connection targets to RFC1918/link-local/loopback IPv4 by default.
bool localIPv4(const std::string& address);

// One TCP peer, one chess game. Protocol: "WCHESS/1" then MOVE <ply> <uci>.
// A received move is validated with the same chess rules as a local move.
class Peer {
public:
    Peer() = default;
    Peer(const Peer&) = delete;
    Peer& operator=(const Peer&) = delete;
    ~Peer();

    // Both calls are blocking and require direct user action in the GUI.
    bool host(std::uint16_t port, int timeoutMs, std::string* error = nullptr,
              const std::atomic<bool>* cancel = nullptr);
    bool join(const std::string& address, std::uint16_t port, int timeoutMs,
              std::string* error = nullptr);
    void close();
    bool connected() const { return socket_ >= 0; }
    bool hostSide() const { return host_; }
    bool myTurn() const { return connected() && game_.board().white == host_; }
    const Game& game() const { return game_; }
    bool sendMove(const std::string& uci, std::string* error = nullptr);
    enum class Incoming { Waiting, Move, Closed };
    Incoming pollMove(std::string& notation, std::string* error = nullptr);
    bool receiveMove(int timeoutMs, std::string* error = nullptr);
private:
    int socket_ = -1;
    bool host_ = false;
    std::string incoming_;
    bool peerClosed_ = false;
    Game game_;
    bool begin(int fd, bool hosting, int timeoutMs, std::string* error);
};
} // namespace chess::lan

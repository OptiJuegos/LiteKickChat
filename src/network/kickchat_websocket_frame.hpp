#ifndef KICKCHAT_WEBSOCKET_FRAME_HPP
#define KICKCHAT_WEBSOCKET_FRAME_HPP

#include <cstddef>
#include <string>

namespace websocketFrame {

enum Result { Invalid = -1, NeedMore = 0, Text = 1, Ping = 2, Pong = 3, Closed = 4 };

bool encode(unsigned opcode, const std::string &payload,
            const unsigned char mask[4], std::string &frame);

class Decoder {
    std::string pending, fragment;
    bool fragmenting;
public:
    Decoder() : fragmenting(false) {}
    bool append(const std::string &data);
    Result next(std::string &payload, unsigned &closeCode);
};

} // namespace websocketFrame

#endif

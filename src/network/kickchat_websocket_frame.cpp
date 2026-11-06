#include "kickchat_websocket_frame.hpp"
#include <stdint.h>

namespace websocketFrame {

static const size_t MaximumMessage = 1024 * 1024;
static const size_t MaximumPending = 2 * MaximumMessage;

bool encode(unsigned opcode, const std::string &payload,
            const unsigned char mask[4], std::string &frame) {
    frame.clear();
    if (!mask || payload.size() > MaximumMessage ||
        (opcode != 1 && opcode != 8 && opcode != 9 && opcode != 10) ||
        (opcode >= 8 && payload.size() > 125)) return false;
    frame.reserve(payload.size() + 14);
    frame += static_cast<char>(0x80 | opcode);
    const uint64_t n = payload.size();
    if (n <= 125) frame += static_cast<char>(0x80 | n);
    else if (n <= 65535) {
        frame += static_cast<char>(0x80 | 126);
        frame += static_cast<char>(n >> 8);
        frame += static_cast<char>(n);
    } else {
        frame += static_cast<char>(0x80 | 127);
        for (int i = 7; i >= 0; --i) frame += static_cast<char>(n >> (i * 8));
    }
    frame.append(reinterpret_cast<const char *>(mask), 4);
    for (size_t i = 0; i < payload.size(); ++i)
        frame += static_cast<char>(payload[i] ^ mask[i % 4]);
    return true;
}

bool Decoder::append(const std::string &data) {
    if (data.size() > MaximumPending - pending.size()) return false;
    pending += data;
    return true;
}

Result Decoder::next(std::string &payload, unsigned &closeCode) {
    payload.clear();
    closeCode = 0;
    while (pending.size() >= 2) {
        const unsigned char first = static_cast<unsigned char>(pending[0]);
        const unsigned char second = static_cast<unsigned char>(pending[1]);
        const bool fin = (first & 0x80) != 0;
        const unsigned opcode = first & 0x0f;
        const bool masked = (second & 0x80) != 0;
        if ((first & 0x70) != 0) return Invalid;
        uint64_t length = second & 0x7f;
        size_t header = 2;
        if (length == 126) {
            if (pending.size() < 4) return NeedMore;
            length = (static_cast<unsigned char>(pending[2]) << 8) |
                      static_cast<unsigned char>(pending[3]);
            header = 4;
        } else if (length == 127) {
            if (pending.size() < 10) return NeedMore;
            if ((static_cast<unsigned char>(pending[2]) & 0x80) != 0) return Invalid;
            length = 0;
            for (int i = 2; i < 10; ++i)
                length = (length << 8) | static_cast<unsigned char>(pending[i]);
            header = 10;
        }
        if (length > MaximumMessage || (opcode >= 8 && (!fin || length > 125)))
            return Invalid;
        const size_t maskBytes = masked ? 4 : 0;
        const size_t total = header + maskBytes + static_cast<size_t>(length);
        if (pending.size() < total) return NeedMore;
        std::string data = pending.substr(header + maskBytes, static_cast<size_t>(length));
        if (masked) {
            for (size_t i = 0; i < data.size(); ++i)
                data[i] ^= pending[header + (i % 4)];
        }
        pending.erase(0, total);
        if (opcode == 8) {
            if (data.size() == 1) return Invalid;
            if (data.size() >= 2)
                closeCode = (static_cast<unsigned char>(data[0]) << 8) |
                            static_cast<unsigned char>(data[1]);
            return Closed;
        }
        if (opcode == 9) { payload.swap(data); return Ping; }
        if (opcode == 10) return Pong;
        if (opcode == 1) {
            if (fragmenting) return Invalid;
            if (fin) { payload.swap(data); return Text; }
            fragment.swap(data);
            fragmenting = true;
            continue;
        }
        if (opcode == 0) {
            if (!fragmenting || data.size() > MaximumMessage - fragment.size())
                return Invalid;
            fragment += data;
            if (fin) {
                payload.swap(fragment);
                fragment.clear();
                fragmenting = false;
                return Text;
            }
            continue;
        }
        if (opcode != 2 || fragmenting) return Invalid;
    }
    return NeedMore;
}

} // namespace websocketFrame

// SPDX-License-Identifier: GPL-3.0-or-later
// ELRSChat modifications: 2026-09-19. See NOTICE.md in the repository root.
#pragma once
#include "ChatProtocol.h"
#include <string.h>

namespace chat {
// A device-local CRSF STRING parameter. No new global CRSF frame/command ID.
// Binary local messages are base64 strings; the RF protocol is unchanged.
class Mailbox {
public:
    static constexpr uint8_t Read = 0x2c, Write = 0x2d, Entry = 0x2b;
    static constexpr uint8_t MaxValue = 56, MaxCommand = LocalHeader + 1 + ChunkSize;
    static const char *name() { return "ELRS Chat"; }
    uint8_t field = 0;
    char value[MaxValue + 1] = "v1";

    bool addressed(uint8_t type, const uint8_t *p, size_t n) const {
        return field && (type == Read || type == Write) && n >= 4
            && p[0] == 0xee && p[1] == 0xef && p[2] == field;
    }
    static bool command(const uint8_t *p, uint8_t n) {
        if (n < LocalHeader || p[0] != 'C' || p[1] != 'H'
            || p[2] != LocalVersion || !get32(p + 4)) return false;
        switch (Op(p[3])) {
        case Op::Enter: return n == LocalHeader || n == LocalHeader + 5;
        case Op::Poll: case Op::Exit: case Op::Commit: case Op::Release:
            return n == LocalHeader;
        case Op::Begin: case Op::Read: return n == LocalHeader + 1;
        case Op::Data: return n >= LocalHeader + 2 && n <= MaxCommand;
        default: return false;
        }
    }
    static uint8_t encode(const uint8_t *p, uint8_t n, char *out) {
        static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        uint8_t j = 0;
        for (uint8_t i = 0; i < n; i += 3) {
            uint32_t v = uint32_t(p[i]) << 16;
            if (i + 1 < n) v |= uint32_t(p[i + 1]) << 8;
            if (i + 2 < n) v |= p[i + 2];
            out[j++] = alphabet[(v >> 18) & 63]; out[j++] = alphabet[(v >> 12) & 63];
            out[j++] = i + 1 < n ? alphabet[(v >> 6) & 63] : '=';
            out[j++] = i + 2 < n ? alphabet[v & 63] : '=';
        }
        out[j] = 0;
        return j;
    }
    static uint8_t decode(const uint8_t *s, uint8_t n, uint8_t *out, uint8_t capacity) {
        if (!n || n % 4 || n > MaxValue) return 0;
        uint8_t used = 0;
        for (uint8_t i = 0; i < n; i += 4) {
            uint32_t v = 0;
            uint8_t pad = 0;
            for (uint8_t j = 0; j < 4; ++j) {
                const uint8_t c = s[i + j]; int b;
                if (c >= 'A' && c <= 'Z') b = c - 'A';
                else if (c >= 'a' && c <= 'z') b = c - 'a' + 26;
                else if (c >= '0' && c <= '9') b = c - '0' + 52;
                else if (c == '+') b = 62;
                else if (c == '/') b = 63;
                else if (c == '=' && i + 4 == n && j >= 2) { b = 0; ++pad; }
                else return 0;
                if (pad && c != '=') return 0;
                v = (v << 6) | unsigned(b);
            }
            if ((pad == 1 && (v & 0xff)) || (pad == 2 && (v & 0xffff))) return 0;
            if (used + 3 - pad > capacity) return 0;
            out[used++] = v >> 16;
            if (pad < 2) out[used++] = v >> 8;
            if (!pad) out[used++] = v;
        }
        return used;
    }
    uint8_t request(const uint8_t *p, uint8_t n, uint8_t *out) const {
        // Payload includes destination, origin, field, and a terminated STRING.
        if (n < 5 || n > MaxValue + 4 || p[n - 1] != 0) return 0;
        const uint8_t count = decode(p + 3, n - 4, out, MaxCommand);
        return command(out, count) ? count : 0;
    }
    void reply(const uint8_t *p, uint8_t n) {
        if (n <= 42) encode(p, n, value);
    }
    uint8_t read(uint8_t chunk, uint8_t packetLimit, uint8_t *out) {
        // Parameter snapshots must not change between chunks, including retries.
        if (packetLimit < 12 || packetLimit > 64) return 0;
        if (!chunk) { memcpy(snapshot_, value, sizeof(value)); chunkMax_ = packetLimit - 8; }
        if (!chunkMax_ || packetLimit - 8 != chunkMax_) return 0;
        uint8_t data[2 + 10 + MaxValue + 2] = {0, 0x8a}; // root, hidden STRING
        memcpy(data + 2, name(), 10);
        const uint8_t len = uint8_t(strlen(snapshot_));
        memcpy(data + 12, snapshot_, len + 1);
        data[13 + len] = MaxValue; // CRSF STRING maximum length
        const uint8_t total = 14 + len;
        const uint16_t offset = uint16_t(chunk) * chunkMax_;
        if (offset >= total) return 0;
        const uint8_t count = total - offset < chunkMax_ ? total - offset : chunkMax_;
        out[0] = 0xef; out[1] = 0xee; out[2] = field;
        out[3] = (total + chunkMax_ - 1) / chunkMax_ - chunk - 1;
        memcpy(out + 4, data + offset, count);
        return count + 4;
    }
private:
    char snapshot_[MaxValue + 1] = {};
    uint8_t chunkMax_ = 0;
};

// Both caller and consumer run on the handset/loop core. RF callers additionally
// use a critical section; no blocking RTOS mutex is needed on the normal RC path.
template <typename T, uint8_t Capacity> class Queue {
public:
    inline __attribute__((always_inline)) bool push(const T &item) {
        if (count_ == Capacity) return false;
        data_[(head_ + count_) % Capacity] = item; ++count_; return true;
    }
    inline __attribute__((always_inline)) bool pop(T &item) {
        if (!count_) return false;
        item = data_[head_]; head_ = (head_ + 1) % Capacity; --count_; return true;
    }
    void clear() { head_ = count_ = 0; }
private:
    T data_[Capacity] = {};
    volatile uint8_t head_ = 0, count_ = 0;
};
}

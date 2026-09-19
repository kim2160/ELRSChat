#include "ChatProtocol.h"
#include <string.h>

namespace chat {
uint16_t get16(const uint8_t *p) { return uint16_t(p[0]) | uint16_t(p[1]) << 8; }
uint32_t get32(const uint8_t *p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
void put16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
void put32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) p[i] = v >> (8 * i);
}
uint16_t crc16(const uint8_t *p, size_t n) {
    uint16_t crc = 0xffff; // CRC-16/CCITT-FALSE: poly 1021, init FFFF, xorout 0.
    while (n--) {
        crc ^= uint16_t(*p++) << 8;
        for (unsigned i = 0; i < 8; ++i)
            crc = (crc & 0x8000) ? uint16_t((crc << 1) ^ 0x1021) : uint16_t(crc << 1);
    }
    return crc;
}
bool validText(const uint8_t *p, size_t n) {
    if (!n || n > MaxText) return false;
    size_t i = 0;
    while (i < n) {
        uint8_t c = p[i++];
        if (c >= 0x20 && c <= 0x7e) continue;
        unsigned extra;
        uint32_t value, minimum;
        if (c >= 0xc2 && c <= 0xdf) { extra = 1; value = c & 0x1f; minimum = 0x80; }
        else if (c >= 0xe0 && c <= 0xef) { extra = 2; value = c & 0x0f; minimum = 0x800; }
        else if (c >= 0xf0 && c <= 0xf4) { extra = 3; value = c & 7; minimum = 0x10000; }
        else return false;
        if (i + extra > n) return false;
        while (extra--) {
            c = p[i++];
            if ((c & 0xc0) != 0x80) return false;
            value = (value << 6) | (c & 0x3f);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)
            || (value >= 0x80 && value <= 0x9f)) return false;
    }
    return true;
}
bool encode(const Message &m, uint8_t profile, uint8_t *out) {
    if (!validText(m.text, m.length) || !m.sequence) return false;
    memset(out, 0, PacketSize);
    out[0] = 'E'; out[1] = 'C'; out[2] = Version; out[3] = profile;
    memcpy(out + 4, m.sender, 8);
    put32(out + 12, m.session); put32(out + 16, m.sequence);
    out[20] = 0; // Text only. Presets are sent as their readable text.
    out[21] = m.length;
    memcpy(out + 22, m.text, m.length);
    put16(out + 54, crc16(out, 54));
    return true;
}
bool decode(const uint8_t *data, size_t n, uint8_t profile, Message &m) {
    if (n != PacketSize || data[0] != 'E' || data[1] != 'C' || data[2] != Version
        || data[3] != profile || data[20] != 0 || data[21] > MaxText
        || get16(data + 54) != crc16(data, 54) || !get32(data + 16)
        || !validText(data + 22, data[21])) return false;
    for (size_t i = 22 + data[21]; i < 54; ++i) if (data[i]) return false;
    memcpy(m.sender, data + 4, 8); m.session = get32(data + 12);
    m.sequence = get32(data + 16); m.length = data[21];
    memset(m.text, 0, sizeof(m.text)); memcpy(m.text, data + 22, m.length);
    return true;
}
}

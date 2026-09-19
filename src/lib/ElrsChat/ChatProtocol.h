// SPDX-License-Identifier: GPL-3.0-or-later
// ELRSChat modifications: 2026-09-19. See NOTICE.md in the repository root.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace chat {
constexpr uint8_t Version = 1;
// Local v2 requires explicit entry; v1 Lua automatically entered on launch.
constexpr uint8_t LocalVersion = 2;
constexpr uint8_t MaxText = 32;
constexpr uint8_t PacketSize = 56;
constexpr uint8_t LocalHeader = 10;
constexpr uint8_t ChunkSize = 16;
constexpr uint8_t RecordSize = PacketSize + 2;
constexpr uint8_t RxCapacity = 4;

enum class Op : uint8_t {
    Poll = 1, Enter = 2, Exit = 3, Begin = 4, Data = 5, Commit = 6,
    Read = 7, Release = 8, Wifi = 9, Status = 0x81, Record = 0x87
};
enum class State : uint8_t { Idle, Receiving, Staging, Queued, Transmitting };
enum class Result : uint8_t {
    None, Assembling, Queued, Sending, Sent, Cancelled, Invalid,
    Incomplete, Busy, Expired, RadioError, Unknown, RateLimited,
    NoProfile, Unsupported, Inactive, Stale, Armed, RcBusy
};
enum class Mode : uint8_t { Normal, Chat, Unavailable };
struct Message {
    uint8_t sender[8] = {};
    uint32_t session = 0;
    uint32_t sequence = 0;
    uint8_t length = 0;
    uint8_t text[MaxText] = {};
};
uint16_t get16(const uint8_t *p);
uint32_t get32(const uint8_t *p);
void put16(uint8_t *p, uint16_t v);
void put32(uint8_t *p, uint32_t v);
uint16_t crc16(const uint8_t *p, size_t n);
bool validText(const uint8_t *p, size_t n);
bool encode(const Message &m, uint8_t profile, uint8_t *out);
bool decode(const uint8_t *data, size_t n, uint8_t profile, Message &m);
}

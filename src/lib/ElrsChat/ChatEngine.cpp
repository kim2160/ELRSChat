#include "ChatEngine.h"
#include <string.h>

namespace chat {
static bool due(uint32_t now, uint32_t when) { return int32_t(now - when) >= 0; }
Engine::Engine(Port &port, const Profile &profile, const uint8_t *sender, uint32_t boot)
    : port_(port), profile_(profile), boot_(boot) { memcpy(sender_, sender, 8); }

void Engine::status(uint32_t client, uint16_t req, Result replyResult) {
    uint8_t out[42] = {'C', 'H', LocalVersion, uint8_t(Op::Status)};
    put32(out + 4, client); put16(out + 8, req);
    put32(out + 10, boot_); memcpy(out + 14, sender_, 8);
    out[22] = uint8_t(state_); out[23] = uint8_t(replyResult);
    out[24] = MaxText; out[25] = profile_.id;
    put32(out + 26, profile_.frequency); out[30] = profile_.power;
    put16(out + 31, request_); put16(out + 33, rxCount_ ? rx_[rxHead_].id : 0);
    out[35] = rxCount_; put16(out + 36, dropped_);
    put16(out + 38, highWater_);
    out[40] = client == client_ ? 1 : 0;
    out[41] = uint8_t(port_.mode());
    port_.reply(out, sizeof(out));
}
void Engine::finish(Result result) {
    result_ = result; used_ = total_ = 0;
    if (stopAfterTx_ || !active_) {
        active_ = false; stopAfterTx_ = false; state_ = State::Idle; port_.standby();
        port_.endSession();
    } else {
        state_ = State::Receiving;
        if (!port_.startReceive()) {
            active_ = false; state_ = State::Idle; result_ = Result::RadioError;
            port_.standby(); port_.endSession();
        }
    }
}
void Engine::stop(Result result) {
    active_ = false;
    if (state_ == State::Transmitting) {
        // An RF packet already started cannot be reported as cancelled/not sent.
        stopAfterTx_ = true;
        return;
    }
    finish(result);
}
void Engine::disconnect() { stop(Result::Expired); }

void Engine::command(const uint8_t *p, uint8_t n, uint32_t now) {
    if (n < LocalHeader || p[0] != 'C' || p[1] != 'H' || p[2] != LocalVersion) return;
    const Op op = Op(p[3]);
    const uint32_t client = get32(p + 4);
    const uint16_t req = get16(p + 8);
    if (!client) return;
    const uint8_t *data = p + LocalHeader;
    const uint8_t len = n - LocalHeader;
    // Expire stale sessions, but do not launch RF before processing an Exit.
    expire(now);
    if (op == Op::Enter && (len == 0 || len == 5)) {
        Profile requested = profile_;
        if (len) {
            // Lua uses integer kHz: exact even with EdgeTX int32/float32.
            const uint32_t khz = get32(data);
            if (!khz || khz > UINT32_MAX / 1000 || data[4] > 7) {
                status(client, req, Result::Invalid); return;
            }
            requested.frequency = khz * 1000; requested.power = data[4];
        }
        if (client != client_) {
            if (active_ || state_ == State::Transmitting) { status(client, req); return; }
            client_ = client; highWater_ = request_ = 0; result_ = Result::None;
        }
        if (len && (active_ || state_ == State::Transmitting)
            && (requested.frequency != profile_.frequency || requested.power != profile_.power)) {
            status(client, req, Result::Busy); return; // Never retune a live session.
        }
        if (!active_ && state_ != State::Transmitting) {
            if (len) {
                const Result selected = port_.selectProfile(requested);
                if (selected != Result::None) { result_ = selected; status(client, req); return; }
                if (requested.frequency != profile_.frequency || requested.id != profile_.id) {
                    // Never label pending records from an old channel as new arrivals.
                    rxHead_ = rxCount_ = 0; seenCount_ = seenPos_ = 0;
                }
                profile_ = requested;
            }
            if (!profile_.frequency) result_ = Result::NoProfile;
            else if (!port_.startReceive()) result_ = port_.receiveFailure();
            else { active_ = true; state_ = State::Receiving; result_ = Result::None; }
        }
        lease_ = now;
        status(client, req); return;
    }
    if (op == Op::Poll && !len) {
        if (client == client_ && active_) lease_ = now;
        status(client, req); return;
    }
    if (client != client_) return;
    // Only an explicit Enter can re-enable RF after lease expiry.
    if (op == Op::Exit && !len) { stop(Result::Cancelled); status(client, req); return; }
    if (active_) lease_ = now;
    if (op == Op::Read && len == 1) {
        if (!rxCount_ || rx_[rxHead_].id != req || data[0] >= RecordSize) return;
        const uint8_t offset = data[0];
        const uint8_t count = RecordSize - offset > ChunkSize ? ChunkSize : RecordSize - offset;
        uint8_t out[LocalHeader + 2 + ChunkSize] = {'C','H',LocalVersion,uint8_t(Op::Record)};
        put32(out + 4, client); put16(out + 8, req);
        out[10] = offset; out[11] = RecordSize;
        memcpy(out + 12, rx_[rxHead_].bytes + offset, count);
        port_.reply(out, LocalHeader + 2 + count); return;
    }
    if (op == Op::Release && !len) {
        if (rxCount_ && rx_[rxHead_].id == req) { rxHead_ = (rxHead_ + 1) % RxCapacity; --rxCount_; }
        return;
    }
    if (op == Op::Begin && len == 1) {
        // Strictly increasing local IDs ensure a repeated Begin/Commit cannot cause another RF send.
        if (!req || req <= highWater_) { status(client, req); return; }
        if (!active_) { result_ = Result::Inactive; status(client, req); return; }
        if (state_ != State::Receiving) { status(client, req); return; }
        request_ = highWater_ = req;
        if (!data[0] || data[0] > MaxText) { result_ = Result::Invalid; status(client, req); return; }
        if (haveSent_ && now - lastSent_ < profile_.minIntervalMs) {
            result_ = Result::RateLimited; status(client, req); return;
        }
        total_ = data[0]; used_ = 0; stageTime_ = now; state_ = State::Staging;
        result_ = Result::Assembling; status(client, req); return;
    }
    if (!active_ || req != request_) { status(client, req); return; }
    if (op == Op::Data && len >= 2 && len <= ChunkSize + 1 && state_ == State::Staging) {
        const uint8_t offset = data[0], count = len - 1;
        if (unsigned(offset) + count > total_) { finish(Result::Invalid); }
        else if (offset == used_) { memcpy(staging_ + offset, data + 1, count); used_ += count; }
        else if (offset < used_ && unsigned(offset) + count <= used_
                 && !memcmp(staging_ + offset, data + 1, count)) { /* identical local duplicate */ }
        else { finish(Result::Incomplete); }
        return;
    }
    if (op == Op::Commit && !len) {
        if (state_ != State::Staging) { status(client, req); return; }
        if (used_ != total_) { finish(Result::Incomplete); status(client, req); return; }
        Message m; memcpy(m.sender, sender_, 8);
        m.session = boot_; m.sequence = ++seq_;
        m.length = total_; memcpy(m.text, staging_, total_);
        if (!encode(m, profile_.id, packet_)) finish(Result::Invalid);
        else {
            state_ = State::Queued; result_ = Result::Queued;
            queuedAt_ = now; deadline_ = now + port_.random32() % 301;
        }
        status(client, req); return;
    }
    status(client, req);
}
void Engine::expire(uint32_t now) {
    if (active_ && now - lease_ >= 5000) stop(Result::Expired);
    if (state_ == State::Staging && now - stageTime_ >= 2000) finish(Result::Incomplete);
    if (state_ == State::Queued && now - queuedAt_ >= profile_.maxQueueMs) finish(Result::Busy);
    if (state_ == State::Transmitting && due(now, deadline_)) {
        port_.standby();
        finish(Result::Unknown); // Never retry when TX-done is missing.
    }
}
void Engine::tick(uint32_t now) {
    expire(now);
    if (state_ == State::Queued) {
        if (due(now, deadline_)) {
            if (!port_.channelClear()) { deadline_ = now + 20 + port_.random32() % 181; return; }
            // Transition before calling the driver; the call is made at most once for this request.
            state_ = State::Transmitting; result_ = Result::Sending;
            deadline_ = now + profile_.txTimeoutMs;
            lastSent_ = now; haveSent_ = true;
            if (!port_.transmit(packet_)) finish(Result::RadioError);
        }
    }
}
void Engine::txDone() {
    if (state_ == State::Transmitting) finish(Result::Sent);
}
bool Engine::duplicate(const Message &m) const {
    for (uint8_t i = 0; i < seenCount_; ++i)
        if (seen_[i].boot == m.session && seen_[i].seq == m.sequence
            && !memcmp(seen_[i].sender, m.sender, 8)) return true;
    return false;
}
void Engine::received(const uint8_t *packet, int8_t rssi, int8_t snr) {
    if (!active_ || state_ == State::Transmitting) return;
    Message m;
    if (!decode(packet, PacketSize, profile_.id, m)
        || !memcmp(m.sender, sender_, 8) || duplicate(m)) return;
    Key &key = seen_[seenPos_]; memcpy(key.sender, m.sender, 8);
    key.boot = m.session; key.seq = m.sequence;
    seenPos_ = (seenPos_ + 1) % 64; if (seenCount_ < 64) ++seenCount_;
    if (rxCount_ == RxCapacity) { ++dropped_; return; }
    Record &record = rx_[(rxHead_ + rxCount_) % RxCapacity];
    if (!++nextRx_) ++nextRx_;
    record.id = nextRx_; memcpy(record.bytes, packet, PacketSize);
    record.bytes[PacketSize] = uint8_t(rssi); record.bytes[PacketSize + 1] = uint8_t(snr);
    ++rxCount_;
}
}

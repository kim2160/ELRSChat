#pragma once
#include "ChatProtocol.h"

namespace chat {
struct Profile {
    uint8_t id = 1;
    uint32_t frequency = 0; // Zero means no RF profile provisioned.
    uint8_t power = 0;
    uint32_t minIntervalMs = 1000;
    uint32_t txTimeoutMs = 2000;
    uint32_t maxQueueMs = 3000;
};
class Port {
public:
    virtual ~Port() {}
    // Validate/store temporary chat settings only; never touch RC or start RF.
    // A successful adapter also selects the compatible over-air profile ID.
    virtual Result selectProfile(Profile &profile) { (void)profile; return Result::Unsupported; }
    virtual bool startReceive() = 0;
    virtual Result receiveFailure() const { return Result::Unsupported; }
    virtual void endSession() {}
    virtual Mode mode() const { return Mode::Normal; }
    virtual void standby() = 0;
    virtual bool channelClear() = 0;
    virtual bool transmit(const uint8_t *packet) = 0;
    virtual uint32_t random32() = 0;
    virtual void reply(const uint8_t *payload, uint8_t length) = 0;
};
// All methods run on one task. ISR/other-core input is marshalled by ChatDevice.
class Engine {
public:
    Engine(Port &port, const Profile &profile, const uint8_t *sender, uint32_t boot);
    void command(const uint8_t *p, uint8_t n, uint32_t now);
    void tick(uint32_t now);
    void received(const uint8_t *packet, int8_t rssi, int8_t snr);
    void txDone();
    void disconnect();
    State state() const { return state_; }
    Result result() const { return result_; }
    uint8_t receivedCount() const { return rxCount_; }
    uint32_t client() const { return client_; }
private:
    struct Key { uint8_t sender[8]; uint32_t boot; uint32_t seq; };
    struct Record { uint16_t id; uint8_t bytes[RecordSize]; };
    Port &port_;
    Profile profile_;
    uint8_t sender_[8];
    uint32_t boot_, client_ = 0, seq_ = 0, lease_ = 0, deadline_ = 0, stageTime_ = 0;
    uint32_t queuedAt_ = 0, lastSent_ = 0;
    bool haveSent_ = false, stopAfterTx_ = false, active_ = false;
    State state_ = State::Idle;
    Result result_ = Result::None;
    uint16_t request_ = 0, highWater_ = 0;
    uint8_t total_ = 0, used_ = 0, staging_[MaxText] = {}, packet_[PacketSize] = {};
    Key seen_[64] = {};
    uint8_t seenCount_ = 0, seenPos_ = 0;
    Record rx_[RxCapacity] = {};
    uint8_t rxHead_ = 0, rxCount_ = 0;
    uint16_t nextRx_ = 0, dropped_ = 0;
    void status(uint32_t client, uint16_t request) { status(client, request, result_); }
    void status(uint32_t client, uint16_t request, Result replyResult);
    void expire(uint32_t now);
    void finish(Result result);
    void stop(Result result);
    bool duplicate(const Message &m) const;
};
}

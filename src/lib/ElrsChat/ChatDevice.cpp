#include "targets.h"
#if defined(ELRS_CHAT) && defined(TARGET_TX)
#include "ChatDevice.h"
#include "ChatEngine.h"
#include "ChatSession.h"
#include "ChatProfile.h"
#include "OTA.h"
#include "common.h"
#include "rxtx_intf.h"
#include "FHSS.h"
#include "POWERMGNT.h"
#include "CRSFRouter.h"
#include "ChatMailbox.h"
#include <new>
#include "device.h"
#if !defined(RADIO_LR1121)
#include "RFAMP_hal.h"
extern RFAMP_hal RFAMP;
#endif
#if defined(PLATFORM_ESP8266)
#include <user_interface.h>
#endif

#ifndef CHAT_FREQUENCY_HZ
#define CHAT_FREQUENCY_HZ 0
#endif
#ifndef CHAT_POWER
#define CHAT_POWER 0
#endif
#ifndef CHAT_PROFILE_ID
#if defined(RADIO_SX128X)
#define CHAT_PROFILE_ID 1
#elif defined(RADIO_SX127X)
#define CHAT_PROFILE_ID 2
#elif CHAT_FREQUENCY_HZ >= 2401000000UL
#define CHAT_PROFILE_ID 1
#else
#define CHAT_PROFILE_ID 2
#endif
#endif

bool chatDeviceScheduled = false;
namespace {
chat::Mailbox mailbox;
struct Command { uint8_t length; uint8_t bytes[chat::Mailbox::MaxCommand]; };
struct RadioEvent { uint8_t bytes[chat::PacketSize]; int8_t rssi, snr; };
struct Runtime {
    chat::Engine engine;
    chat::Queue<Command, 4> commands;
    chat::Queue<RadioEvent, 4> events;
    volatile bool txDone = false;
    bool disconnected = false;
    uint32_t lastCommand;
    Runtime(chat::Port &port, const chat::Profile &profile, const uint8_t *sender, uint32_t now)
        : engine(port, profile, sender, port.random32()), lastCommand(now) {}
};
// Handset callbacks and loop are on the same core. Only events/txDone cross ISR.
Runtime *runtime = nullptr;
bool available = false, updating = false;
chat::Profile profile;
chat::Result entryResult = chat::Result::None;
bool supportedProfile(chat::Profile &candidate);
bool configureRadio();
void stopRadio();
class RadioSessionPort : public chat::SessionPort {
public:
    chat::Result suspendRc() override {
        auto candidate = profile;
        if (updating || !supportedProfile(candidate)) return chat::Result::Unsupported;
        return chatRcSuspend();
    }
    bool configureChat() override { return configureRadio(); }
    void stopChat() override { stopRadio(); }
    void restoreRc(bool resume) override { chatRcRestore(resume); }
} sessionPort;
chat::Session session(sessionPort);

// SPI control is on the same core that attached the RF ISR in setup().
// Do not invoke engine or CRSF routing from an RF interrupt.
bool ICACHE_RAM_ATTR receiveISR(SX12xxDriverCommon::rx_status status) {
    if (!chatDeviceActive()) return false;
    if (status != SX12xxDriverCommon::SX12XX_RX_OK) return false;
    Radio.GetLastPacketStats();
    RadioEvent event;
    memcpy(event.bytes, Radio.RXdataBuffer, chat::PacketSize);
    event.rssi = int8_t(Radio.LastPacketRSSI); event.snr = int8_t(Radio.LastPacketSNRRaw);
    runtime->events.push(event);
    return true;
}
void ICACHE_RAM_ATTR transmitISR() {
    if (chatDeviceActive()) runtime->txDone = true;
}
// Caller holds the RF interrupt critical section. Preserve queued RX events.
void idleRadio() {
    Radio.SetTxIdleMode();
#if defined(RADIO_SX128X)
    Radio.ClearIrqStatus(SX1280_IRQ_RADIO_ALL, SX12XX_Radio_All);
#elif defined(RADIO_SX127X)
    Radio.ClearIrqFlags(SX12XX_Radio_All);
#else
    Radio.ClearIrqStatus(SX12XX_Radio_All);
#endif
}
class HardwarePort : public chat::Port {
public:
    chat::Result selectProfile(chat::Profile &candidate) override {
        if (!available || updating || connectionState >= wifiUpdate) return chat::Result::Unsupported;
        if (session.active()) return chat::Result::Busy;
        if (!supportedProfile(candidate)) return chat::Result::Unsupported;
        profile = candidate; // RAM only. The RC configuration is unchanged.
        return chat::Result::None;
    }
    bool startReceive() override {
        if (!available || updating || connectionState >= wifiUpdate) {
            entryResult = chat::Result::Unsupported;
            return false;
        }
        entryResult = session.enter();
        if (entryResult != chat::Result::None) return false;
        noInterrupts(); Radio.RXnb(); interrupts();
        return true;
    }
    chat::Result receiveFailure() const override { return entryResult; }
    void endSession() override { session.leave(!updating); }
    chat::Mode mode() const override {
        if (!available || updating) return chat::Mode::Unavailable;
        return session.active() ? chat::Mode::Chat : chat::Mode::Normal;
    }
    void standby() override {
        if (session.active()) stopRadio();
    }
    bool channelClear() override {
        noInterrupts();
#if defined(RADIO_SX128X)
        const int rssi = Radio.GetRssiInst(SX12XX_Radio_1);
#elif defined(RADIO_SX127X)
        const int rssi = Radio.GetCurrRSSI(SX12XX_Radio_1);
#else
        Radio.StartRssiInst(SX12XX_Radio_1);
        const int rssi = Radio.GetRssiInst(SX12XX_Radio_1);
#endif
        interrupts();
        // Collision avoidance heuristic, NOT a regulatory LBT implementation.
        return rssi < -85;
    }
    bool transmit(const uint8_t *packet) override {
        if (!available || !session.active() || updating) return false;
        noInterrupts();
        idleRadio();
        Radio.TXnb(const_cast<uint8_t *>(packet), false, nullptr, SX12XX_Radio_1);
        interrupts();
        return true;
    }
    uint32_t random32() override {
#if defined(PLATFORM_ESP32)
        return esp_random();
#else
        return os_random();
#endif
    }
    void reply(const uint8_t *bytes, uint8_t n) override { mailbox.reply(bytes, n); }
} port;

bool supportedProfile(chat::Profile &candidate) {
    // This fixed-channel waveform has not been qualified for stock regulatory
    // profiles. Inclusion of the code must not silently enable RF operation.
#if !defined(CHAT_EXPERIMENTAL_RF) || !CHAT_EXPERIMENTAL_RF || defined(Regulatory_Domain_EU_CE_2400)
    return false;
#endif
    // Only a single RF chip is supported in v1. Reject Gemini/dual rather than
    // silently using an unknown antenna/power path.
    if (!available || GPIO_PIN_NSS_2 != UNDEF_PIN) return false;
#if defined(RADIO_SX128X)
    const bool sub = false, band2400 = true;
#elif defined(RADIO_SX127X)
    const bool sub = true, band2400 = false;
#else
    const bool sub = true, band2400 = POWER_OUTPUT_VALUES_DUAL != nullptr;
#endif
    return chat::selectSupportedProfile(candidate, POWERMGNT::getMinPower(),
        POWERMGNT::getMaxPower(), sub, band2400);
}
void stopRadio() {
    if (!available || connectionState >= wifiUpdate) return;
    noInterrupts();
    idleRadio();
#if !defined(RADIO_LR1121)
    RFAMP.TXRXdisable();
#endif
    if (runtime) { runtime->events.clear(); runtime->txDone = false; }
    interrupts();
}
bool configureRadio() {
    const bool sub = profile.frequency < 1000000000UL;
    stopRadio();
    noInterrupts();
    Radio.RXdoneCallback = receiveISR; Radio.TXdoneCallback = transmitISR;
    FHSSusePrimaryFreqBand = sub;
    FHSSuseDualBand = false;
    POWERMGNT::setPower(PowerLevels_e(profile.power));
    if (POWERMGNT::currPower() != profile.power) { interrupts(); return false; }
    const uint32_t freq = FREQ_HZ_TO_REG_VAL(profile.frequency);
#if defined(RADIO_SX128X)
    Radio.Config(SX1280_LORA_BW_0800, SX1280_LORA_SF7, SX1280_LORA_CR_4_7,
                 freq, 12, false, chat::PacketSize, 0, 0, RadioBandMod::LORA_2G4);
#elif defined(RADIO_SX127X)
    Radio.Config(SX127x_BW_500_00_KHZ, SX127x_SF_9, SX127x_CR_4_7,
                 freq, 12, 0x12, false, chat::PacketSize);
#else
    Radio.Config(sub ? LR11XX_RADIO_LORA_BW_500 : LR11XX_RADIO_LORA_BW_800,
                 sub ? LR11XX_RADIO_LORA_SF9 : LR11XX_RADIO_LORA_SF7,
                 LR11XX_RADIO_LORA_CR_4_7, freq, 12, false, chat::PacketSize,
                 sub ? RadioBandMod::LORA_900 : RadioBandMod::LORA_2G4, 0, 0);
#endif
    Radio.ApplyPendingPower();
    interrupts();
    stopRadio();
    return true;
}
}

stringParameter chatParameter = {
    {chat::Mailbox::name(), crsf_value_type_e(CRSF_STRING | CRSF_FIELD_HIDDEN), 0, 0}, mailbox.value
};

void chatDeviceBegin(bool radioAvailable) {
    available = radioAvailable && GPIO_PIN_SCK != UNDEF_PIN;
    profile.id = CHAT_PROFILE_ID; profile.frequency = CHAT_FREQUENCY_HZ; profile.power = CHAT_POWER;
}
void chatDeviceDisconnected() {
    if (runtime) runtime->disconnected = true;
}
static void sendParameterFrame(uint8_t type, const uint8_t *payload, uint8_t n) {
    uint8_t bytes[64];
    memcpy(bytes + sizeof(crsf_header_t), payload, n);
    crsfRouter.SetExtendedHeaderAndCrc(reinterpret_cast<crsf_ext_header_t *>(bytes),
        crsf_frame_type_e(type), n + 2, CRSF_ADDRESS_ELRS_LUA, CRSF_ADDRESS_CRSF_TRANSMITTER);
    crsfRouter.deliverMessageTo(CRSF_ADDRESS_ELRS_LUA, reinterpret_cast<crsf_header_t *>(bytes));
}
static bool handleParameter(const crsf_header_t *frame) {
    if (frame->frame_size < 6 || frame->frame_size > 62) return false;
    const uint8_t *payload = reinterpret_cast<const uint8_t *>(frame) + sizeof(crsf_header_t);
    const uint8_t length = frame->frame_size - 2;
    mailbox.field = chatParameter.common.id;
    if (!mailbox.addressed(frame->type, payload, length)) return false;
    if (frame->type == chat::Mailbox::Read) {
        if (length != 4) return true;
        uint8_t out[60];
        const uint8_t n = mailbox.read(payload[3], crsfRouter.getConnectorMaxPacketSize(CRSF_ADDRESS_ELRS_LUA), out);
        if (n) sendParameterFrame(chat::Mailbox::Entry, out, n);
        return true;
    }
    Command command;
    command.length = mailbox.request(payload, length, command.bytes);
    if (!command.length || updating) return true;
    if (!runtime) {
        uint8_t sender[8] = {};
#if defined(PLATFORM_ESP32)
        const uint64_t mac = ESP.getEfuseMac(); memcpy(sender, &mac, 6);
#else
        wifi_get_macaddr(STATION_IF, sender);
#endif
        sender[6] = 'E'; sender[7] = 'C';
        runtime = new (std::nothrow) Runtime(port, profile, sender, millis());
        if (!runtime) return true; // No ACK: the Lua transport times out safely.
        chatDeviceScheduled = true;
    }
    if (runtime->commands.push(command)) {
        runtime->lastCommand = millis();
        memcpy(mailbox.value, payload + 3, length - 3);
        sendParameterFrame(chat::Mailbox::Write, payload, length);
    }
    return true;
}
bool chatDeviceHandleMessage(const crsf_header_t *frame) {
    // Called only for addressed extended messages, never on the raw RC path.
    if (handleParameter(frame)) return true;
    if (!chatDeviceActive()) return false;
    const auto ext = reinterpret_cast<const crsf_ext_header_t *>(frame);
    // Preserve model selection and cancel chat before returning to that model.
    if (frame->type != CRSF_FRAMETYPE_COMMAND || frame->frame_size < 7
        || ext->payload[0] != CRSF_COMMAND_SUBCMD_RX
        || ext->payload[1] != CRSF_COMMAND_MODEL_SELECT_ID) return true;
    chatDeviceDisconnected();
    return false;
}
void chatDeviceLoop(uint32_t now) {
    if (!runtime) return;
    auto &engine = runtime->engine;
    if (connectionState >= wifiUpdate) {
        updating = true; available = false;
        engine.disconnect(); // Never revive an updater's RF driver.
    }
    if (runtime->disconnected || (session.active() && isArmed)) {
        runtime->disconnected = false; engine.disconnect();
    }
    bool done;
    noInterrupts(); done = runtime->txDone; runtime->txDone = false; interrupts();
    if (done) engine.txDone();
    for (uint8_t i = 0; i < 4; ++i) {
        RadioEvent event;
        noInterrupts(); const bool have = runtime->events.pop(event); interrupts();
        if (!have) break;
        engine.received(event.bytes, event.rssi, event.snr);
    }
    Command command;
    for (uint8_t i = 0; i < 4 && runtime->commands.pop(command); ++i)
        if (!updating) engine.command(command.bytes, command.length, now);
    engine.tick(now);
    // The radio has already left chat and its callbacks have been restored.
    // Expire all application buffers after the Lua tool goes away.
    if (!session.active() && engine.state() == chat::State::Idle && now - runtime->lastCommand >= 6000) {
        Runtime *old = runtime;
        noInterrupts(); runtime = nullptr; chatDeviceScheduled = false; interrupts();
        delete old;
        strcpy(mailbox.value, "v1");
    }
}
#endif

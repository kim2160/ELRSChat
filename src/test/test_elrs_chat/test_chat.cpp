#include <unity.h>
#include "ChatMailbox.h"
#include "ChatSession.h"
#include "ChatProfile.h"
#include "../../lib/DEVICE/DevicePause.h"

void setUp() {}
void tearDown() {}

void existing_crsf_frames_are_not_claimed() {
    chat::Mailbox box; box.field = 42;
    const uint8_t p[] = {0xee,0xef,42,0};
    for (unsigned type = 0; type < 256; ++type)
        TEST_ASSERT_EQUAL(type == 0x2c || type == 0x2d, box.addressed(type,p,sizeof(p)));
    const uint8_t foreign[] = {0xec,0xef,42,0};
    TEST_ASSERT_FALSE(box.addressed(0x2d,foreign,sizeof(foreign)));
}

void parameter_chunks_are_stable_and_bounded() {
    chat::Mailbox box; box.field = 42;
    uint8_t bytes[42]; for (unsigned i = 0; i < sizeof(bytes); ++i) bytes[i] = i;
    box.reply(bytes,sizeof(bytes));
    uint8_t chunk0[64], chunk1[64], repeat[64];
    TEST_ASSERT_EQUAL(60,box.read(0,64,chunk0));
    TEST_ASSERT_EQUAL(1,chunk0[3]);
    const unsigned n=box.read(1,64,chunk1);
    box.reply(bytes,1);
    TEST_ASSERT_EQUAL(n,box.read(1,64,repeat));
    TEST_ASSERT_EQUAL_MEMORY(chunk1,repeat,n);
    TEST_ASSERT_EQUAL(0,box.read(255,64,repeat));
    TEST_ASSERT_EQUAL(0,box.read(0,8,repeat));
}

void invalid_base64_cannot_become_a_command() {
    uint8_t out[42];
    TEST_ASSERT_EQUAL(0,chat::Mailbox::decode(reinterpret_cast<const uint8_t *>("AB=="),4,out,42));
    TEST_ASSERT_EQUAL(0,chat::Mailbox::decode(reinterpret_cast<const uint8_t *>("AA=A"),4,out,42));
    TEST_ASSERT_EQUAL(0,chat::Mailbox::decode(reinterpret_cast<const uint8_t *>("AAAA"),4,out,2));
}

void pause_waits_for_the_other_core_boundary() {
    elrs::DevicePause gate;
    auto ticket=gate.request(2); gate.boundary(1);
    TEST_ASSERT_FALSE(gate.acknowledged(ticket));
    gate.boundary(0);
    TEST_ASSERT_TRUE(gate.acknowledged(ticket));
    TEST_ASSERT_TRUE(gate.paused(0,1));
    ticket=gate.request(0);
    TEST_ASSERT_FALSE(gate.acknowledged(ticket));
    gate.boundary(0);
    TEST_ASSERT_TRUE(gate.acknowledged(ticket));
    ticket=gate.request(2); gate.boundary(0);
    gate.request(0); ticket=gate.request(2);
    TEST_ASSERT_FALSE(gate.acknowledged(ticket));
}

void runtime_profile_checks_band_and_calibrated_power() {
    chat::Profile p; p.frequency=2440123000UL; p.power=6;
    TEST_ASSERT_TRUE(chat::selectSupportedProfile(p,0,6,false,true));
    TEST_ASSERT_EQUAL(1,p.id);
    p.power=7;
    TEST_ASSERT_FALSE(chat::selectSupportedProfile(p,0,6,false,true));
    p.power=1; p.frequency=915000000;
    TEST_ASSERT_FALSE(chat::selectSupportedProfile(p,0,6,false,true));
    TEST_ASSERT_TRUE(chat::selectSupportedProfile(p,0,6,true,false));
    TEST_ASSERT_EQUAL(2,p.id);
    p.frequency=0;
    TEST_ASSERT_FALSE(chat::selectSupportedProfile(p,0,6,true,true));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(existing_crsf_frames_are_not_claimed);
    RUN_TEST(parameter_chunks_are_stable_and_bounded);
    RUN_TEST(invalid_base64_cannot_become_a_command);
    RUN_TEST(pause_waits_for_the_other_core_boundary);
    RUN_TEST(runtime_profile_checks_band_and_calibrated_power);
    return UNITY_END();
}

#include "MeshTypes.h"
#include "TestUtil.h"
#include "PowerMon.h"
#include "UptimeClock.h"
#include "main.h"
#include "mesh/NodeDB.h"
#include "mesh/RadioLibInterface.h"
#include <unity.h>

// Exercise production recovery policy without GPIO, SPI, or an actual transceiver.
class RecoveryRadio : public RadioLibInterface
{
  public:
    RecoveryRadio() : RadioLibInterface(nullptr, RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC) {}
    bool reinitSucceeds = false;
    bool rxSucceeds = true;
    unsigned attempts = 0, starts = 0, agcResets = 0;
    bool recoverChipStateLoss() override { ++attempts; return reinitSucceeds; }
    void startReceive() override
    {
        ++starts;
        if (rxSucceeds)
            RadioLibInterface::startReceive();
        else
            rxOffline = true;
    }
    void resetAGC() override { ++agcResets; }
    bool isChannelActive() override { return false; }
    bool isActivelyReceiving() override { return false; }
    uint32_t getPacketTime(uint32_t, bool) override { return 0; }
    int16_t getCurrentRSSI() override { return -120; }
    void addReceiveMetadata(meshtastic_MeshPacket *) override {}
    void setRadioIsr(void (*)()) override {}
    void clearRadioIsr() override {}
};

static RecoveryRadio *radio;
static RadioLibInterface *savedInstance;
static PowerMon testPowerMon;
static PowerMon *savedPowerMon;
static uint32_t savedReboot, savedErrorAddress;
static meshtastic_CriticalErrorCode savedError;

void setUp()
{
    savedInstance = RadioLibInterface::instance;
    savedPowerMon = powerMon;
    savedReboot = rebootAtMsec;
    savedError = error_code;
    savedErrorAddress = error_address;
    powerMon = &testPowerMon;
    rebootAtMsec = 0;
    Time::setTestMillis(0);
    radio = new RecoveryRadio();
}

void tearDown()
{
    delete radio;
    radio = nullptr;
    RadioLibInterface::instance = savedInstance;
    powerMon = savedPowerMon;
    rebootAtMsec = savedReboot;
    error_code = savedError;
    error_address = savedErrorAddress;
    Time::useRealClock();
}

static void test_zero_timestamp_attempt_is_throttled()
{
    TEST_ASSERT_FALSE(radio->maybeRecoverChipStateLoss());
    TEST_ASSERT_FALSE(radio->maybeRecoverChipStateLoss());
    TEST_ASSERT_EQUAL_UINT(1, radio->attempts);
    Time::advanceTestMillis(29999);
    TEST_ASSERT_FALSE(radio->maybeRecoverChipStateLoss());
    TEST_ASSERT_EQUAL_UINT(1, radio->attempts);
    Time::advanceTestMillis(1);
    radio->maybeRecoverChipStateLoss();
    TEST_ASSERT_EQUAL_UINT(2, radio->attempts);
}

static void test_attempt_throttle_survives_rollover()
{
    Time::setTestMillis(UINT32_MAX - 9999);
    radio->maybeRecoverChipStateLoss();
    Time::advanceTestMillis(29999);
    radio->maybeRecoverChipStateLoss();
    TEST_ASSERT_EQUAL_UINT(1, radio->attempts);
    Time::advanceTestMillis(1);
    radio->maybeRecoverChipStateLoss();
    TEST_ASSERT_EQUAL_UINT(2, radio->attempts);
}

static void test_successful_reinit_does_not_clear_failures()
{
    radio->reinitSucceeds = true;
    TEST_ASSERT_TRUE(radio->maybeRecoverChipStateLoss());
    TEST_ASSERT_EQUAL_UINT(1, radio->chipRecoveryFailures);
    Time::advanceTestMillis(30000);
    TEST_ASSERT_TRUE(radio->maybeRecoverChipStateLoss());
    TEST_ASSERT_EQUAL_UINT(2, radio->chipRecoveryFailures);
    radio->startReceive();
    TEST_ASSERT_EQUAL_UINT(0, radio->chipRecoveryFailures);
    TEST_ASSERT_FALSE(radio->rxOffline);
}

static void test_quiet_offline_radio_gets_retried()
{
    radio->rxOffline = true;
    radio->reinitSucceeds = true;
    radio->periodicRadioMaintenance();
    TEST_ASSERT_EQUAL_UINT(1, radio->attempts);
    TEST_ASSERT_EQUAL_UINT(1, radio->starts);
    TEST_ASSERT_EQUAL_UINT(0, radio->agcResets);
    TEST_ASSERT_FALSE(radio->rxOffline);
}

static void test_failed_reinit_stays_offline()
{
    radio->rxOffline = true;
    radio->periodicRadioMaintenance();
    TEST_ASSERT_TRUE(radio->rxOffline);
    TEST_ASSERT_EQUAL_UINT(1, radio->attempts);
    TEST_ASSERT_EQUAL_UINT(0, radio->starts);
    TEST_ASSERT_EQUAL_UINT(0, radio->agcResets);
}

static void test_failed_rx_start_does_not_reset_ladder()
{
    radio->rxOffline = true;
    radio->reinitSucceeds = true;
    radio->rxSucceeds = false;
    radio->periodicRadioMaintenance();
    TEST_ASSERT_TRUE(radio->rxOffline);
    TEST_ASSERT_EQUAL_UINT(1, radio->chipRecoveryFailures);
    Time::advanceTestMillis(30000);
    radio->periodicRadioMaintenance();
    TEST_ASSERT_EQUAL_UINT(2, radio->chipRecoveryFailures);
}

static void test_healthy_radio_only_runs_agc()
{
    radio->periodicRadioMaintenance();
    TEST_ASSERT_EQUAL_UINT(0, radio->attempts);
    TEST_ASSERT_EQUAL_UINT(0, radio->starts);
    TEST_ASSERT_EQUAL_UINT(1, radio->agcResets);
}

static void test_reboot_is_delayed_until_failed_retry_window()
{
    for (unsigned i = 0; i < radio->MAX_CHIP_RECOVERY_FAILURES; ++i) {
        radio->maybeRecoverChipStateLoss();
        TEST_ASSERT_EQUAL_UINT32(0, rebootAtMsec);
        Time::advanceTestMillis(30000);
    }
    radio->maybeRecoverChipStateLoss();
    TEST_ASSERT_EQUAL_UINT32(Time::getMillis() + DEFAULT_REBOOT_SECONDS * 1000UL, rebootAtMsec);
}

static void test_zero_reboot_deadline_is_armed()
{
    radio->chipRecoveryFailures = radio->MAX_CHIP_RECOVERY_FAILURES;
    const uint32_t now = 0u - DEFAULT_REBOOT_SECONDS * 1000UL;
    Time::setTestMillis(now);
    radio->lastChipRecoveryMs = now - 30000;
    radio->maybeRecoverChipStateLoss();
    TEST_ASSERT_EQUAL_UINT32(1, rebootAtMsec);
}

static void test_existing_reboot_and_failure_counter_are_preserved()
{
    rebootAtMsec = 123456;
    radio->chipRecoveryFailures = radio->MAX_CHIP_RECOVERY_FAILURES;
    for (unsigned i = 0; i < 3; ++i) {
        Time::advanceTestMillis(30000);
        radio->maybeRecoverChipStateLoss();
    }
    TEST_ASSERT_EQUAL_UINT32(123456, rebootAtMsec);
    TEST_ASSERT_EQUAL_UINT(radio->MAX_CHIP_RECOVERY_FAILURES, radio->chipRecoveryFailures);
}

void setup()
{
    initializeTestEnvironment();
    UNITY_BEGIN();
    RUN_TEST(test_zero_timestamp_attempt_is_throttled);
    RUN_TEST(test_attempt_throttle_survives_rollover);
    RUN_TEST(test_successful_reinit_does_not_clear_failures);
    RUN_TEST(test_quiet_offline_radio_gets_retried);
    RUN_TEST(test_failed_reinit_stays_offline);
    RUN_TEST(test_failed_rx_start_does_not_reset_ladder);
    RUN_TEST(test_healthy_radio_only_runs_agc);
    RUN_TEST(test_reboot_is_delayed_until_failed_retry_window);
    RUN_TEST(test_zero_reboot_deadline_is_armed);
    RUN_TEST(test_existing_reboot_and_failure_counter_are_preserved);
    exit(UNITY_END());
}

void loop() {}

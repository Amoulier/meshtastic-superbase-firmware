#include "Arduino.h"
#include "NodeDB.h"
#include "TestUtil.h"
#include "UptimeClock.h"
#include "buzz/BuzzerMode.h"
#include "modules/ExternalNotificationModule.h"
#include <memory>
#include <unity.h>

namespace
{
class NotificationProbe : public ExternalNotificationModule
{
  public:
    using ExternalNotificationModule::runOnce;
};

meshtastic_LocalConfig savedConfig;
meshtastic_LocalModuleConfig savedModuleConfig;
std::unique_ptr<NotificationProbe> notification;

void prepareNotification(bool generic, bool vibra, bool buzzer)
{
    moduleConfig.external_notification.enabled = true;
    moduleConfig.external_notification.alert_message = generic;
    moduleConfig.external_notification.alert_message_vibra = vibra;
    moduleConfig.external_notification.alert_message_buzzer = buzzer;
    moduleConfig.external_notification.nag_timeout = 5;
    moduleConfig.external_notification.output_ms = 100;
    config.device.buzzer_mode = meshtastic_Config_DeviceConfig_BuzzerMode_ALL_ENABLED;
    notification = std::make_unique<NotificationProbe>();
}

void tickNotification(uint32_t elapsed = 0)
{
    Time::advanceTestMillis(elapsed);
    notification->runOnce();
}
} // namespace

void test_system_tones_follow_mode_policy()
{
    TEST_ASSERT_TRUE(buzzerModeAllowsSystemTones(meshtastic_Config_DeviceConfig_BuzzerMode_ALL_ENABLED));
    TEST_ASSERT_FALSE(buzzerModeAllowsSystemTones(meshtastic_Config_DeviceConfig_BuzzerMode_DISABLED));
    TEST_ASSERT_FALSE(buzzerModeAllowsSystemTones(meshtastic_Config_DeviceConfig_BuzzerMode_NOTIFICATIONS_ONLY));
    TEST_ASSERT_TRUE(buzzerModeAllowsSystemTones(meshtastic_Config_DeviceConfig_BuzzerMode_SYSTEM_ONLY));
    TEST_ASSERT_FALSE(buzzerModeAllowsSystemTones(meshtastic_Config_DeviceConfig_BuzzerMode_DIRECT_MSG_ONLY));
}

void test_channel_notifications_follow_mode_policy()
{
    TEST_ASSERT_TRUE(buzzerModeAllowsNotification(meshtastic_Config_DeviceConfig_BuzzerMode_ALL_ENABLED, false));
    TEST_ASSERT_FALSE(buzzerModeAllowsNotification(meshtastic_Config_DeviceConfig_BuzzerMode_DISABLED, false));
    TEST_ASSERT_TRUE(buzzerModeAllowsNotification(meshtastic_Config_DeviceConfig_BuzzerMode_NOTIFICATIONS_ONLY, false));
    TEST_ASSERT_FALSE(buzzerModeAllowsNotification(meshtastic_Config_DeviceConfig_BuzzerMode_SYSTEM_ONLY, false));
    TEST_ASSERT_FALSE(buzzerModeAllowsNotification(meshtastic_Config_DeviceConfig_BuzzerMode_DIRECT_MSG_ONLY, false));
}

void test_direct_notifications_follow_mode_policy()
{
    TEST_ASSERT_TRUE(buzzerModeAllowsNotification(meshtastic_Config_DeviceConfig_BuzzerMode_ALL_ENABLED, true));
    TEST_ASSERT_FALSE(buzzerModeAllowsNotification(meshtastic_Config_DeviceConfig_BuzzerMode_DISABLED, true));
    TEST_ASSERT_TRUE(buzzerModeAllowsNotification(meshtastic_Config_DeviceConfig_BuzzerMode_NOTIFICATIONS_ONLY, true));
    TEST_ASSERT_FALSE(buzzerModeAllowsNotification(meshtastic_Config_DeviceConfig_BuzzerMode_SYSTEM_ONLY, true));
    TEST_ASSERT_TRUE(buzzerModeAllowsNotification(meshtastic_Config_DeviceConfig_BuzzerMode_DIRECT_MSG_ONLY, true));
}

void test_unknown_mode_is_silent()
{
    const auto unknown = static_cast<meshtastic_Config_DeviceConfig_BuzzerMode>(99);
    TEST_ASSERT_FALSE(buzzerModeAllowsSystemTones(unknown));
    TEST_ASSERT_FALSE(buzzerModeAllowsNotification(unknown, false));
    TEST_ASSERT_FALSE(buzzerModeAllowsNotification(unknown, true));
}

void test_buzzer_only_never_activates_other_outputs()
{
    prepareNotification(false, false, true);
    notification->startNotification();
    for (unsigned i = 0; i < 5; ++i) {
        tickNotification(100);
        TEST_ASSERT_FALSE(notification->getExternal(0));
        TEST_ASSERT_FALSE(notification->getExternal(1));
    }
}

void test_generic_only_repeats_without_vibra_or_buzzer()
{
    prepareNotification(true, false, false);
    notification->startNotification();
    TEST_ASSERT_TRUE(notification->getExternal(0));
    tickNotification(100);
    TEST_ASSERT_FALSE(notification->getExternal(0));
    TEST_ASSERT_FALSE(notification->getExternal(1));
    TEST_ASSERT_FALSE(notification->getExternal(2));
    tickNotification(100);
    TEST_ASSERT_TRUE(notification->getExternal(0));
    TEST_ASSERT_FALSE(notification->getExternal(1));
    TEST_ASSERT_FALSE(notification->getExternal(2));
}

void test_bell_only_outputs_stay_off_for_non_message_notification()
{
    prepareNotification(false, false, true);
    moduleConfig.external_notification.alert_bell = true;
    moduleConfig.external_notification.alert_bell_vibra = true;
    notification->startNotification();
    tickNotification(100);
    TEST_ASSERT_FALSE(notification->getExternal(0));
    TEST_ASSERT_FALSE(notification->getExternal(1));
}

void test_vibra_only_repeats_without_generic_or_buzzer()
{
    prepareNotification(false, true, false);
    notification->startNotification();
    TEST_ASSERT_TRUE(notification->getExternal(1));
    tickNotification(100);
    TEST_ASSERT_FALSE(notification->getExternal(0));
    TEST_ASSERT_FALSE(notification->getExternal(1));
    TEST_ASSERT_FALSE(notification->getExternal(2));
    tickNotification(100);
    TEST_ASSERT_FALSE(notification->getExternal(0));
    TEST_ASSERT_TRUE(notification->getExternal(1));
    TEST_ASSERT_FALSE(notification->getExternal(2));
}

void test_stopped_outputs_do_not_restart_in_a_later_buzzer_cycle()
{
    prepareNotification(true, true, false);
    notification->startNotification();
    notification->stopNow();
    moduleConfig.external_notification.alert_message = false;
    moduleConfig.external_notification.alert_message_vibra = false;
    moduleConfig.external_notification.alert_message_buzzer = true;
    notification->startNotification();
    tickNotification(100);
    TEST_ASSERT_FALSE(notification->getExternal(0));
    TEST_ASSERT_FALSE(notification->getExternal(1));
}

void test_timeout_and_mute_stop_all_active_outputs()
{
    prepareNotification(true, true, true);
    notification->startNotification();
    tickNotification(5000);
    TEST_ASSERT_FALSE(notification->nagging());
    for (uint8_t i = 0; i < 3; ++i)
        TEST_ASSERT_FALSE(notification->getExternal(i));

    notification->startNotification();
    notification->setMute(true);
    tickNotification(100);
    notification->startNotification();
    TEST_ASSERT_FALSE(notification->nagging());
    for (uint8_t i = 0; i < 3; ++i)
        TEST_ASSERT_FALSE(notification->getExternal(i));
}

void setUp(void)
{
    savedConfig = config;
    savedModuleConfig = moduleConfig;
    config = meshtastic_LocalConfig_init_zero;
    moduleConfig = meshtastic_LocalModuleConfig_init_zero;
    Time::setTestMillis(10000);
}

void tearDown(void)
{
    if (notification)
        notification->stopNow();
    notification.reset();
    config = savedConfig;
    moduleConfig = savedModuleConfig;
    Time::useRealClock();
}

void setup()
{
    delay(10);
    initializeTestEnvironment();
    UNITY_BEGIN();
    RUN_TEST(test_system_tones_follow_mode_policy);
    RUN_TEST(test_channel_notifications_follow_mode_policy);
    RUN_TEST(test_direct_notifications_follow_mode_policy);
    RUN_TEST(test_unknown_mode_is_silent);
    RUN_TEST(test_buzzer_only_never_activates_other_outputs);
    RUN_TEST(test_generic_only_repeats_without_vibra_or_buzzer);
    RUN_TEST(test_bell_only_outputs_stay_off_for_non_message_notification);
    RUN_TEST(test_vibra_only_repeats_without_generic_or_buzzer);
    RUN_TEST(test_stopped_outputs_do_not_restart_in_a_later_buzzer_cycle);
    RUN_TEST(test_timeout_and_mute_stop_all_active_outputs);
    exit(UNITY_END());
}

void loop() {}

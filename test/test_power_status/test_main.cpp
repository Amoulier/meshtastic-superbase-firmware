#include "PowerStatus.h"
#include "TestUtil.h"
#include "power/BQ25185Status.h"
#include <cstdlib>
#include <unity.h>

using namespace meshtastic;

class PowerObserver : public Observer<const Status *>
{
  public:
    int notifications = 0;
    bool charging = false;
    int percent = 0;

    int onNotify(const Status *status) override
    {
        const auto *power = static_cast<const PowerStatus *>(status);
        ++notifications;
        charging = power->getIsCharging();
        percent = power->getBatteryChargePercent();
        return 0;
    }
};

void setUp() {}
void tearDown() {}

void test_charge_stop_and_start_notify_at_constant_voltage()
{
    PowerStatus current;
    PowerObserver observer;
    observer.observe(&current.onNewStatus);
    const PowerStatus charging(OptTrue, OptTrue, OptTrue, 4040, 97);
    const PowerStatus stopped(OptTrue, OptTrue, OptFalse, 4040, 97);
    current.updateStatus(&charging);
    current.updateStatus(&stopped);
    TEST_ASSERT_EQUAL(2, observer.notifications);
    TEST_ASSERT_FALSE(observer.charging);
    current.updateStatus(&charging);
    TEST_ASSERT_EQUAL(3, observer.notifications);
    TEST_ASSERT_TRUE(observer.charging);
}

void test_percentage_change_notifies_at_constant_voltage()
{
    PowerStatus current;
    PowerObserver observer;
    observer.observe(&current.onNewStatus);
    const PowerStatus before(OptTrue, OptTrue, OptFalse, 4050, 99);
    const PowerStatus after(OptTrue, OptTrue, OptFalse, 4050, 100);
    current.updateStatus(&before);
    current.updateStatus(&after);
    TEST_ASSERT_EQUAL(2, observer.notifications);
    TEST_ASSERT_EQUAL(100, observer.percent);
    current.updateStatus(&after);
    TEST_ASSERT_EQUAL(2, observer.notifications);
}

void test_identical_unknown_samples_only_notify_on_initialization()
{
    PowerStatus current;
    PowerObserver observer;
    observer.observe(&current.onNewStatus);
    const PowerStatus unknown;
    current.updateStatus(&unknown);
    TEST_ASSERT_TRUE(current.isInitialized());
    TEST_ASSERT_EQUAL(1, observer.notifications);
    current.updateStatus(&unknown);
    TEST_ASSERT_EQUAL(1, observer.notifications);
}

void test_optional_values_preserve_unknown_transitions()
{
    const OptionalBool values[] = {OptFalse, OptTrue, OptUnknown};
    for (auto value : values) {
        for (auto next : values) {
            for (int field = 0; field < 3; ++field) {
                PowerStatus current;
                PowerObserver observer;
                observer.observe(&current.onNewStatus);
                const PowerStatus before(field == 0 ? value : OptTrue, field == 1 ? value : OptTrue, field == 2 ? value : OptTrue,
                                         4000, 90);
                const PowerStatus after(field == 0 ? next : OptTrue, field == 1 ? next : OptTrue, field == 2 ? next : OptTrue,
                                        4000, 90);
                current.updateStatus(&before);
                current.updateStatus(&after);
                TEST_ASSERT_EQUAL(value == next ? 1 : 2, observer.notifications);
                current.updateStatus(&after);
                TEST_ASSERT_EQUAL(value == next ? 1 : 2, observer.notifications);
            }
        }
    }
}

void test_voltage_change_still_notifies()
{
    PowerStatus current;
    PowerObserver observer;
    observer.observe(&current.onNewStatus);
    const PowerStatus before(OptTrue, OptFalse, OptFalse, 4000, 90);
    const PowerStatus after(OptTrue, OptFalse, OptFalse, 4001, 90);
    current.updateStatus(&before);
    current.updateStatus(&after);
    TEST_ASSERT_EQUAL(2, observer.notifications);
    TEST_ASSERT_EQUAL(4001, current.getBatteryVoltageMv());
}

void test_bq25185_normal_charge()
{
    TEST_ASSERT_TRUE(decodeBQ25185Status(true, false) == BQ25185Status::Charging);
}

void test_bq25185_idle_is_not_proof_of_full_charge()
{
    TEST_ASSERT_TRUE(decodeBQ25185Status(true, true) == BQ25185Status::Idle);
}

void test_bq25185_recoverable_fault()
{
    TEST_ASSERT_TRUE(decodeBQ25185Status(false, true) == BQ25185Status::RecoverableFault);
}

void test_bq25185_latched_fault_is_not_charging()
{
    TEST_ASSERT_TRUE(decodeBQ25185Status(false, false) == BQ25185Status::LatchedFault);
}

void setup()
{
    initializeTestEnvironment();
    UNITY_BEGIN();
    RUN_TEST(test_charge_stop_and_start_notify_at_constant_voltage);
    RUN_TEST(test_percentage_change_notifies_at_constant_voltage);
    RUN_TEST(test_identical_unknown_samples_only_notify_on_initialization);
    RUN_TEST(test_optional_values_preserve_unknown_transitions);
    RUN_TEST(test_voltage_change_still_notifies);
    RUN_TEST(test_bq25185_normal_charge);
    RUN_TEST(test_bq25185_idle_is_not_proof_of_full_charge);
    RUN_TEST(test_bq25185_recoverable_fault);
    RUN_TEST(test_bq25185_latched_fault_is_not_charging);
    exit(UNITY_END());
}

void loop() {}

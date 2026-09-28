#pragma once
#include "Status.h"
#include "configuration.h"
#include "power/ChargeFault.h"
#include <Arduino.h>

namespace meshtastic
{

/**
 * A boolean where we have a third state of Unknown
 */
enum OptionalBool { OptFalse = 0, OptTrue = 1, OptUnknown = 2 };

/// Describes the state of the Power system.
class PowerStatus : public Status
{

  private:
    CallbackObserver<PowerStatus, const PowerStatus *> statusObserver =
        CallbackObserver<PowerStatus, const PowerStatus *>(this, &PowerStatus::updateStatus);

    /// Whether we have a battery connected
    OptionalBool hasBattery = OptUnknown;
    /// Battery voltage in mV, valid if haveBattery is true
    int batteryVoltageMv = 0;
    /// Battery charge percentage, either read directly or estimated
    int8_t batteryChargePercent = 0;
    /// Whether USB is connected
    OptionalBool hasUSB = OptUnknown;
    /// Whether we are charging the battery
    OptionalBool isCharging = OptUnknown;
    ChargeFault chargeFault = ChargeFault::None;

  public:
    PowerStatus() { statusType = STATUS_TYPE_POWER; }
    PowerStatus(OptionalBool hasBattery, OptionalBool hasUSB, OptionalBool isCharging, int batteryVoltageMv = -1,
                int8_t batteryChargePercent = 0, ChargeFault chargeFault = ChargeFault::None)
        : Status()
    {
        this->hasBattery = hasBattery;
        this->hasUSB = hasUSB;
        this->isCharging = isCharging;
        this->chargeFault = chargeFault;
        this->batteryVoltageMv = batteryVoltageMv;
        this->batteryChargePercent = batteryChargePercent;
    }
    PowerStatus(const PowerStatus &);
    PowerStatus &operator=(const PowerStatus &);

    void observe(Observable<const PowerStatus *> *source) { statusObserver.observe(source); }

    bool getHasBattery() const { return hasBattery == OptTrue; }

    bool getHasUSB() const { return hasUSB == OptTrue; }

    bool getIsCharging() const { return isCharging == OptTrue; }

    ChargeFault getChargeFault() const { return chargeFault; }

    int getBatteryVoltageMv() const { return batteryVoltageMv; }

    /**
     * Note: for boards with battery pin or PMU, 0% battery means 'unknown/this board doesn't have a battery installed'
     */
#if defined(HAS_PMU) || defined(BATTERY_PIN)
    uint8_t getBatteryChargePercent() const { return getHasBattery() ? batteryChargePercent : 0; }
#endif

    /**
     * Note: for boards without battery pin and PMU, 101% battery means 'the board is using external power'
     */
#if !defined(HAS_PMU) && !defined(BATTERY_PIN)
    uint8_t getBatteryChargePercent() const { return getHasBattery() ? batteryChargePercent : 101; }
#endif

    bool matches(const PowerStatus *newStatus) const
    {
        return (newStatus->hasBattery != hasBattery || newStatus->hasUSB != hasUSB || newStatus->isCharging != isCharging ||
                newStatus->batteryVoltageMv != batteryVoltageMv || newStatus->batteryChargePercent != batteryChargePercent ||
                newStatus->chargeFault != chargeFault);
    }
    int updateStatus(const PowerStatus *newStatus)
    {
        // Only update the status if values have actually changed
        bool isDirty;
        {
            isDirty = !initialized || matches(newStatus);
            initialized = true;
            hasBattery = newStatus->hasBattery;
            batteryVoltageMv = newStatus->getBatteryVoltageMv();
            batteryChargePercent = newStatus->batteryChargePercent;
            hasUSB = newStatus->hasUSB;
            isCharging = newStatus->isCharging;
            chargeFault = newStatus->chargeFault;
        }
        if (isDirty) {
            // LOG_DEBUG("Battery %dmV %d%%", batteryVoltageMv, batteryChargePercent);
            onNewStatus.notifyObservers(this);
        }
        return 0;
    }
};

} // namespace meshtastic

extern meshtastic::PowerStatus *powerStatus;

#pragma once

enum class ChargeFault { None, Recoverable, Latched };

constexpr const char *chargeFaultMessage(ChargeFault fault)
{
    switch (fault) {
    case ChargeFault::Recoverable:
        return "Charging paused\nCheck power/temp";
    case ChargeFault::Latched:
        return "Charging fault\nCheck charger";
    default:
        return nullptr;
    }
}

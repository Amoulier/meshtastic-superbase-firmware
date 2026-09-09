#pragma once

enum class BQ25185Status { Idle, Charging, RecoverableFault, LatchedFault };

// BQ25185 datasheet, table 6-2: STAT2 low alone does not imply charging.
constexpr BQ25185Status decodeBQ25185Status(bool stat1High, bool stat2High)
{
    if (!stat1High)
        return stat2High ? BQ25185Status::RecoverableFault : BQ25185Status::LatchedFault;
    return stat2High ? BQ25185Status::Idle : BQ25185Status::Charging;
}

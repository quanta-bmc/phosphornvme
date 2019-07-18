#include "nvme_manager.hpp"

#include <iostream>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <sstream>
#include <string>

namespace phosphor
{
namespace nvme
{
using namespace std;
using namespace phosphor::logging;

void NvmeSSD::checkSensorThreshold()
{
    uint64_t value = ValueIface::value();
    uint64_t criticalHigh = CriticalInterface::criticalHigh();
    uint64_t criticalLow = CriticalInterface::criticalLow();
    uint64_t warningHigh = WarningInterface::warningHigh();
    uint64_t warningLow = WarningInterface::warningLow();

    if (value > criticalHigh)
        CriticalInterface::criticalAlarmHigh(true);
    else
        CriticalInterface::criticalAlarmHigh(false);

    if (value < criticalLow)
        CriticalInterface::criticalAlarmLow(true);
    else
        CriticalInterface::criticalAlarmLow(false);

    if (value > warningHigh)
        WarningInterface::warningAlarmHigh(true);
    else
        WarningInterface::warningAlarmHigh(false);

    if (value < warningLow)
        WarningInterface::warningAlarmLow(true);
    else
        WarningInterface::warningAlarmLow(false);
}

void NvmeSSD::setSensorThreshold(uint64_t criticalHigh, uint64_t criticalLow,
                                 uint64_t maxValue, uint64_t minValue,
                                 uint64_t warningHigh, uint64_t warningLow)
{

    CriticalInterface::criticalHigh(criticalHigh);
    CriticalInterface::criticalLow(criticalLow);

    WarningInterface::warningHigh(warningHigh);
    WarningInterface::warningLow(warningLow);

    ValueIface::maxValue(maxValue);
    ValueIface::minValue(minValue);
}

void NvmeSSD::setSensorValueToDbus(const u_int64_t value)
{
    ValueIface::value(value);
}

} // namespace nvme
} // namespace phosphor
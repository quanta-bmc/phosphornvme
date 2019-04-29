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

void NvmeSSD::assertFaultLog(int smartWarning)
{
    std::vector<bool> bits;

    for (int i = 0; i < 5; i++)
    {
        if (((smartWarning >> i) & 1) == 0)
            bits.push_back(true);
        else
            bits.push_back(false);

        switch (i)
        {
            case 0:
                InfoIface::capacityFault(bits[i]);
                break;
            case 1:
                InfoIface::temperatureFault(bits[i]);
                break;
            case 2:
                InfoIface::degradesFault(bits[i]);
                break;
            case 3:
                InfoIface::mediaFault(bits[i]);
                break;
            case 4:
                InfoIface::backupDeviceFault(bits[i]);
                break;
        }
    }
}

void NvmeSSD::setPresent(const bool value)
{
    ItemIface::present(value);
}

void NvmeSSD::checkSensorThreshold()
{
    uint64_t value = ValueIface::value();
    uint64_t criticalHigh = CriticalInterface::criticalHigh();
    uint64_t criticalLow = CriticalInterface::criticalLow();

    if (value > criticalHigh)
        CriticalInterface::criticalAlarmHigh(true);
    else
        CriticalInterface::criticalAlarmHigh(false);

    if (value < criticalLow)
        CriticalInterface::criticalAlarmLow(true);
    else
        CriticalInterface::criticalAlarmLow(false);
}

void NvmeSSD::setSensorThreshold(uint64_t criticalHigh, uint64_t criticalLow,
                                 uint64_t maxValue, uint64_t minValue)
{

    CriticalInterface::criticalHigh(criticalHigh);
    CriticalInterface::criticalLow(criticalLow);
    ValueIface::maxValue(maxValue);
    ValueIface::minValue(minValue);
}

void NvmeSSD::setPropertiesToDbus(const u_int64_t value,
                                  const std::string vendorID,
                                  const std::string serialNumber,
                                  const std::string smartWarnings,
                                  const std::string statusFlags,
                                  const std::string driveLifeUsed
                                  )
{
    ValueIface::value(value);
    AssetIface::manufacturer(vendorID);
    AssetIface::serialNumber(serialNumber);
    InfoIface::smartWarnings(smartWarnings);
    InfoIface::statusFlags(statusFlags);
    InfoIface::driveLifeUsed(driveLifeUsed);
}

} // namespace nvme
} // namespace phosphor
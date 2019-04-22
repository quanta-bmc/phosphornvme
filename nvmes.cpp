#include "nvme_manager.hpp"
#include <iostream>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <sstream>
#include <string>
#include <xyz/openbmc_project/Led/Physical/server.hpp>

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

void NvmeSSD::checkAssertFaultLED(std::string &locateLedGroupPath, std::string &faultLedGroupPath, bool request)
{
    if(locateLedGroupPath.empty() || faultLedGroupPath.empty())
    {
        return;
    }

    if( !getLEDGroupState(locateLedGroupPath))
    {
        setFaultLED("Asserted", request, faultLedGroupPath);
    }
}

void NvmeSSD::checkAssertLocateLED(std::string &locateLedGroupPath, std::string &locateLedBusName, std::string &locateLedPath , bool isPresent)
{
    if(locateLedGroupPath.empty() || locateLedBusName.empty() || locateLedPath.empty())
    {
        return;
    }

    namespace server = sdbusplus::xyz::openbmc_project::Led::server;

    if( !getLEDGroupState(locateLedGroupPath))
    {
        if(isPresent)
            setLocateLED("State", server::convertForMessage(server::Physical::Action::On), locateLedBusName, locateLedPath);
        else
            setLocateLED("State", server::convertForMessage(server::Physical::Action::Off), locateLedBusName, locateLedPath);
    }

}

bool NvmeSSD::getLEDGroupState(std::string &ledPath)
{
    std::string obj_path;
    obj_path = ledPath;

    auto method = bus.new_method_call(LED_GROUP_BUSNAME, obj_path.c_str(),
                                      DBUS_PROPERTY_IFACE, "GetAll");

    method.append(LED_IFACE);
    auto reply = bus.call(method);
    if (reply.is_method_error())
    {
        log<level::ERR>("Error in get fault LED status.");
        return "";
    }
    std::map<std::string, variant<bool>> properties;
    reply.read(properties);

    bool asserted;
    asserted = get<bool>(properties.at("Asserted"));

    return asserted;
}

template <typename T>
void NvmeSSD::setFaultLED(const std::string &property, const T &value, std::string &ledPath)
{
    if(ledPath.empty())
        return;

    sdbusplus::message::variant<bool> data = value;
    std::string obj_path;
    obj_path = ledPath;

    try{
        auto methodCall = bus.new_method_call(LED_GROUP_BUSNAME, obj_path.c_str(),
                                            DBUS_PROPERTY_IFACE, "Set");

        methodCall.append(LED_IFACE);
        methodCall.append(property);
        methodCall.append(data);

        auto reply = bus.call(methodCall);
        if (reply.is_method_error())
        {
            log<level::ERR>("is_method_error()!");
        }
    }catch (const std::exception &e)
    {
        log<level::ERR>("Set FaultLED error");
        return ;
    }
}

template <typename T>
void NvmeSSD::setLocateLED(const std::string& property, const T& value, std::string &locateLedBusName, std::string &locateLedPath)
{
    sdbusplus::message::variant<T> data = value;
    std::string obj_path;
    obj_path = locateLedPath;

    try{
        auto methodCall = bus.new_method_call(locateLedBusName.c_str(), obj_path.c_str(),
                                                DBUS_PROPERTY_IFACE, "Set");

        methodCall.append(LED_CONTROLLER_IFACE);
        methodCall.append(property);
        methodCall.append(data);

        auto reply = bus.call(methodCall);
        if (reply.is_method_error())
        {
            log<level::ERR>("is_method_error()!");
        }
    }catch (const std::exception &e)
    {
        log<level::ERR>("Set LocateLED error");
        return ;
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
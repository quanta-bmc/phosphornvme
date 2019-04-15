#include "config.h"
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <sdeventplus/clock.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/utility/timer.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/server.hpp>
#include <xyz/openbmc_project/Nvme/Status/server.hpp>
#include <xyz/openbmc_project/Sensor/Value/server.hpp>

namespace phosphor
{
namespace nvme
{

using ValueIface = sdbusplus::xyz::openbmc_project::Sensor::server::Value;
using InfoIface = sdbusplus::xyz::openbmc_project::Nvme::server::Status;
using ItemIface = sdbusplus::xyz::openbmc_project::Inventory::server::Item;
using AssetIface = sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Asset;

using NvmeIfaces = sdbusplus::server::object::object<
    ValueIface,
    InfoIface,
    ItemIface,
    AssetIface>;

class NvmeSSD : public NvmeIfaces
{
public:
  NvmeSSD() = delete;
  NvmeSSD(const NvmeSSD &) = delete;
  NvmeSSD &operator=(const NvmeSSD &) = delete;
  NvmeSSD(NvmeSSD &&) = delete;
  NvmeSSD &operator=(NvmeSSD &&) = delete;
  virtual ~NvmeSSD() = default;

  NvmeSSD(sdbusplus::bus::bus &bus, const char *objPath) : NvmeIfaces(bus, objPath), bus(bus), _objPath(objPath)
  {
  }


  void setPresent(const bool value);
  const std::string _objPath;

  template <typename T>
  void setFaultLED(const std::string &property, const T &value, std::string &ledPath);

  template <typename T>
  void setLocateLED(const std::string& property, const T& value, std::string &locateLedBusName, std::string &locateLedPath);

  void checkAssertFaultLED(std::string &locateLedGroupPath, std::string &ledPath, bool request);
  void checkAssertLocateLED(std::string &ledPath, std::string &locateLedBusName, std::string &locateLedPath , bool ispresent);
  void assertFaultLog(int smartWarning);
  bool getLEDGroupState(std::string &ledPath);

  void setPropertiesToDbus(const u_int64_t value, const std::string vendorID,
                           const std::string serialNumber, const std::string smartWarnings,
                           const std::string statusFlags, const std::string driveLifeUsed);

private:
  sdbusplus::bus::bus &bus;

};
} // namespace nvme
} // namespace phosphor
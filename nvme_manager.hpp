#include "config.h"
#include "nvmes.hpp"
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/server/object.hpp>
#include <sdeventplus/clock.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/utility/timer.hpp>

namespace phosphor
{
namespace nvme
{

class Nvme
{
  public:
    Nvme() = delete;
    Nvme(const Nvme &) = delete;
    Nvme &operator=(const Nvme &) = delete;
    Nvme(Nvme &&) = delete;
    Nvme &operator=(Nvme &&) = delete;

    Nvme(sdbusplus::bus::bus &bus, const char *objPath) : bus(bus), _event(sdeventplus::Event::get_default()), _timer(_event, std::bind(&Nvme::read, this)), _objPath(objPath)
    {
    }

    struct NVMeConfig
    {
        uint8_t index;
        uint8_t busID;
        std::string faultLedGroupPath;
        uint8_t presentPin;
        uint8_t pwrGoodPin;
        std::string locateLedControllerBusName;
        std::string locateLedControllerPath;
        std::string locateLedGroupPath;
        uint64_t criticalHigh;
        uint64_t criticalLow;
        uint64_t maxValue;
        uint64_t minValue;
    };

    struct NVMeData {
      bool present;
      std::string vendor;
      std::string serialNumber;
      std::string smartWarnings;
      std::string statusFlags;
      std::string driveLifeUsed;
      u_int64_t sensorValue;
    };

    void run();
    const std::string _objPath;

    std::string getValue(std::string);
    std::unordered_map<std::string, std::shared_ptr<phosphor::nvme::NvmeSSD>>
        nvmes;
    void setSSDLEDStatus(std::shared_ptr<phosphor::nvme::NvmeSSD> nvmeSSD,
                         phosphor::nvme::Nvme::NVMeConfig config, bool success,
                         phosphor::nvme::Nvme::NVMeData nvmeData);

    template <typename T>
    void setFaultLED(const std::string &property, const T &value,
                     std::string &ledPath);

    template <typename T>
    void setLocateLED(const std::string &property, const T &value,
                      std::string &locateLedBusName,
                      std::string &locateLedPath);

    void checkAssertFaultLED(std::string &locateLedGroupPath,
                             std::string &ledPath, bool request);
    void checkAssertLocateLED(std::string &ledPath,
                              std::string &locateLedBusName,
                              std::string &locateLedPath, bool ispresent);

    bool getLEDGroupState(std::string &ledPath);

  private:
    sdbusplus::bus::bus &bus;

    sdeventplus::Event _event;
    /** @brief Read Timer */
    sdeventplus::utility::Timer<sdeventplus::ClockId::Monotonic> _timer;

    void init();
    void read();

};
} // namespace nvme
} // namespace phosphor
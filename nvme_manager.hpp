#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <sdeventplus/clock.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/utility/timer.hpp>
#include "config.h"
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

    class Nvme : public NvmeIfaces
    {
        public:
            Nvme() = delete;
            Nvme(const Nvme&) = delete;
            Nvme& operator=(const Nvme&) = delete;
            Nvme(Nvme&&) = delete;
            Nvme& operator=(Nvme&&) = delete;
            virtual ~Nvme() = default; 

                

            Nvme(sdbusplus::bus::bus& bus, const char* objPath, const int busid, const std::string ledPath) :
                NvmeIfaces(bus, objPath), bus(bus),_event(sdeventplus::Event::get_default()),
            _timer(_event, std::bind(&Nvme::read, this)), _objPath(objPath), _busID(busid), _ledPath(ledPath)
            {
                
            }    

            void setPropertiesToDbus(const u_int64_t value,const std::string vendorID,
                const std::string serialNumber,const std::string smartWarnings,
                const std::string statusFlags,const std::string driveLifeUsed );

            void setPresent(const bool value);
            bool isPresent();            
            void run();   
            const std::string _objPath;             
            const int _busID;            
            const std::string  _ledPath; 

            template <typename T>
            void setFaultLED(const std::string& property, const T& value, std::string &ledPath);
            void checkAssertFaultLED(std::string &smartWarning, std::string &ledPath);
            void assertFaultLog(int smartWarning);
            bool getFaultLEDstate(std::string &ledPath);  
        
        private:
        
            sdbusplus::bus::bus& bus;

            sdeventplus::Event _event;
            /** @brief Read Timer */
            sdeventplus::utility::Timer<sdeventplus::ClockId::Monotonic> _timer;

            void init();
            void read(); 
            bool isBoot();
            
    };
}
}
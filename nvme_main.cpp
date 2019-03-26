#include <sdbusplus/bus.hpp>
#include <sdbusplus/sdbus.hpp>
#include <sdbusplus/server/manager.hpp>
#include <string.h>
#include "nvme_manager.hpp"
#include <set>
#include "nlohmann/json.hpp"
#include <fstream>
#include <phosphor-logging/log.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <exception>
#include <iostream>

using Json = nlohmann::json; 
using namespace phosphor::logging;

static constexpr auto configFile = "/usr/share/nvme_config.json";

struct NVMeConfig
{
    uint8_t index ;   
    uint8_t budID ;
    std::string faultLedPath;   
};

Json parseSensorConfig()
{
    std::ifstream jsonFile(configFile);
    if (!jsonFile.is_open())
    {
        log<level::ERR>("NVMe config JSON file not found");       
    }

    auto data = Json::parse(jsonFile, nullptr, false);
    if (data.is_discarded())
    {
        log<level::ERR>("NVMe config readings JSON parser failure");
    }

    return data;
}

std::vector<NVMeConfig> getNvmeConfig()
{  
    NVMeConfig nvmeConfig;

    std::vector<NVMeConfig> nvmeConfigs;

    std::cout << "start getNvmeConfig " << std::endl;

    try{
        auto data = parseSensorConfig();

        static const std::vector<Json> empty{};    

        std::vector<Json> readings = data.value("config", empty);

        if (!readings.empty())
        {
            for (const auto& instance : readings)
            {
                uint8_t index = instance.value("NvmeDriveIndex", 0);  
                uint8_t busID = instance.value("NVMeDriveBusID", 0);
                std::string faultLedPath = instance.value("NVMeDriveFaultLED", "");                 

                nvmeConfig.index = index;
                nvmeConfig.budID = busID;                
                nvmeConfig.faultLedPath = faultLedPath;
                nvmeConfigs.push_back(nvmeConfig);        
            }    
        }
        else
        {
            log<level::ERR>("Invalid NVMe config file");
        }        

    }catch (const Json::exception& e)
    {
        log<level::DEBUG>("Json Exception caught.", entry("MSG:%s", e.what()));        
    }

    return nvmeConfigs;    
}

int main(void)
{  
    std::vector<NVMeConfig> configs;
    
    configs = getNvmeConfig();
    size_t size = configs.size();       

    sdbusplus::bus::bus bus = sdbusplus::bus::new_default(); 
    sd_event* event = nullptr;
    auto eventDeleter = [](sd_event* e) { e = sd_event_unref(e); };
    using SdEvent = std::unique_ptr<sd_event, decltype(eventDeleter)>;

    // acquire a reference to the default event loop
    sd_event_default(&event);
    SdEvent sdEvent{event, eventDeleter};
    event = nullptr;    

    // attach bus to this event loop
    bus.attach_event(sdEvent.get(), SD_EVENT_PRIORITY_NORMAL);
    sdbusplus::server::manager::manager objManager(bus, NVME_OBJ_PATH_ROOT); 
    std::set<std::unique_ptr<phosphor::nvme::Nvme>> nvmes; 

    std::string objPath;    

    for (int i = 0; i< size; i++)
    {     
        objPath = NVME_OBJ_PATH + std::to_string(configs[i].index); 

        nvmes.insert(std::make_unique<phosphor::nvme::Nvme>(bus,objPath.c_str(),configs[i].budID, configs[i].faultLedPath));
    }

    bus.request_name(NVME_REQUEST_NAME);     
    
    for(auto& nvme : nvmes)
    {        
        nvme->run();        
    }

    // Start event loop for all sd-bus events

    sd_event_loop(bus.get_event());

    bus.detach_event();  

    return 0;
}
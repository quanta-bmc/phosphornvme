#include "nvme_manager.hpp"
#include <string>
#include <sstream>
#include "i2c-dev.h"
#include "smbus.hpp"
#include <phosphor-logging/log.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <iostream>

#define MAX_I2C_BUS               30
#define MONITOR_INTERVAL_SENCODS  1
#define NVME_SSD_SLAVE_ADDRESS    0x6a

static int fd[MAX_I2C_BUS] = {0};

namespace phosphor
{
namespace nvme
{

using namespace std;
using namespace phosphor::logging;

    void Nvme::assertFaultLog(int smartWarning)
    {        
        std::vector<bool> bits; 

        for(int i = 0; i < 5 ; i++)
        {
            if(((smartWarning >> i) &1) == 0)
                bits.push_back(true);
            else
                bits.push_back(false); 

            switch (i)
            {
                case 0 :
                    InfoIface::capacityFault(bits[i]); 
                    break;
                case 1 :
                    InfoIface::temperatureFault(bits[i]); 
                    break;
                case 2 :
                    InfoIface::degradesFault(bits[i]);  
                    break;
                case 3 :
                    InfoIface::mediaFault(bits[i]);
                    break;
                case 4 :
                    InfoIface::backupDeviceFault(bits[i]); 
                    break;
            }
        }           
    }    

    void Nvme::checkAssertFaultLED(std::string &smartWarning, std::string &ledPath) 
    {        
        bool request; 
        bool current = getFaultLEDstate(ledPath); 

        

        if(strcmp(smartWarning.c_str(), "ff") == 0) 
            request = false; 
        else        
            request = true; 

        
        if(request != current)
        {
            setFaultLED("Asserted", request, ledPath); 
        }

        assertFaultLog(std::stoi(smartWarning, 0, 16));       
    }

    bool Nvme::getFaultLEDstate(std::string &ledPath) 
    {
        std::string obj_path;
        obj_path = ledPath;

        auto method = bus.new_method_call(LED_BUSNAME, obj_path.c_str(),
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
    void Nvme::setFaultLED(const std::string& property, const T& value, std::string &ledPath) 
    {
        sdbusplus::message::variant<bool> data = value;
        std::string obj_path;
        obj_path = ledPath;
            
        auto methodCall = bus.new_method_call(LED_BUSNAME, obj_path.c_str(),
                                                DBUS_PROPERTY_IFACE, "Set");
            
        methodCall.append(LED_IFACE);
        methodCall.append(property);
        methodCall.append(data);

        auto reply = bus.call(methodCall);
        if (reply.is_method_error())
        {
            log<level::ERR>("is_method_error()!");             
        }
    }    

    bool Nvme::isPresent()
    {  
        return ItemIface::present();          
    } 
    
    void Nvme::setPresent(const bool value)
    {  
        ItemIface::present(value);          
    }    

    void Nvme::setPropertiesToDbus(const u_int64_t value,const std::string vendorID,
        const std::string serialNumber,const std::string smartWarnings,
        const std::string statusFlags,const std::string driveLifeUsed )
    {
        ValueIface::value(value);
        AssetIface::manufacturer(vendorID);  
        AssetIface::serialNumber(serialNumber);  
        InfoIface::smartWarnings(smartWarnings);
        InfoIface::statusFlags(statusFlags); 
        InfoIface::driveLifeUsed(driveLifeUsed); 
    }

    struct NVMeData
    {        
        bool present;
        std::string vendor;
        std::string serialNumber;        
        std::string smartWarnings;
        std::string statusFlags;
        std::string driveLifeUsed;
        u_int64_t sensorValue;                  
    };    

    std::string intToHex(int input)
    {       
        std::stringstream tmp;
        tmp << std::hex << input;

        return tmp.str();
    }

    bool getNVMeInfobyBusID(int busID, NVMeData &nvmeData)
    {     
        nvmeData.present = true;        
        nvmeData.vendor = "";
        nvmeData.serialNumber = "";
        nvmeData.smartWarnings = "";
        nvmeData.statusFlags = "";
        nvmeData.driveLifeUsed = "";
        nvmeData.sensorValue = 0;

        phosphor::smbus::Smbus smbus;        
        
        unsigned char rsp_data_command_0[8] = {0};
        unsigned char rsp_data_command_8[24] = {0};

        uint8_t in_data = 0; // command code

        auto init = smbus.smbusInit(busID);        

        if(init == -1)
        {
            log<level::ERR>("smbusInit fail!");  

            nvmeData.present = false;

            return nvmeData.present;
        }            

        auto res_int = smbus.SendSmbusRWBlockCmdRAW(busID, NVME_SSD_SLAVE_ADDRESS, &in_data , 1, rsp_data_command_0);              

        if(res_int < 0 )
        {
            log<level::ERR>("Send command 0 fail!");  
            
            smbus.smbusClose(busID);
            nvmeData.present = false;
            return nvmeData.present;
        }

        in_data = 8;// command code        

        res_int = smbus.SendSmbusRWBlockCmdRAW(busID, NVME_SSD_SLAVE_ADDRESS, &in_data , 1, rsp_data_command_8);        

        if(res_int < 0 )
        {
            log<level::ERR>("Send command 8 fail!");  
            smbus.smbusClose(busID);
            nvmeData.present = false;
            return nvmeData.present;
        }
        
        nvmeData.vendor = intToHex(rsp_data_command_8[1]) + " " + intToHex(rsp_data_command_8[2]);
        for (int i = 3; i< 23 ; i++) // i: serialNumber position
        {
            nvmeData.serialNumber += static_cast<char>(rsp_data_command_8[i]);
        }

        nvmeData.statusFlags = intToHex(rsp_data_command_0[1]);
        nvmeData.smartWarnings = intToHex(rsp_data_command_0[2]);
        nvmeData.driveLifeUsed = intToHex(rsp_data_command_0[4]);            
        nvmeData.sensorValue = (u_int64_t) rsp_data_command_0[3];
            
        smbus.smbusClose(busID);

        return nvmeData.present;
    }
    

    void Nvme::run()
    {
        init();
        
        std::function<void()> callback(std::bind(&Nvme::read, this));
        try
        {            
            u_int64_t interval = MONITOR_INTERVAL_SENCODS * 1000000;
            _timer.restart(std::chrono::microseconds(interval));                 
        }
        catch (const std::exception& e)
        {            
            log<level::ERR>("Error in polling loop",
                        entry("ERROR=%s", e.what())); 
            throw;
        }        
    }

    void Nvme::init()
    {        
        NVMeData nvmeData;

        if(isPresent())
        {
            try
            {
                nvmeData.present = getNVMeInfobyBusID(_busID, nvmeData); // get NVMe information through i2c by busID.
            
                setPresent(nvmeData.present);

                if (nvmeData.present)
                {
                    setPropertiesToDbus(nvmeData.sensorValue, nvmeData.vendor, nvmeData.serialNumber, 
                        nvmeData.smartWarnings,nvmeData.statusFlags,nvmeData.driveLifeUsed);

                    std::string smartWarnings = InfoIface::smartWarnings(); 
                    std::string ledPath = _ledPath;            

                    if(smartWarnings.empty() == false && ledPath.empty() == false)
                        checkAssertFaultLED(smartWarnings, ledPath);    
                } 
            }
            catch(const std::exception& e)
            {
                log<level::ERR>("Error in get NVMe information",
                        entry("ERROR=%s", e.what())); 
            }
        }     
    }

    void Nvme::read()
    {       
        NVMeData nvmeData;        

        if(isPresent())
        {
            try
            {
                nvmeData.present = getNVMeInfobyBusID(_busID, nvmeData); // get NVMe information through i2c by busID.
            
                setPresent(nvmeData.present);

                if (nvmeData.present)
                {
                    setPropertiesToDbus(nvmeData.sensorValue, nvmeData.vendor, nvmeData.serialNumber, 
                        nvmeData.smartWarnings,nvmeData.statusFlags,nvmeData.driveLifeUsed);

                    std::string smartWarnings = InfoIface::smartWarnings(); 
                    std::string ledPath = _ledPath;            

                    if(smartWarnings.empty() == false && ledPath.empty() == false)
                        checkAssertFaultLED(smartWarnings, ledPath);    
                } 
            }
            catch(const std::exception& e)
            {
                log<level::ERR>("Error in get NVMe information",
                        entry("ERROR=%s", e.what())); 
            }
        }            
    }

}
}
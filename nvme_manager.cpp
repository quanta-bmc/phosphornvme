#include "nvme_manager.hpp"
#include "i2c-dev.h"
#include "nlohmann/json.hpp"
#include "smbus.hpp"
#include <experimental/filesystem>
#include <fstream>
#include <iostream>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <sstream>
#include <string>

#define MAX_I2C_BUS 30
#define MONITOR_INTERVAL_SENCODS 1
#define NVME_SSD_SLAVE_ADDRESS 0x6a
#define GPIO_BASE_PATH "/sys/class/gpio/gpio"
#define IS_PRESENT "0"
#define POWERGD "1"

static int fd[MAX_I2C_BUS] = {0};
static constexpr auto configFile = "/usr/share/nvme_config.json";
auto retries = 3;
static constexpr auto delay = std::chrono::milliseconds{100};

struct NVMeConfig {
  uint8_t index;
  uint8_t budID;
  std::string faultLedGroupPath;
  uint8_t presentPin;
  uint8_t pwrGoodPin;
  std::string locateLedControllerBusName;
  std::string locateLedControllerPath;
  std::string locateLedGroupPath;
};

using Json = nlohmann::json;
std::vector<NVMeConfig> configs;
namespace fs = std::experimental::filesystem;

namespace phosphor
{
namespace nvme
{

using namespace std;
using namespace phosphor::logging;

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

    if (init == -1)
    {
        log<level::ERR>("smbusInit fail!");

        nvmeData.present = false;

        return nvmeData.present;
    }

    auto res_int = smbus.SendSmbusRWBlockCmdRAW(busID, NVME_SSD_SLAVE_ADDRESS, &in_data, 1, rsp_data_command_0);

    if (res_int < 0)
    {
        log<level::ERR>("Send command 0 fail!");

        smbus.smbusClose(busID);
        nvmeData.present = false;
        return nvmeData.present;
    }

    in_data = 8; // command code

    res_int = smbus.SendSmbusRWBlockCmdRAW(busID, NVME_SSD_SLAVE_ADDRESS, &in_data, 1, rsp_data_command_8);

    if (res_int < 0)
    {
        log<level::ERR>("Send command 8 fail!");
        smbus.smbusClose(busID);
        nvmeData.present = false;
        return nvmeData.present;
    }

    nvmeData.vendor = intToHex(rsp_data_command_8[1]) + " " + intToHex(rsp_data_command_8[2]);
    for (int i = 3; i < 23; i++) // i: serialNumber position
    {
        nvmeData.serialNumber += static_cast<char>(rsp_data_command_8[i]);
    }

    nvmeData.statusFlags = intToHex(rsp_data_command_0[1]);
    nvmeData.smartWarnings = intToHex(rsp_data_command_0[2]);
    nvmeData.driveLifeUsed = intToHex(rsp_data_command_0[4]);
    nvmeData.sensorValue = (u_int64_t)rsp_data_command_0[3];

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
    catch (const std::exception &e)
    {
        log<level::ERR>("Error in polling loop",
                        entry("ERROR=%s", e.what()));
        throw;
    }
}

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

    try
    {
        auto data = parseSensorConfig();

        static const std::vector<Json> empty{};

        std::vector<Json> readings = data.value("config", empty);

        if (!readings.empty())
        {
            for (const auto &instance : readings)
            {
                uint8_t index = instance.value("NvmeDriveIndex", 0);
                uint8_t busID = instance.value("NVMeDriveBusID", 0);
                std::string faultLedGroupPath = instance.value("NVMeDriveFaultLEDGroupPath", "");
                std::string locateLedGroupPath = instance.value("NVMeDriveLocateLEDGroupPath", "");
                uint8_t presentPin = instance.value("NVMeDrivePresentPin", 0);
                uint8_t pwrGoodPin = instance.value("NVMeDrivePwrGoodPin", 0);
                std::string locateLedControllerBusName = instance.value("NVMeDriveLocateLEDControllerBusName", "");
                std::string locateLedControllerPath = instance.value("NVMeDriveLocateLEDControllerPath", "");

                nvmeConfig.index = index;
                nvmeConfig.budID = busID;
                nvmeConfig.faultLedGroupPath = faultLedGroupPath;
                nvmeConfig.presentPin = presentPin;
                nvmeConfig.pwrGoodPin = pwrGoodPin;
                nvmeConfig.locateLedControllerBusName = locateLedControllerBusName;
                nvmeConfig.locateLedControllerPath = locateLedControllerPath;
                nvmeConfig.locateLedGroupPath = locateLedGroupPath;
                nvmeConfigs.push_back(nvmeConfig);
            }
        }
        else
        {
            log<level::INFO>("Invalid NVMe config file");
        }
    }
    catch (const Json::exception &e)
    {
        log<level::DEBUG>("Json Exception caught.", entry("MSG:%s", e.what()));
    }

    return nvmeConfigs;
}

std::string Nvme::getValue(std::string fullPath)
{
    std::string val;
    std::ifstream ifs;

    while (true)
    {
        try {
            if (!ifs.is_open())
                ifs.open(fullPath);
            ifs.clear();
            ifs.seekg(0);
            ifs >> val;
        } catch (const std::exception &e) {
            --retries;
            std::this_thread::sleep_for(delay);
            continue;
        }
        break;
    }

    ifs.close();
    return val;
}

void Nvme::init()
{
    //read json file
    configs = getNvmeConfig();

    try{
        for (int i = 0; i < configs.size(); i++)
        {
            std::string objPath = NVME_OBJ_PATH + std::to_string(configs[i].index);
            nvmes.emplace(std::to_string(configs[i].index), std::make_unique<phosphor::nvme::NvmeSSD>(
                                        bus, objPath.c_str()));
        }
    }catch (const std::exception &e){
        log<level::ERR>("Create d-bus path fail");
    }
}

void Nvme::read()
{
    for (int i = 0; i < configs.size(); i++)
    {
        NVMeData nvmeData;

        std::string devPresentPath = GPIO_BASE_PATH +
                                     std::to_string(configs[i].presentPin) +
                                     "/value";

        std::string devPwrGoodPath = GPIO_BASE_PATH +
                                     std::to_string(configs[i].pwrGoodPin) +
                                     "/value";

        auto iter = nvmes.find(std::to_string(configs[i].index));

        if (getValue(devPresentPath) == IS_PRESENT)
        {
            // Drive status is good, update value or create d-bus and update value.
            if(getValue(devPwrGoodPath) == POWERGD)
            {
                // get NVMe information through i2c by busID.
                auto success = getNVMeInfobyBusID(configs[i].budID, nvmeData);

                // Can not find it in previous nvmes map but drive is present this time, created d-bus
                if (iter == nvmes.end())
                {

                    std::string objPath = NVME_OBJ_PATH + std::to_string(configs[i].index);
                    nvmes.emplace(std::to_string(configs[i].index), std::make_unique<phosphor::nvme::NvmeSSD>(
                                    bus, objPath.c_str()));
                }
                iter = nvmes.find(std::to_string(configs[i].index)); // find target again
                if (iter != nvmes.end())
                {
                    iter->second->setPresent(true);
                    iter->second->setPropertiesToDbus(
                            nvmeData.sensorValue, nvmeData.vendor,
                            nvmeData.serialNumber, nvmeData.smartWarnings,
                            nvmeData.statusFlags, nvmeData.driveLifeUsed);
                    if (success)
                    {
                        if(!nvmeData.smartWarnings.empty())
                        {
                            iter->second->assertFaultLog(std::stoi(nvmeData.smartWarnings, 0, 16));
                            auto request = (strcmp(nvmeData.smartWarnings.c_str(), "ff") == 0)? false : true;
                            iter->second->checkAssertFaultLED(configs[i].locateLedGroupPath, configs[i].faultLedGroupPath, request);
                            iter->second->checkAssertLocateLED(configs[i].locateLedGroupPath,configs[i].locateLedControllerBusName,configs[i].locateLedControllerPath, !request);
                        }

                    }
                    else
                    {
                        // Drive is present but can not get data, turn on fault LED.
                        log<level::ERR>("Drive status is good but can not get data.");
                        iter->second->checkAssertFaultLED(configs[i].locateLedGroupPath, configs[i].faultLedGroupPath, true);
                        iter->second->checkAssertLocateLED(configs[i].locateLedGroupPath,configs[i].locateLedControllerBusName,configs[i].locateLedControllerPath, false);
                    }

                }

            }
            else
            {
                // Present pin is true but power good pin is false
                log<level::ERR>("Present pin is true but power good pin is false");
                if (iter != nvmes.end())
                {
                    iter->second->checkAssertFaultLED(configs[i].locateLedGroupPath, configs[i].faultLedGroupPath, true);
                    iter->second->checkAssertLocateLED(configs[i].locateLedGroupPath,configs[i].locateLedControllerBusName,configs[i].locateLedControllerPath, false);
                }
                nvmes.erase(std::to_string(configs[i].index));
            }
        }
        else
        {
            // Drive not present, remove nvme d-bus path and turn off fault and locate LED
            if (iter != nvmes.end())
            {
                iter->second->checkAssertFaultLED(configs[i].locateLedGroupPath, configs[i].faultLedGroupPath, false);
                iter->second->checkAssertLocateLED(configs[i].locateLedGroupPath,configs[i].locateLedControllerBusName,configs[i].locateLedControllerPath, false);
            }
            nvmes.erase(std::to_string(configs[i].index));
        }
    }
}

} // namespace nvme
} // namespace phosphor
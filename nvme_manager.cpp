#include "nvme_manager.hpp"
#include "nlohmann/json.hpp"
#include "smbus.hpp"
#include <experimental/filesystem>
#include <fstream>
#include <iostream>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <sstream>
#include <string>
#include "i2c-dev.h"

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
using Json = nlohmann::json;
std::vector<phosphor::nvme::Nvme::NVMeConfig> configs;
namespace fs = std::experimental::filesystem;

namespace phosphor
{
namespace nvme
{

using namespace std;
using namespace phosphor::logging;

std::string intToHex(int input)
{
    std::stringstream tmp;
    tmp << std::hex << input;

    return tmp.str();
}

bool getNVMeInfobyBusID(int busID, phosphor::nvme::Nvme::NVMeData &nvmeData)
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

    auto res_int = smbus.SendSmbusRWBlockCmdRAW(
        busID, NVME_SSD_SLAVE_ADDRESS, &in_data, 1, rsp_data_command_0);

    if (res_int < 0)
    {
        log<level::ERR>("Send command 0 fail!");

        smbus.smbusClose(busID);
        nvmeData.present = false;
        return nvmeData.present;
    }

    in_data = 8; // command code

    res_int = smbus.SendSmbusRWBlockCmdRAW(busID, NVME_SSD_SLAVE_ADDRESS,
                                           &in_data, 1, rsp_data_command_8);

    if (res_int < 0)
    {
        log<level::ERR>("Send command 8 fail!");
        smbus.smbusClose(busID);
        nvmeData.present = false;
        return nvmeData.present;
    }

    nvmeData.vendor =
        intToHex(rsp_data_command_8[1]) + " " + intToHex(rsp_data_command_8[2]);
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
        log<level::ERR>("Error in polling loop", entry("ERROR=%s", e.what()));
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

std::vector<phosphor::nvme::Nvme::NVMeConfig> getNvmeConfig()
{

    phosphor::nvme::Nvme::NVMeConfig nvmeConfig;
    std::vector<phosphor::nvme::Nvme::NVMeConfig> nvmeConfigs;
    uint64_t criticalHigh = 0;
    uint64_t criticalLow = 0;
    uint64_t maxValue = 0;
    uint64_t minValue = 0;

    try
    {
        auto data = parseSensorConfig();
        static const std::vector<Json> empty{};
        std::vector<Json> readings = data.value("config", empty);
        std::vector<Json> thresholds = data.value("threshold", empty);
        if (!thresholds.empty())
        {
            for (const auto &instance : thresholds)
            {
                criticalHigh = instance.value("criticalHigh", 0);
                criticalLow = instance.value("criticalLow", 0);
                maxValue = instance.value("maxValue", 0);
                minValue = instance.value("minValue", 0);
            }
        }
        else
        {
            log<level::INFO>("Invalid NVMe config file");
        }

        if (!readings.empty())
        {
            for (const auto &instance : readings)
            {
                uint8_t index = instance.value("NvmeDriveIndex", 0);
                uint8_t busID = instance.value("NVMeDriveBusID", 0);
                std::string faultLedGroupPath =
                    instance.value("NVMeDriveFaultLEDGroupPath", "");
                std::string locateLedGroupPath =
                    instance.value("NVMeDriveLocateLEDGroupPath", "");
                uint8_t presentPin = instance.value("NVMeDrivePresentPin", 0);
                uint8_t pwrGoodPin = instance.value("NVMeDrivePwrGoodPin", 0);
                std::string locateLedControllerBusName =
                    instance.value("NVMeDriveLocateLEDControllerBusName", "");
                std::string locateLedControllerPath =
                    instance.value("NVMeDriveLocateLEDControllerPath", "");

                nvmeConfig.index = index;
                nvmeConfig.busID = busID;
                nvmeConfig.faultLedGroupPath = faultLedGroupPath;
                nvmeConfig.presentPin = presentPin;
                nvmeConfig.pwrGoodPin = pwrGoodPin;
                nvmeConfig.locateLedControllerBusName =
                    locateLedControllerBusName;
                nvmeConfig.locateLedControllerPath = locateLedControllerPath;
                nvmeConfig.locateLedGroupPath = locateLedGroupPath;
                nvmeConfig.criticalHigh = criticalHigh;
                nvmeConfig.criticalLow = criticalLow;
                nvmeConfig.maxValue = maxValue;
                nvmeConfig.minValue = minValue;
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
        try
        {
            if (!ifs.is_open())
                ifs.open(fullPath);
            ifs.clear();
            ifs.seekg(0);
            ifs >> val;
        }
        catch (const std::exception &e)
        {
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
    // read json file
    configs = getNvmeConfig();

    try{
        for (int i = 0; i < configs.size(); i++)
        {
            std::string objPath = NVME_OBJ_PATH + std::to_string(configs[i].index);
            auto nvmeSSD = std::make_shared<phosphor::nvme::NvmeSSD>(
                            bus, objPath.c_str());
            nvmes.emplace(std::to_string(configs[i].index), nvmeSSD);
            nvmeSSD->setSensorThreshold(
                            configs[i].criticalHigh, configs[i].criticalLow,
                            configs[i].maxValue, configs[i].minValue);
        }

    }catch (const std::exception &e){
        log<level::ERR>("Create d-bus path fail");
    }
}

void Nvme::setSSDLEDStatus(std::shared_ptr<phosphor::nvme::NvmeSSD> nvmeSSD,
                           phosphor::nvme::Nvme::NVMeConfig config,
                           bool success,
                           phosphor::nvme::Nvme::NVMeData nvmeData)
{
    if (success)
    {
        if (!nvmeData.smartWarnings.empty())
        {
            nvmeSSD->assertFaultLog(std::stoi(nvmeData.smartWarnings, 0, 16));
            auto request = (strcmp(nvmeData.smartWarnings.c_str(), "ff") == 0)
                               ? false
                               : true;
            nvmeSSD->checkAssertFaultLED(config.locateLedGroupPath,
                                    config.faultLedGroupPath, request);
            nvmeSSD->checkAssertLocateLED(config.locateLedGroupPath,
                                     config.locateLedControllerBusName,
                                     config.locateLedControllerPath, !request);
        }
    }
    else
    {
        // Drive is present but can not get data, turn on fault LED.
        log<level::ERR>("Drive status is good but can not get data.");
        nvmeSSD->checkAssertFaultLED(config.locateLedGroupPath,
                                config.faultLedGroupPath, true);
        nvmeSSD->checkAssertLocateLED(config.locateLedGroupPath,
                                 config.locateLedControllerBusName,
                                 config.locateLedControllerPath, false);
    }
}



void Nvme::read()
{

    try
    {
        for (int i = 0; i < configs.size(); i++)
        {
            NVMeData nvmeData;
            std::string devPresentPath =
                GPIO_BASE_PATH + std::to_string(configs[i].presentPin) + "/value";

            std::string devPwrGoodPath =
                GPIO_BASE_PATH + std::to_string(configs[i].pwrGoodPin) + "/value";

            auto iter = nvmes.find(std::to_string(configs[i].index));

            if (getValue(devPresentPath) == IS_PRESENT)
            {
                // Drive status is good, update value or create d-bus and update
                // value.
                if (getValue(devPwrGoodPath) == POWERGD)
                {
                    // get NVMe information through i2c by busID.
                    auto success = getNVMeInfobyBusID(configs[i].busID, nvmeData);
                    // can not find. create dbus
                    if (iter == nvmes.end())
                    {
                        std::string objPath =
                            NVME_OBJ_PATH + std::to_string(configs[i].index);
                        auto nvmeSSD = std::make_shared<phosphor::nvme::NvmeSSD>(
                            bus, objPath.c_str());
                        nvmes.emplace(std::to_string(configs[i].index), nvmeSSD);
                        nvmeSSD->setPresent(true);
                        nvmeSSD->setPropertiesToDbus(
                            nvmeData.sensorValue, nvmeData.vendor,
                            nvmeData.serialNumber, nvmeData.smartWarnings,
                            nvmeData.statusFlags, nvmeData.driveLifeUsed);

                        nvmeSSD->setSensorThreshold(
                            configs[i].criticalHigh, configs[i].criticalLow,
                            configs[i].maxValue, configs[i].minValue);

                        nvmeSSD->checkSensorThreshold();
                        setSSDLEDStatus(nvmeSSD, configs[i], success, nvmeData);
                    }
                    else
                    {
                        iter->second->setPresent(true);
                        iter->second->setPropertiesToDbus(
                            nvmeData.sensorValue, nvmeData.vendor,
                            nvmeData.serialNumber, nvmeData.smartWarnings,
                            nvmeData.statusFlags, nvmeData.driveLifeUsed);

                        iter->second->checkSensorThreshold();
                        setSSDLEDStatus(iter->second, configs[i], success,
                                        nvmeData);
                    }
                }
                else
                {
                    // Present pin is true but power good pin is false
                    log<level::ERR>(
                        "Present pin is true but power good pin is false");
                    if (iter != nvmes.end())
                    {
                        iter->second->checkAssertFaultLED(
                            configs[i].locateLedGroupPath,
                            configs[i].faultLedGroupPath, true);
                        iter->second->checkAssertLocateLED(
                            configs[i].locateLedGroupPath,
                            configs[i].locateLedControllerBusName,
                            configs[i].locateLedControllerPath, false);
                    }
                    nvmes.erase(std::to_string(configs[i].index));
                }
            }
            else
            {
                // Drive not present, remove nvme d-bus path and turn off fault and
                // locate LED
                if (iter != nvmes.end())
                {
                    iter->second->checkAssertFaultLED(configs[i].locateLedGroupPath,
                                                    configs[i].faultLedGroupPath,
                                                    false);
                    iter->second->checkAssertLocateLED(
                        configs[i].locateLedGroupPath,
                        configs[i].locateLedControllerBusName,
                        configs[i].locateLedControllerPath, false);
                }
                nvmes.erase(std::to_string(configs[i].index));
            }
        }

    }
    catch (const std::exception &e)
    {
        log<level::ERR>("Error in read loop", entry("ERROR=%s", e.what()));
    }
}
} // namespace nvme
} // namespace phosphor

/*
 * clipper - https://github.com/Rosalie241/clipper
 *  Copyright (C) 2024 Rosalie Wanders <rosalie@mailbox.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

//
// Includes
//

#include <windows.h>
#include <hidusage.h>

#include <filesystem>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>
#include <mutex>

#include <ViGEm/Client.h>
#include <INIReader.h>
#include <hidapi.h>

#include "guitar.hpp"
#include "drum.hpp"

//
// Global Variables
//

bool IsRunning = true;

//
// Local Variables
//

static bool l_CleanedUp = false;

static std::mutex l_OpenedDevicesMutex;
static std::vector<std::string> l_OpenedDevices;
static std::mutex l_ClosedDevicesMutex;
static std::vector<hid_device*> l_ClosedDevices;
static std::vector<std::thread> l_PollThreads;

//
// Local Functions
//

static void show_error(const char* fmt, ...)
{
    char error[1024] = { 0 };
    va_list args;
    va_start(args, fmt);
    vsnprintf(error, sizeof(error), fmt, args);
    va_end(args);

    puts(error);

    puts("Press a key to continue...");
    (void)getchar();
}

static BOOL WINAPI signal_handler(DWORD signal)
{
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT)
    {
        IsRunning = false;

        while (!l_CleanedUp)
        {
            Sleep(250);
        }
    }

    return TRUE;
}

static bool is_valid_device(hid_device_info* deviceInfo, std::string& deviceName, DeviceType& deviceType, bool& hasPickupSwitch)
{
    // Thank you @TheNathannator for helping me with this
    if (deviceInfo->usage_page != HID_USAGE_PAGE_GENERIC ||
        deviceInfo->usage != HID_USAGE_GENERIC_GAMEPAD)
    {
        return false;
    }

    return IsValidPS4Guitar(deviceInfo, deviceName, deviceType, hasPickupSwitch) ||
           IsValidPS5Guitar(deviceInfo, deviceName, deviceType) ||
           IsValidPS4Drum(deviceInfo, deviceName, deviceType);
}

static bool has_device_open(hid_device_info* deviceInfo)
{
    const std::lock_guard<std::mutex> lock(l_OpenedDevicesMutex);
    return std::find(l_OpenedDevices.begin(), l_OpenedDevices.end(), deviceInfo->path) != l_OpenedDevices.end();
}

static hid_device* open_device(hid_device_info* deviceInfo)
{
    const std::lock_guard<std::mutex> lock(l_OpenedDevicesMutex);
    hid_device* device = hid_open_path(deviceInfo->path);
    if (device == nullptr)
    {
        wprintf(L"[WARNING] Failed to open device: %ls\n", hid_error(device));
    }
    else
    {
        l_OpenedDevices.push_back(deviceInfo->path);
    }
    return device;
}

static void close_devices()
{
    const std::lock_guard<std::mutex> lock(l_ClosedDevicesMutex);
    for (const auto& device : l_ClosedDevices)
    {
        hid_close(device);
    }
    l_ClosedDevices.clear();
}

static void launch_poll_thread(PVIGEM_CLIENT client, hid_device* device, std::string devicePath, DeviceType deviceType, GuitarDeviceConfiguration configuration)
{
    switch (deviceType)
    {
    case DeviceType::PS4Guitar:
        l_PollThreads.push_back(std::thread(PS4GuitarPollInputThread, client, device, devicePath, configuration));
        break;
    case DeviceType::PS5Guitar:
        l_PollThreads.push_back(std::thread(PS5GuitarPollInputThread, client, device, devicePath, configuration));
        break;
    case DeviceType::PS4Drum:
        l_PollThreads.push_back(std::thread(PS4DrumPollInputThread, client, device, devicePath));
        break;
    default:
        break;
    }
}

static bool write_default_config_file()
{
    static const char defaultConfiguration[] =
        ";\n"
        "; clipper - https://github.com/Rosalie241/clipper\n"
        ";\n"
        "[PDP Riffmaster]\n"
        "TiltSensitivity = 130\n"
        "TiltDeadZone = 20\n"
        "\n"
        "[PDP Jaguar]\n"
        "TiltSensitivity = 130\n"
        "TiltDeadZone = 20\n"
        "\n"
        "[MadCatz Stratocaster]\n"
        "TiltSensitivity = 130\n"
        "TiltDeadZone = 20\n";

    FILE* configFile = nullptr;
    errno_t err = fopen_s(&configFile, "clipper.ini", "w+");
    if (err != 0 || configFile == nullptr)
    {
        show_error("[ERROR] Failed to create clipper.ini!");
        return false;
    }

    puts("[INFO] Created clipper.ini with the default configuration");
    fwrite(defaultConfiguration, strlen(defaultConfiguration), 1, configFile);
    fclose(configFile);
    return true;
}

static GuitarDeviceConfiguration get_configuration(INIReader& reader, std::string& deviceName, bool hasPickupSwitch)
{
    GuitarDeviceConfiguration configuration;
    configuration.HasPickupSwitch = hasPickupSwitch;
    configuration.TiltSensitivity = reader.GetInteger(deviceName, "TiltSensitivity", 130);
    configuration.TiltDeadZone = reader.GetInteger(deviceName, "TiltDeadZone", 20);
    return configuration;
}

//
// Exported Functions
//

void RemoveDevice(const std::string& devicePath, hid_device* device)
{
    l_OpenedDevicesMutex.lock();
    const auto iter = std::find(l_OpenedDevices.begin(), l_OpenedDevices.end(), devicePath);
    if (iter != l_OpenedDevices.end())
    {
        l_OpenedDevices.erase(iter);
    }
    l_OpenedDevicesMutex.unlock();

    l_ClosedDevicesMutex.lock();
    l_ClosedDevices.push_back(device);
    l_ClosedDevicesMutex.unlock();
}

int main()
{
    // set console handler
    if (!SetConsoleCtrlHandler(signal_handler, TRUE))
    {
        show_error("[ERROR] Failed to set console handler!");
        return 1;
    }

    // initialize configuration file
    if (!std::filesystem::is_regular_file("clipper.ini"))
    {
        if (!write_default_config_file())
        {
            return 1;
        }
    }
    INIReader iniReader("clipper.ini");
    if (iniReader.ParseError() != 0)
    {
        show_error("[ERROR] Failed to parse clipper.ini: %s", iniReader.ParseErrorMessage().c_str());
        return 1;
    }

    // initialize ViGEm
    PVIGEM_CLIENT client = vigem_alloc();
    if (client == nullptr)
    {
        show_error("[ERROR] Failed to allocate memory for ViGEm!");
        return 1;
    }

    VIGEM_ERROR vigem_ret = vigem_connect(client);
    if (vigem_ret != VIGEM_ERROR_NONE)
    {
        show_error("[ERROR] Failed to connect to ViGEm driver!");
        vigem_free(client);
        return 1;
    }

    // initialize libhidapi
    int hid_ret = hid_init();
    if (hid_ret < 0)
    {
        show_error("[ERROR] Failed to initialize libhidapi!");
        vigem_free(client);
        return 1;
    }

    puts("[INFO] Waiting for devices...");
    while (IsRunning)
    {
        hid_device_info* devices = hid_enumerate(0, 0);
        hid_device_info* deviceInfo = devices;
        std::string deviceName;
        DeviceType deviceType;
        bool hasPickupSwitch = false;

        while (deviceInfo->next)
        {
            if (is_valid_device(deviceInfo, deviceName, deviceType, hasPickupSwitch) &&
                !has_device_open(deviceInfo))
            {   
                printf("[INFO] Found device: %s at %s\n", deviceName.c_str(), deviceInfo->path);

                hid_device* hidDevice = open_device(deviceInfo);
                if (hidDevice != nullptr)
                {
                    printf("[INFO] Opened device: %s, starting poll thread...\n", deviceName.c_str());
                    launch_poll_thread(client, hidDevice, deviceInfo->path, deviceType, get_configuration(iniReader, deviceName, hasPickupSwitch));
                }
            }

            deviceInfo = deviceInfo->next;
        }

        // close devices for threads that
        // are no longer running
        close_devices();

        hid_free_enumeration(devices);

        Sleep(2000);
    }

    puts("[INFO] Shutting down...");

    // wait for all threads to finish executing
    for (auto& pollThread : l_PollThreads)
    {
        if (pollThread.joinable())
        {
            pollThread.join();
        }
    }
    vigem_free(client);

    // close remaining devices
    close_devices();
    hid_exit();

    // needed for signal handler
    l_CleanedUp = true;

    return 0;
}

/*
 * clipper - https://github.com/Rosalie241/clipper
 *  Copyright (C) 2024 Rosalie Wanders <rosalie@mailbox.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef GUITAR_HPP
#define GUITAR_HPP

#include <ViGEm/Client.h>
#include <hidapi.h>

#include <string>
#include "types.hpp"

struct GuitarDeviceConfiguration
{
    int TiltSensitivity = 0;
    int TiltDeadZone = 0;
    bool HasPickupSwitch = false;
};

// returns whether device is a valid PS4 guitar
bool IsValidPS4Guitar(hid_device_info* deviceInfo, std::string& deviceName, DeviceType& deviceType, bool& hasPickupSwitch);

// returns whether device is a valid PS5 guitar
bool IsValidPS5Guitar(hid_device_info* deviceInfo, std::string& deviceName, DeviceType& deviceType);

// input thread for PS4 guitars
void PS4GuitarPollInputThread(PVIGEM_CLIENT client, hid_device* device, std::string devicePath, GuitarDeviceConfiguration configuration);

// input thread for PS5 guitars
void PS5GuitarPollInputThread(PVIGEM_CLIENT client, hid_device* device, std::string devicePath, GuitarDeviceConfiguration configuration);

#endif // GUITAR_HPP
/*
 * clipper - https://github.com/Rosalie241/clipper
 *  Copyright (C) 2024 Rosalie Wanders <rosalie@mailbox.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include <windows.h>

#include "guitar.hpp"
#include "clipper.hpp"

//
// Local Enums
//

enum
{
    PS4_RIFFMASTER_VENDOR_ID  = 0x0E6F,
    PS4_RIFFMASTER_PRODUCT_ID = 0x024A,

    PS4_JAGUAR_VENDOR_ID  = 0x0E6F,
    PS4_JAGUAR_PRODUCT_ID = 0x0173,

    PS4_STRATOCASTER_VENDOR_ID  = 0x0738,
    PS4_STRATOCASTER_PRODUCT_ID = 0x8261,

    PS4_GIBSONSG_VENDOR_ID  = 0x3651,
    PS4_GIBSONSG_PRODUCT_ID = 0x5500,
};

enum
{
    BTN_MASK_FRET_1 = 0b00000001,
    BTN_MASK_FRET_2 = 0b00000010,
    BTN_MASK_FRET_3 = 0b00000100,
    BTN_MASK_FRET_4 = 0b00001000,
    BTN_MASK_FRET_5 = 0b00010000,

    BTN_MASK_DPAD   = 0b00001111,

    BTN_MASK_STICK  = 0b01000000,
    BTN_MASK_START  = 0b00100000,
    BTN_MASK_SELECT = 0b00010000,
    BTN_MASK_HOME   = 0b00000001,
};

enum
{
    BUF_STICK_X = 1,
    BUF_STICK_Y = 2,

    BUF_DPAD        = 5,
    BUF_SYSTEM_BTNS = 6,
    BUF_PS_BTN      = 7,

    BUF_PICKUP      = 43,
    BUF_WHAMMY      = 44,
    BUF_TILT        = 45,
    BUF_FRETS       = 46,
    BUF_LOWER_FRETS = 47,
};

//
// Local Types
//

struct GuitarDevice
{
    int VendorId  = 0;
    int ProductId = 0;
    std::string ProductName;
    bool HasPickupSwitch = false;
};

//
// Local Variables
//

static const GuitarDevice l_SupportedGuitarDevices[] =
{
    {PS4_RIFFMASTER_VENDOR_ID  , PS4_RIFFMASTER_PRODUCT_ID  , "PDP Riffmaster"      , false},
    {PS4_JAGUAR_VENDOR_ID      , PS4_JAGUAR_PRODUCT_ID      , "PDP Jaguar"          , false},
    {PS4_STRATOCASTER_VENDOR_ID, PS4_STRATOCASTER_PRODUCT_ID, "MadCatz Stratocaster", true},
    {PS4_GIBSONSG_VENDOR_ID    , PS4_GIBSONSG_PRODUCT_ID    , "CRKD Gibson SG"      , false},
};

//
// Exported Functions
//

bool IsValidGuitar(hid_device_info* deviceInfo, std::string& deviceName, DeviceType& deviceType, bool& hasPickupSwitch)
{
    for (const GuitarDevice& guitarDevice : l_SupportedGuitarDevices)
    {
        if (deviceInfo->product_id == guitarDevice.ProductId &&
            deviceInfo->vendor_id == guitarDevice.VendorId)
        {
            hasPickupSwitch = guitarDevice.HasPickupSwitch;
            deviceName = guitarDevice.ProductName;
            deviceType = DeviceType::Guitar;
            return true;
        }
    }

    return false;
}

void GuitarPollInputThread(PVIGEM_CLIENT client, hid_device* device, std::string devicePath, GuitarDeviceConfiguration configuration)
{
    int ret;
    unsigned char buffer[64];
    XUSB_REPORT virtual_report = { 0 };
    const BYTE pickup_values[] =
    {
        0xE0, 0xAB, 0x79, 0x4B, 0x17
    };
    const BYTE dpad_values[]
    {
        0x1, 0x9, 0x8, 0xA,
        0x2, 0x6, 0x4, 0x5,
        // padding to prevent overflow
        0x0, 0x0, 0x0, 0x0,
        0x0, 0x0, 0x0, 0x0
    };
    const double tilt_sensitivity = configuration.TiltSensitivity / 100.0;
    const SHORT tilt_deadzone = static_cast<SHORT>(MAXSHORT * (configuration.TiltDeadZone / 100.0));

    PVIGEM_TARGET gamepad = vigem_target_x360_alloc();
    if (gamepad == nullptr)
    {
        puts("[ERROR] Failed to allocate virtual gamepad!");
        RemoveDevice(devicePath, device);
        vigem_free(client);
        return;
    }

    // set vendor and product ID to match
    // what rb4instrumentmapper provides
    vigem_target_set_vid(gamepad, 0x1BAD);
    vigem_target_set_pid(gamepad, 0x0719);

    VIGEM_ERROR vigem_ret = vigem_target_add(client, gamepad);
    if (vigem_ret != VIGEM_ERROR_NONE)
    {
        puts("[ERROR] Failed to add virtual gamepad to ViGEm!");
        RemoveDevice(devicePath, device);
        vigem_target_free(gamepad);
        return;
    }

    while (IsRunning)
    {
        ret = hid_read(device, buffer, sizeof(buffer));
        if (ret != sizeof(buffer))
        {
            printf("[ERROR] Failed to read packets for %s!\n", devicePath.c_str());
            break;
        }

        // we only care about the last 4 bits for the dpad
        buffer[BUF_DPAD] &= BTN_MASK_DPAD;
        // lower frets buffer matches the frets buffer
        buffer[BUF_FRETS] |= buffer[BUF_LOWER_FRETS];

        virtual_report.wButtons = ((buffer[BUF_FRETS] & BTN_MASK_FRET_1) << 12) |
            ((buffer[BUF_FRETS] & BTN_MASK_FRET_2) << 12) |
            ((buffer[BUF_FRETS] & BTN_MASK_FRET_3) << 13) |
            ((buffer[BUF_FRETS] & BTN_MASK_FRET_4) << 11) |
            ((buffer[BUF_FRETS] & BTN_MASK_FRET_5) << 4) |
            ((buffer[BUF_SYSTEM_BTNS] & BTN_MASK_STICK)) |
            ((buffer[BUF_SYSTEM_BTNS] & BTN_MASK_START) >> 1) |
            ((buffer[BUF_SYSTEM_BTNS] & BTN_MASK_SELECT) << 1) |
            ((buffer[BUF_PS_BTN] & BTN_MASK_HOME) << 10) |
            ((dpad_values[buffer[BUF_DPAD]]));

        virtual_report.sThumbRX = ((buffer[BUF_WHAMMY] * 255) - 32767);

        // account for deadzone
        const SHORT tilt_value = static_cast<SHORT>(min((buffer[BUF_TILT] * (128 * tilt_sensitivity)), MAXSHORT));
        virtual_report.sThumbRY = (tilt_value < tilt_deadzone ? 0 : tilt_value);

        virtual_report.sThumbLX = ((buffer[BUF_STICK_X] * 255) - 32767);
        virtual_report.sThumbLY = ((buffer[BUF_STICK_Y] * 255) - 32767);

        if (configuration.HasPickupSwitch)
        {
            virtual_report.bLeftTrigger = (buffer[BUF_PICKUP] >= sizeof(pickup_values)) 
                                            ? 0
                                            : pickup_values[buffer[BUF_PICKUP]];
        }

        vigem_target_x360_update(client, gamepad, virtual_report);
    }

    // remove allocated devices on thread exit
    vigem_target_remove(client, gamepad);
    vigem_target_free(gamepad);

    // remove device from opened devices list
    RemoveDevice(devicePath, device);
}

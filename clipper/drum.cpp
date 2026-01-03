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
#include "drum.hpp"
#include "clipper.hpp"

 //
 // Local Enums
 //

enum
{
    PS4_MADCATZ_DRUMSET_VENDOR_ID = 0x0738,
    PS4_MADCATZ_DRUMSET_PRODUCT_ID = 0x8262,

    PS4_PDP_DRUMSET_VENDOR_ID = 0x0E6F,
    PS4_PDP_DRUMSET_PRODUCT_ID = 0x0174,
};

enum
{
    BTN_MASK_KICK_1 = 0b00000001,
    BTN_MASK_KICK_2 = 0b00000010,

    BTN_MASK_START  = 0b00100000,
    BTN_MASK_SELECT = 0b00010000,

    BTN_MASK_SQUARE   = 0b00010000,
    BTN_MASK_CROSS    = 0b00100000,
    BTN_MASK_CIRCLE   = 0b01000000,
    BTN_MASK_TRIANGLE = 0b10000000,

    BTN_MASK_GUIDE = 0b00000001,

    BTN_MASK_DPAD = 0b00001111,
};

enum
{
    BUF_FACE_BTNS = 5,
    BUF_KICK = 6,

    BUF_GUIDE = 7,

    BUF_DRUM_RED = 43,
    BUF_DRUM_BLUE = 44,
    BUF_DRUM_YELLOW = 45,
    BUF_DRUM_GREEN = 46,

    BUF_CYMBAL_YELLOW = 47,
    BUF_CYMBAL_BLUE = 48,
    BUF_CYMBAL_GREEN = 49,
};

//
// Local Types
//

struct DrumDevice
{
    int VendorId = 0;
    int ProductId = 0;
    std::string ProductName;
};

//
// Local Variables
//

static const DrumDevice l_SupportedDrumDevices[] =
{
    {PS4_MADCATZ_DRUMSET_VENDOR_ID, PS4_MADCATZ_DRUMSET_PRODUCT_ID, "MadCatz Drum Set"},
    {PS4_PDP_DRUMSET_VENDOR_ID, PS4_PDP_DRUMSET_PRODUCT_ID, "PDP Drum Set"}
};

//
// Exported Functions
//

bool IsValidDrum(hid_device_info* deviceInfo, std::string& deviceName, DeviceType& deviceType)
{
    for (const DrumDevice& drumDevice : l_SupportedDrumDevices)
    {
        if (deviceInfo->product_id == drumDevice.ProductId &&
            deviceInfo->vendor_id == drumDevice.VendorId)
        {
            deviceName = drumDevice.ProductName;
            deviceType = DeviceType::Drum;
            return true;
        }
    }

    return false;
}

void DrumPollInputThread(PVIGEM_CLIENT client, hid_device* device, std::string devicePath)
{
    int ret;
    unsigned char buffer[64];
    XUSB_REPORT virtual_report = { 0 };
    const BYTE dpad_values[] =
    {
        0x1, 0x9, 0x8, 0xA,
        0x2, 0x6, 0x4, 0x5,
        // padding to prevent overflow
        0x0, 0x0, 0x0, 0x0,
        0x0, 0x0, 0x0, 0x0
    };

    PVIGEM_TARGET gamepad = vigem_target_x360_alloc();
    if (gamepad == nullptr)
    {
        puts("[ERROR] Failed to allocate virtual gamepad!");
        RemoveDevice(devicePath, device);
        vigem_free(client);
        return;
    }

    // set vendor and product ID to match
    // what RB4InstrumentMapper provides
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

        // sadly we cannot use bitshifts for most of these,
        // the drum values seem to change randomly so we're
        // just going to use old-school if statements even though
        // that's likely to be a little bit slower
        virtual_report.wButtons = 0;
        if (buffer[BUF_DRUM_RED])
            virtual_report.wButtons |= XUSB_GAMEPAD_B;
        if (buffer[BUF_DRUM_BLUE])
            virtual_report.wButtons |= XUSB_GAMEPAD_X;
        if (buffer[BUF_DRUM_YELLOW])
            virtual_report.wButtons |= XUSB_GAMEPAD_Y;
        if (buffer[BUF_DRUM_GREEN])
            virtual_report.wButtons |= XUSB_GAMEPAD_A;

        if (buffer[BUF_CYMBAL_YELLOW])
            virtual_report.wButtons |= XUSB_GAMEPAD_Y | XUSB_GAMEPAD_RIGHT_SHOULDER;
        if (buffer[BUF_CYMBAL_BLUE])
            virtual_report.wButtons |= XUSB_GAMEPAD_X | XUSB_GAMEPAD_RIGHT_SHOULDER;
        if (buffer[BUF_CYMBAL_GREEN])
            virtual_report.wButtons |= XUSB_GAMEPAD_A | XUSB_GAMEPAD_RIGHT_SHOULDER;

        if (buffer[BUF_CYMBAL_YELLOW])
            virtual_report.wButtons |= XUSB_GAMEPAD_DPAD_UP;
        else if (buffer[BUF_CYMBAL_BLUE])
            virtual_report.wButtons |= XUSB_GAMEPAD_DPAD_DOWN;

        if (buffer[BUF_KICK] & BTN_MASK_KICK_1)
            virtual_report.wButtons |= XUSB_GAMEPAD_LEFT_SHOULDER;
        if (buffer[BUF_KICK] & BTN_MASK_KICK_2)
            virtual_report.wButtons |= XUSB_GAMEPAD_LEFT_THUMB;

        if (buffer[BUF_FACE_BTNS] & BTN_MASK_SQUARE)
            virtual_report.wButtons |= XUSB_GAMEPAD_X;
        if (buffer[BUF_FACE_BTNS] & BTN_MASK_CROSS)
            virtual_report.wButtons |= XUSB_GAMEPAD_A;
        if (buffer[BUF_FACE_BTNS] & BTN_MASK_CIRCLE)
            virtual_report.wButtons |= XUSB_GAMEPAD_B;
        if (buffer[BUF_FACE_BTNS] & BTN_MASK_TRIANGLE)
            virtual_report.wButtons |= XUSB_GAMEPAD_Y;

        if (buffer[BUF_KICK] & BTN_MASK_SELECT)
            virtual_report.wButtons |= XUSB_GAMEPAD_BACK;
        if (buffer[BUF_KICK] & BTN_MASK_START)
            virtual_report.wButtons |= XUSB_GAMEPAD_START;

        if (buffer[BUF_GUIDE] & BTN_MASK_GUIDE)
            virtual_report.wButtons |= XUSB_GAMEPAD_GUIDE;

        virtual_report.wButtons |= dpad_values[(buffer[BUF_FACE_BTNS] & BTN_MASK_DPAD)];

        vigem_target_x360_update(client, gamepad, virtual_report);
    }

    // remove allocated devices on thread exit
    vigem_target_remove(client, gamepad);
    vigem_target_free(gamepad);

    // remove device from opened devices list
    RemoveDevice(devicePath, device);
}

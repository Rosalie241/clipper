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
#include <cstdio>

#include <hidapi.h>
#include <ViGEm/Client.h>

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
};

enum
{
    BTN_MASK_FRET_1     = 0b00000001,
    BTN_MASK_FRET_2     = 0b00000010,
    BTN_MASK_FRET_3     = 0b00000100,
    BTN_MASK_FRET_4     = 0b00001000,
    BTN_MASK_FRET_5     = 0b00010000,

    BTN_MASK_DPAD_UP    = 0b00000000,
    BTN_MASK_DPAD_DOWN  = 0b00000100,
    BTN_MASK_DPAD_LEFT  = 0b00000110,
    BTN_MASK_DPAD_RIGHT = 0b00000010,

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
// Local Structs
//

struct GuitarDevice
{
    int VendorId  = 0;
    int ProductId = 0;
    const char ProductName[24] = { 0 };
    bool HasPickupSwitch = false;
};

struct GuitarDeviceHandle
{
    hid_device* HidDevice = nullptr;
    bool HasPickupSwitch  = false;
};

//
// Local Variables
//

static bool l_Running = true;
static bool l_CleanedUp = false;
static GuitarDevice l_SupportedDevices[] =
{
    {PS4_RIFFMASTER_VENDOR_ID  , PS4_RIFFMASTER_PRODUCT_ID  , "PDP Riffmaster"      , false},
    {PS4_JAGUAR_VENDOR_ID      , PS4_JAGUAR_PRODUCT_ID      , "PDP Jaguar"          , false},
    {PS4_STRATOCASTER_VENDOR_ID, PS4_STRATOCASTER_PRODUCT_ID, "MadCatz Stratocaster", true}
};

//
// Local Functions
//

static void show_error(const char* error)
{
    puts(error);

    puts("Press a key to continue...");
    (void)getchar();
}

static BOOL WINAPI signal_handler(DWORD signal)
{
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT)
    {
        l_Running = false;

        while (!l_CleanedUp)
        {
            Sleep(250);
        }
    }

    return TRUE;
}

static bool find_device(GuitarDeviceHandle& deviceHandle)
{
    puts("[INFO] Waiting for device...");

    while (l_Running)
    {
        for (const GuitarDevice& guitarDevice : l_SupportedDevices)
        {
            deviceHandle.HidDevice = hid_open(guitarDevice.VendorId, guitarDevice.ProductId, nullptr);
            if (deviceHandle.HidDevice != nullptr)
            {
                printf("[INFO] Device found: %s, polling input...\n", guitarDevice.ProductName);
                deviceHandle.HasPickupSwitch = guitarDevice.HasPickupSwitch;
                return true;
            }
        }

        // while we're waiting, dont waste too many cycles
        Sleep(250);
    }

    return false;
}

static void poll_input(GuitarDeviceHandle& deviceHandle, PVIGEM_CLIENT client, PVIGEM_TARGET gamepad)
{
    int ret;
    unsigned char buffer[64];
    XUSB_REPORT virtual_report = { 0 };
    const BYTE pickup_values[] =
    {
        0xE0, 0xAB, 0x79, 0x4B, 0x17
    };

    while (l_Running)
    {
        ret = hid_read(deviceHandle.HidDevice, buffer, sizeof(buffer));
        if (ret != sizeof(buffer))
        {
            wprintf(L"[WARNING] Failed to read packets: %ls\n", hid_error(deviceHandle.HidDevice));
            break;
        }

        // we only care about the last 4 bits for the dpad
        buffer[BUF_DPAD] &= 0b00001111;
        // lower frets buffer matches the frets buffer
        buffer[BUF_FRETS] |= buffer[BUF_LOWER_FRETS];

        virtual_report.wButtons = ((buffer[BUF_FRETS] & BTN_MASK_FRET_1) << 12) |
                                  ((buffer[BUF_FRETS] & BTN_MASK_FRET_2) << 12) |
                                  ((buffer[BUF_FRETS] & BTN_MASK_FRET_3) << 13) |
                                  ((buffer[BUF_FRETS] & BTN_MASK_FRET_4) << 11) |
                                  ((buffer[BUF_FRETS] & BTN_MASK_FRET_5) << 4)  |
                                  ((buffer[BUF_SYSTEM_BTNS] & BTN_MASK_STICK))       |
                                  ((buffer[BUF_SYSTEM_BTNS] & BTN_MASK_START)  >> 1) |
                                  ((buffer[BUF_SYSTEM_BTNS] & BTN_MASK_SELECT) << 1) |
                                  ((buffer[BUF_PS_BTN] & BTN_MASK_HOME) << 10) |
                                  ((buffer[BUF_DPAD] == BTN_MASK_DPAD_UP))         |
                                  ((buffer[BUF_DPAD] == BTN_MASK_DPAD_DOWN)  << 1) |
                                  ((buffer[BUF_DPAD] == BTN_MASK_DPAD_LEFT)  << 2) |
                                  ((buffer[BUF_DPAD] == BTN_MASK_DPAD_RIGHT) << 3);

        virtual_report.sThumbRX = ((buffer[BUF_WHAMMY] * 255) - 32767);
        virtual_report.sThumbRY = min((buffer[BUF_TILT] * (128 * 1.3)), 32767);

        virtual_report.sThumbLX = ((buffer[BUF_STICK_X] * 255) - 32767);
        virtual_report.sThumbLY = ((buffer[BUF_STICK_Y] * 255) - 32767);

        if (deviceHandle.HasPickupSwitch)
        {
            virtual_report.bLeftTrigger = pickup_values[(buffer[BUF_PICKUP])];
        }

        vigem_target_x360_update(client, gamepad, virtual_report);
    }
}

//
// Exported Functions
//

int main()
{
    // set console handler
    if (!SetConsoleCtrlHandler(signal_handler, TRUE))
    {
        show_error("[ERROR] Failed to set console handler!");
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

    PVIGEM_TARGET gamepad = vigem_target_x360_alloc();
    if (gamepad == nullptr)
    {
        show_error("[ERROR] Failed to allocate virtual gamepad!");
        vigem_free(client);
        return 1;
    }

    vigem_ret = vigem_target_add(client, gamepad);
    if (vigem_ret != VIGEM_ERROR_NONE)
    {
        show_error("[ERROR] Failed to add virtual gamepad to ViGEm!");
        vigem_target_free(gamepad);
        vigem_free(client);
        return 1;
    }

    // initialize libhidapi
    int hid_ret = hid_init();
    if (hid_ret < 0)
    {
        show_error("[ERROR] Failed to initialize libhidapi!");
        vigem_target_remove(client, gamepad);
        vigem_target_free(gamepad);
        vigem_free(client);
        return 1;
    }

    GuitarDeviceHandle deviceHandle;
    while (l_Running)
    {
        if (find_device(deviceHandle))
        {
            poll_input(deviceHandle, client, gamepad);
        }
    }

    puts("[INFO] Shutting down...");

    vigem_target_remove(client, gamepad);
    vigem_target_free(gamepad);
    vigem_free(client);

    hid_close(deviceHandle.HidDevice);
    hid_exit();

    // needed for signal handler
    l_CleanedUp = true;

    return 0;
}

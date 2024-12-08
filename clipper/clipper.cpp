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

#ifdef _WIN32
#include <windows.h>
#else
#include <thread>
#define Sleep(x) std::this_thread::sleep_for(std::chrono::milliseconds(x))
#include <signal.h>


#endif
#include <cstdio>

#define min(x, y) (x) < (y) ? (x) : (y)


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
    BTN_MASK_FRET_2_3_4 = BTN_MASK_FRET_2 | BTN_MASK_FRET_3 | BTN_MASK_FRET_4,
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
};

//
// Local Variables
//

static bool l_Running = true;
static bool l_CleanedUp = false;
static GuitarDevice l_SupportedDevices[] =
{
    {PS4_RIFFMASTER_VENDOR_ID  , PS4_RIFFMASTER_PRODUCT_ID  , "PDP Riffmaster"},
    {PS4_JAGUAR_VENDOR_ID      , PS4_JAGUAR_PRODUCT_ID      , "PDP Jaguar"},
    {PS4_STRATOCASTER_VENDOR_ID, PS4_STRATOCASTER_PRODUCT_ID, "MadCatz Stratocaster"}
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

#ifdef _WIN32
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
#else // Linux
static void signal_handler(int signal)
{
    l_Running = false;
}
#endif

static hid_device* find_device(void)
{
    hid_device* device = nullptr;

    puts("[INFO] Waiting for device...");

    while (l_Running)
    {
        for (const GuitarDevice& guitarDevice : l_SupportedDevices)
        {
            device = hid_open(guitarDevice.VendorId, guitarDevice.ProductId, nullptr);
            if (device != nullptr)
            {
                printf("[INFO] Device found: %s, polling input...\n", guitarDevice.ProductName);
                return device;
            }
        }

        // while we're waiting, dont waste too many cycles
        Sleep(250);
    }

    return nullptr;
}

static void poll_input(hid_device* device, PVIGEM_CLIENT client, PVIGEM_TARGET gamepad)
{
    int ret;
    unsigned char buffer[64];
    XUSB_REPORT virtual_report = { 0 };

    while (l_Running)
    {
        ret = hid_read(device, buffer, sizeof(buffer));
        if (ret != sizeof(buffer))
        {
            puts("[WARNING] Failed to read packets, device disconnected?");
            break;
        }

        // we only care about the last 3 bits for the dpad
        buffer[BUF_DPAD] &= 0b00000111;
        // lower frets buffer matches the frets buffer
        buffer[BUF_FRETS] |= buffer[BUF_LOWER_FRETS];

        virtual_report.wButtons = ((buffer[BUF_FRETS] & BTN_MASK_FRET_1)     << 8)  |
                                  ((buffer[BUF_FRETS] & BTN_MASK_FRET_2_3_4) << 11) |
                                  ((buffer[BUF_FRETS] & BTN_MASK_FRET_5)     << 5)  |
                                  ((buffer[BUF_SYSTEM_BTNS] & BTN_MASK_STICK))       |
                                  ((buffer[BUF_SYSTEM_BTNS] & BTN_MASK_START)  >> 1) |
                                  ((buffer[BUF_SYSTEM_BTNS] & BTN_MASK_SELECT) << 1) |
                                  ((buffer[BUF_PS_BTN] & BTN_MASK_HOME) << 10) |
                                  ((buffer[BUF_DPAD] == BTN_MASK_DPAD_UP))         |
                                  ((buffer[BUF_DPAD] == BTN_MASK_DPAD_DOWN)  << 1) |
                                  ((buffer[BUF_DPAD] == BTN_MASK_DPAD_LEFT)  << 2) |
                                  ((buffer[BUF_DPAD] == BTN_MASK_DPAD_RIGHT) << 3);

        virtual_report.bLeftTrigger  = buffer[BUF_WHAMMY];
        virtual_report.bRightTrigger = min((int)(buffer[BUF_TILT] * 1.3f), 255);

        virtual_report.sThumbLX = buffer[BUF_STICK_X];
        virtual_report.sThumbLY = buffer[BUF_STICK_Y];

        vigem_target_x360_update(client, gamepad, virtual_report);
    }
}

//
// Exported Functions
//

int main()
{
#ifdef _WIN32
    // set console handler
    if (!SetConsoleCtrlHandler(signal_handler, TRUE))
    {
        show_error("[ERROR] Failed to set console handler!");
        return 1;
    }
#else // Linux
    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);
#endif // _WIN32

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
    hid_device* device = nullptr;
    int hid_ret = hid_init();
    if (hid_ret < 0)
    {
        show_error("[ERROR] Failed to initialize libhidapi!");
        vigem_target_remove(client, gamepad);
        vigem_target_free(gamepad);
        vigem_free(client);
        return 1;
    }

    while (l_Running)
    {
        device = find_device();
        if (device != nullptr)
        {
            poll_input(device, client, gamepad);
        }
    }

    puts("[INFO] Shutting down...");

    vigem_target_remove(client, gamepad);
    vigem_target_free(gamepad);
    vigem_free(client);

    hid_close(device);
    hid_exit();

    // needed for signal handler
    l_CleanedUp = true;

    return 0;
}

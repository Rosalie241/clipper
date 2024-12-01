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
#include <thread>
#include <bitset>

#include <hidapi.h>
#include <ViGEm/Client.h>

//
// Local Enums
//

enum
{
    // Frets
    BTN_MASK_FRET_1 = 0b00000001, // Green
    BTN_MASK_FRET_2 = 0b00000010, // Red
    BTN_MASK_FRET_3 = 0b00000100, // Yellow
    BTN_MASK_FRET_4 = 0b00001000, // Blue
    BTN_MASK_FRET_5 = 0b00010000, // Orange

    // Dpad
    BTN_MASK_DPAD_UP    = 0b00000000, // Up
    BTN_MASK_DPAD_DOWN  = 0b00000100, // Down
    BTN_MASK_DPAD_LEFT  = 0b00000110, // Left
    BTN_MASK_DPAD_RIGHT = 0b00000010, // Right

    // System buttons
    BTN_MASK_STICK  = 0b01000000, // Joystick Button
    BTN_MASK_START  = 0b00100000, // Start Button
    BTN_MASK_SELECT = 0b00010000, // Select Button
    BTN_MASK_HOME   = 0b00000001, // PS Button
};

//
// Local Variables
//

static bool l_Running = true;

//
// Local Functions
//

static BOOL console_handler(DWORD signal)
{
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT)
    {
        l_Running = false;
    }

    return TRUE;
}

//
// Exported Functions
//

int main()
{
    if (!SetConsoleCtrlHandler(console_handler, TRUE))
    {
        return 1;
    }

    hid_device* handle = nullptr;

    int ret = hid_init();

    printf("[INFO] Searching for device...\n");

    while (l_Running)
    {
        handle = hid_open(0x0E6F, 0x024A, nullptr);
        if (handle != nullptr)
        {
            printf("[INFO] Device found!\n");
            break;
        }
        else
        { // while we're waiting, dont waste too many cycles
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    // initialize vigem
    PVIGEM_CLIENT client = vigem_alloc();
    if (client == nullptr)
    {
        printf("[ERROR] vigem_alloc() failed!\n");
    }

    VIGEM_ERROR ret2 = vigem_connect(client);
    if (ret2 != VIGEM_ERROR_NONE)
    {
        printf("[ERROR] vigem_connect() failed!\n");
    }

    PVIGEM_TARGET virtual_pad = vigem_target_x360_alloc();

    ret2 = vigem_target_add(client, virtual_pad);
    if (ret2 != VIGEM_ERROR_NONE)
    {
        printf("[ERROR] vigem_target_add() failed!\n");
    }

    unsigned char buffer[64];

    XUSB_REPORT virtual_report = { 0 };

    /*
        XUSB_GAMEPAD_DPAD_UP    => 0
        XUSB_GAMEPAD_DPAD_DOWN  => 1
        XUSB_GAMEPAD_DPAD_LEFT  => 2
        XUSB_GAMEPAD_DPAD_RIGHT => 3
    
        XUSB_GAMEPAD_START => 4
        XUSB_GAMEPAD_BACK  => 5
        XUSB_GAMEPAD_LEFT_THUMB => 6


        XUSB_GAMEPAD_LEFT_SHOULDER  => 8
        XUSB_GAMEPAD_RIGHT_SHOULDER => 9
        XUSB_GAMEPAD_GUIDE => 10

        XUSB_GAMEPAD_A => 12
        XUSB_GAMEPAD_B => 13
        XUSB_GAMEPAD_X => 14
    */

    while (l_Running)
    {
        ret = hid_read(handle, buffer, sizeof(buffer));
        if (ret == -1 || ret != sizeof(buffer))
        {
            printf("[ERROR] hid_read() failed!\n");
            break;
        }

        buffer[5] &= 0b00001111;

        virtual_report.wButtons = ((buffer[46] & BTN_MASK_FRET_1) << (8-0))  |
                                  ((buffer[46] & BTN_MASK_FRET_2) << (12-1)) |
                                  ((buffer[46] & BTN_MASK_FRET_3) << (13-2)) |
                                  ((buffer[46] & BTN_MASK_FRET_4) << (14-3)) |
                                  ((buffer[46] & BTN_MASK_FRET_5) << (9-4))  |
                                  ((buffer[47] & BTN_MASK_FRET_1) << (8-0))  |
                                  ((buffer[47] & BTN_MASK_FRET_2) << (12-1)) |
                                  ((buffer[47] & BTN_MASK_FRET_3) << (13-2)) |
                                  ((buffer[47] & BTN_MASK_FRET_4) << (14-3)) |
                                  ((buffer[47] & BTN_MASK_FRET_5) << (9-4))  |
                                  ((buffer[6]  & BTN_MASK_STICK)  >> (0)) |
                                  ((buffer[6]  & BTN_MASK_START)  >> (1)) |
                                  ((buffer[6]  & BTN_MASK_SELECT) << (1)) |
                                  ((buffer[7]  & BTN_MASK_HOME)   << (10)) |
                                  ((buffer[5] == 0b0000000000) // thank you @JaxonWasTaken for this :)
                                        + ((buffer[5] == 0b000000100) << 1)
                                        + ((buffer[5] == 0b000000110) << 2)
                                        + ((buffer[5] == 0b000000010) << 3));

        virtual_report.bLeftTrigger  = buffer[44]; // whammy
        virtual_report.bRightTrigger = buffer[45]; // tilt

        virtual_report.sThumbLX = buffer[1];
        virtual_report.sThumbLY = buffer[2];

        vigem_target_x360_update(client, virtual_pad, virtual_report);
    }


    hid_close(handle);
    hid_exit();

    vigem_target_remove(client, virtual_pad);
    vigem_target_free(virtual_pad);
    vigem_free(client);
    return 0;
}

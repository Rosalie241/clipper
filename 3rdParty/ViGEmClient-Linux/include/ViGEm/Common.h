/*
 * clipper - https://github.com/Rosalie241/clipper
 *  Copyright (C) 2024 Rosalie Wanders <rosalie@mailbox.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef VIGEMCLIENT_COMMON_H
#define VIGEMCLIENT_COMMON_H

#include <stdint.h>

typedef int VIGEM_ERROR;

enum VIGEM_ERRORS
{
	VIGEM_ERROR_NONE = 0,
	VIGEM_ERROR_ERR  = 1,
};

enum XUSB_BUTTON
{
    XUSB_GAMEPAD_DPAD_UP        = 0x0001,
    XUSB_GAMEPAD_DPAD_DOWN      = 0x0002,
    XUSB_GAMEPAD_DPAD_LEFT      = 0x0004,
    XUSB_GAMEPAD_DPAD_RIGHT     = 0x0008,
    XUSB_GAMEPAD_START          = 0x0010,
    XUSB_GAMEPAD_BACK           = 0x0020,
    XUSB_GAMEPAD_LEFT_THUMB     = 0x0040,
    XUSB_GAMEPAD_RIGHT_THUMB    = 0x0080,
    XUSB_GAMEPAD_LEFT_SHOULDER  = 0x0100,
    XUSB_GAMEPAD_RIGHT_SHOULDER = 0x0200,
    XUSB_GAMEPAD_GUIDE          = 0x0400,
    XUSB_GAMEPAD_A              = 0x1000,
    XUSB_GAMEPAD_B              = 0x2000,
    XUSB_GAMEPAD_X              = 0x4000,
    XUSB_GAMEPAD_Y              = 0x8000
};

struct XUSB_REPORT
{
    uint16_t wButtons;
    uint8_t bLeftTrigger;
    uint8_t bRightTrigger;
    int16_t sThumbLX;
    int16_t sThumbLY;
    int16_t sThumbRX;
    int16_t sThumbRY;
};

struct VIGEM_TARGET
{
	void* ptr;
};

typedef void* PVIGEM_CLIENT;
typedef struct VIGEM_TARGET* PVIGEM_TARGET;

#endif // VIGEMCLIENT_COMMON_H

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

#include <cstdio>
#include <cstdlib>
#include <libevdev/libevdev-uinput.h>
#include <libevdev/libevdev.h>

#include <ViGEm/Client.h>

//
// Exported Functions
//

PVIGEM_CLIENT vigem_alloc(void)
{
	libevdev* client = libevdev_new();
	if (client == nullptr)
	{
		puts("[ERROR] Failed to create libevdev context!");
		return nullptr;
	}

	const input_absinfo trigger_absinfo = 
	{
		.minimum = 0,
		.maximum = 1023
	};

	const input_absinfo dpad_absinfo = 
	{
		.minimum = -1,
		.maximum = 1
	};

	// initialize controller state
	libevdev_set_name(client, "Xbox 360 Controller (clipper)");

	libevdev_enable_event_type(client, EV_ABS);
	libevdev_enable_event_code(client, EV_ABS, ABS_Z,  &trigger_absinfo);
	libevdev_enable_event_code(client, EV_ABS, ABS_RZ, &trigger_absinfo);
	libevdev_enable_event_code(client, EV_ABS, ABS_HAT0X, &dpad_absinfo);
	libevdev_enable_event_code(client, EV_ABS, ABS_HAT0Y, &dpad_absinfo);

	libevdev_enable_event_type(client, EV_KEY);
	libevdev_enable_event_code(client, EV_KEY, BTN_A, nullptr);
	libevdev_enable_event_code(client, EV_KEY, BTN_B, nullptr);
	libevdev_enable_event_code(client, EV_KEY, BTN_X, nullptr);
	libevdev_enable_event_code(client, EV_KEY, BTN_Y, nullptr);

	libevdev_enable_event_code(client, EV_KEY, BTN_TL, nullptr);
	libevdev_enable_event_code(client, EV_KEY, BTN_TR, nullptr);

	libevdev_enable_event_code(client, EV_KEY, BTN_START, nullptr);
	libevdev_enable_event_code(client, EV_KEY, BTN_BACK, nullptr);
	libevdev_enable_event_code(client, EV_KEY, BTN_THUMBL, nullptr);

	return (PVIGEM_CLIENT)client;
}

void vigem_free(PVIGEM_CLIENT client)
{
	if (client != nullptr)
	{
		libevdev_free((libevdev*)client);
	}
}

VIGEM_ERROR vigem_connect(PVIGEM_CLIENT client)
{
	return VIGEM_ERROR_NONE;
}

PVIGEM_TARGET vigem_target_x360_alloc(void)
{
	PVIGEM_TARGET target = (PVIGEM_TARGET)malloc(sizeof(VIGEM_TARGET));
	target->ptr = nullptr;
	return target;
}

VIGEM_ERROR vigem_target_x360_update(PVIGEM_CLIENT client, PVIGEM_TARGET target, XUSB_REPORT report)
{
#define WRITE_KEY_EVENT(BTN, REPORT_BTN) \
			libevdev_uinput_write_event((libevdev_uinput*)target->ptr, EV_KEY, BTN, report.wButtons & REPORT_BTN)

	WRITE_KEY_EVENT(BTN_A, XUSB_GAMEPAD_A);
	WRITE_KEY_EVENT(BTN_B, XUSB_GAMEPAD_B);
	WRITE_KEY_EVENT(BTN_X, XUSB_GAMEPAD_X);
	WRITE_KEY_EVENT(BTN_Y, XUSB_GAMEPAD_Y);

	WRITE_KEY_EVENT(BTN_TL, XUSB_GAMEPAD_LEFT_SHOULDER);
	WRITE_KEY_EVENT(BTN_TR, XUSB_GAMEPAD_RIGHT_SHOULDER);

	WRITE_KEY_EVENT(BTN_START, XUSB_GAMEPAD_START);
	WRITE_KEY_EVENT(BTN_BACK,  XUSB_GAMEPAD_BACK);

#undef WRITE_KEY_EVENT

	libevdev_uinput_write_event((libevdev_uinput*)target->ptr, EV_ABS, ABS_HAT0X, (report.wButtons & XUSB_GAMEPAD_DPAD_RIGHT) - (report.wButtons & XUSB_GAMEPAD_DPAD_LEFT));
	libevdev_uinput_write_event((libevdev_uinput*)target->ptr, EV_ABS, ABS_HAT0Y, (report.wButtons & XUSB_GAMEPAD_DPAD_DOWN)  - (report.wButtons & XUSB_GAMEPAD_DPAD_UP));

	libevdev_uinput_write_event((libevdev_uinput*)target->ptr, EV_ABS, ABS_Z, report.bLeftTrigger * 4);
	libevdev_uinput_write_event((libevdev_uinput*)target->ptr, EV_ABS, ABS_RZ, report.bRightTrigger * 4);

	int ret = libevdev_uinput_write_event((libevdev_uinput*)target->ptr, EV_SYN, SYN_REPORT, 0);
	if (ret != 0)
	{
		puts("[ERROR] Failed to flush uinput events!");
		return VIGEM_ERROR_ERR;
	}

	return VIGEM_ERROR_NONE;
}

VIGEM_ERROR vigem_target_add(PVIGEM_CLIENT client, PVIGEM_TARGET target)
{
	int ret = libevdev_uinput_create_from_device((libevdev*)client, 
													LIBEVDEV_UINPUT_OPEN_MANAGED, 
													(libevdev_uinput**)&target->ptr);
	if (ret != 0)
	{
		puts("[ERROR] Failed to create uinput device!");
		return VIGEM_ERROR_ERR;
	}

	return VIGEM_ERROR_NONE;
}

void vigem_target_remove(PVIGEM_CLIENT client, PVIGEM_TARGET target)
{
}

void vigem_target_free(PVIGEM_TARGET target)
{
	if (target != nullptr)
	{
		if (target->ptr != nullptr)
		{
			libevdev_uinput_destroy((libevdev_uinput*)target->ptr);
		}
		free(target);
	}
}


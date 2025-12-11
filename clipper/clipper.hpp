/*
 * clipper - https://github.com/Rosalie241/clipper
 *  Copyright (C) 2024 Rosalie Wanders <rosalie@mailbox.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef CLIPPER_HPP
#define CLIPPER_HPP

#include <string>
#include <hidapi.h>

// global variable indicating whether threads should be running
extern bool IsRunning;

// function to remove and close a given device
void RemoveDevice(const std::string& devicePath, hid_device* device);

#endif // CLIPPER_HPP
/*
 * clipper - https://github.com/Rosalie241/clipper
 *  Copyright (C) 2024 Rosalie Wanders <rosalie@mailbox.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef VIGEMCLIENT_CLIENT_H
#define VIGEMCLIENT_CLIENT_H

#include <ViGEm/Common.h>

PVIGEM_CLIENT vigem_alloc(void);
void vigem_free(PVIGEM_CLIENT client);

VIGEM_ERROR vigem_connect(PVIGEM_CLIENT client);

PVIGEM_TARGET vigem_target_x360_alloc(void);

VIGEM_ERROR vigem_target_x360_update(PVIGEM_CLIENT client, PVIGEM_TARGET target, XUSB_REPORT report);

VIGEM_ERROR vigem_target_add(PVIGEM_CLIENT client, PVIGEM_TARGET target);

void vigem_target_remove(PVIGEM_CLIENT client, PVIGEM_TARGET target);
void vigem_target_free(PVIGEM_TARGET target);

#endif // VIGEMCLIENT_CLIENT_H

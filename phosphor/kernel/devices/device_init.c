/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: device_init.c
 *
 */

#include "device_init.h"
#include <kernel/screen/lib/string.h>
#include <kernel/screen/lib/print.h>
#include <kernel/screen/colors.h>
#include <kernel/fs/devfs/devfs.h>


static device_handler *devices[DEVICES_MAX_AMOUNT];
static int device_count = 0;

void devices_init(void)
{
    log("[DEV]", "init device system\n");
    for (int i = 0; i < DEVICES_MAX_AMOUNT; i++)
    {
        devices[i] = NULL;
    }
    device_count = 0;
}

int device_register(device_handler *device)
{
	if (!device || device_count >= DEVICES_MAX_AMOUNT)
	{
        log("[DEV]", "device register failed: invalid device or max reached\n", warning);
        return -1;
    }

    // check if already registered
    for (int i = 0; i < device_count; i++)
    {
        if (devices[i] == device) {
            log("[DEV]", "device already registered\n", warning);
            return -1;
        }
    }

    // call init if exists
    if (device->init)
    {
        int ret = device->init();
        if (ret != 0) {
            log("[DEV]", "device init failed, skipping registration\n", warning);
            return -1; /* don't add module if init fails */
        }
    }
    if (device->mount)
    {
        vfs_node_t *node = devfs_mount(device->mount, device);
        if (!node)
        {
            log(
            	"[DEV]",
             	"could not create vfs node for device, skipping registration\n",
              	warning
            );
            if (device->fini) device->fini();
            return -1;
        }
    }

    devices[device_count++] = device;
    return 0;
}

void device_unregister(const char *name)
{
    if (!name) return;

    for (int i = 0; i < device_count; i++)
    {
	    if (devices[i] && devices[i]->name && str_equals(devices[i]->name, name))
		{
	        // call cleanup if exists
	        if (devices[i]->fini)
			{
	            devices[i]->fini();
	        }

	        if (devices[i]->mount)
			{
	            vfs_remove(devices[i]->mount);
	        }

	        // shift array down
	        for (int j = i; j < device_count - 1; j++) {
	            devices[j] = devices[j + 1];
	        }
	        devices[device_count - 1] = NULL;
	        device_count--;

	        log("[DEV]", "device unregistered\n", d);
	        return;
	    }
    }
    log("[DEV]", "device to unregister not found\n", warning);
}

device_handler* device_find(const char *name)
{
    if (!name) return NULL;

    for (int i = 0; i < device_count; i++)
    {
        if (devices[i] && devices[i]->name && str_equals(devices[i]->name, name))
        {
            return devices[i];
        }
    }
    return NULL;
}

int device_get_count(void)
{
    return device_count;
}

device_handler* device_get_by_index(int idx)
{
    if (idx < 0 || idx >= device_count) {
        return NULL;
    }
    return devices[idx];
}

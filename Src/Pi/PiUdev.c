/*****************************************************************************
**
** blueberryMSX
** https://github.com/pokebyte/blueberryMSX
**
** An MSX Emulator for Raspberry Pi based on blueMSX
**
** Copyright (C) 2003-2006 Daniel Vik
** Copyright (C) 2014 Akop Karapetyan
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
******************************************************************************
*/

#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>

#include "PiInput.h"

static pthread_t monthread;
static int stopMonitor = 0;
static struct udev *udev = NULL;
static struct udev_monitor *mon;

static void udevMon(void *arg);

static int connectedMice = 0;
static int connectedJoysticks = 0;

// Based on
// http://www.signal11.us/oss/udev/udev_example.c

int piInitUdev()
{
	if (!(udev = udev_new())) {
		return 0;
	}

	mon = udev_monitor_new_from_netlink(udev, "udev");
	udev_monitor_filter_add_match_subsystem_devtype(mon, "input", NULL);
	udev_monitor_enable_receiving(mon);

	if (pthread_create(&monthread, NULL, udevMon, NULL) != 0) {
		udev_unref(udev);
		return 0;
	}

	fprintf(stderr, "udev initialized\n");
	
	return 1;
}

int piDestroyUdev()
{
	stopMonitor = 1;
	pthread_join(monthread, NULL);
	
	fprintf(stderr, "udev shut down\n");
	
	udev_unref(udev);
}

void piScanDevices()
{
	connectedMice = 0;
	connectedJoysticks = 0;
	struct udev_enumerate *uenum;
	struct udev_list_entry *devices, *devEntry;
	
	uenum = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(uenum, "input");
	udev_enumerate_scan_devices(uenum);
	devices = udev_enumerate_get_list_entry(uenum);
	udev_list_entry_foreach(devEntry, devices) {
		const char *path = udev_list_entry_get_name(devEntry);
		struct udev_device *dev = udev_device_new_from_syspath(udev, path);
		
		const char *sysname = udev_device_get_sysname(dev);
		if (strncmp(sysname, "mouse", 5) == 0) {
			connectedMice++;
		} else if (strncmp(sysname, "js", 2) == 0) {
			connectedJoysticks++;
		}
		
		udev_device_unref(dev);
	};
	connectedMice = 0;
	udev_enumerate_unref(uenum);

	piInputResetMSXDevices(connectedMice, connectedJoysticks);
}

static void udevMon(void *arg)
{
	fprintf(stderr, "udev monitor starting\n");
	
	int monfd = udev_monitor_get_fd(mon);
	struct udev_device *dev;
	fd_set fds;
	struct timeval tv;
	int ret;
	
	while (!stopMonitor) {
		FD_ZERO(&fds);
		FD_SET(monfd, &fds);
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		
		ret = select(monfd + 1, &fds, NULL, NULL, &tv);
		if (ret > 0 && FD_ISSET(monfd, &fds)) {
			// Make the call to receive the device.
			//   select() ensured that this will not block.
			dev = udev_monitor_receive_device(mon);
			if (dev) {
				const char *action = udev_device_get_action(dev);
				int diff = 0;
				if (strncmp(action, "add", 3) == 0) {
					diff = 1;
				} else if (strncmp(action, "remove", 6) == 0) {
					diff = -1;
				}

				if (diff != 0) {
					const char *sysname = udev_device_get_sysname(dev);
					if (strncmp(sysname, "mouse", 5) == 0) {
						connectedMice += diff;
						piInputResetMSXDevices(connectedMice, connectedJoysticks);
					} else if (strncmp(sysname, "js", 2) == 0) {
						connectedJoysticks += diff;
						piInputResetJoysticks();
						piInputResetMSXDevices(connectedMice, connectedJoysticks);
					}
				}

				udev_device_unref(dev);
			}
		}
		
		usleep(250*1000);
	}
	
	fprintf(stderr, "udev monitor exiting\n");
}


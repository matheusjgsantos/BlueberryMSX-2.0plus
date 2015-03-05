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
				if (strncmp(udev_device_get_sysname(dev), "js", 2) == 0) {
					fprintf(stderr, "FIXME: resetting joysticks\n");
					piInputResetJoysticks();
					
					/*
					printf("   Subsystem: %s\n", udev_device_get_subsystem(dev));
					printf("   Devnode: %s\n", udev_device_get_devnode(dev));
					printf("   Devtype: %s\n", udev_device_get_devtype(dev));
					printf("   Devpath: %s\n", udev_device_get_devpath(dev));
					printf("   Syspath: %s\n", udev_device_get_syspath(dev));
					printf("   Sysname: %s\n", udev_device_get_sysname(dev));
					printf("   Sysnum: %s\n", udev_device_get_sysnum(dev));
					printf("   Action: %s\n",udev_device_get_action(dev));
					*/
				}
				udev_device_unref(dev);
			}					
		}
		
		usleep(250*1000);
	}
	
	fprintf(stderr, "udev monitor exiting\n");
}


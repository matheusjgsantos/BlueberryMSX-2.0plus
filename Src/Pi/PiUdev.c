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
#include <linux/fd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "PiInput.h"

static pthread_t monthread;
static int stopMonitor = 0;
static struct udev *udev = NULL;
static struct udev_monitor *mon;

static void udevMon(void *arg);

static int connectedMice = 0;
static int connectedJoysticks = 0;
static int connectedFloppyDisks = 0;

// Based on
// http://www.signal11.us/oss/udev/udev_example.c

int piInitUdev()
{
	if (!(udev = udev_new())) {
		return 0;
	}

	mon = udev_monitor_new_from_netlink(udev, "udev");
	udev_monitor_filter_add_match_subsystem_devtype(mon, "input", NULL);
	udev_monitor_filter_add_match_subsystem_devtype(mon, "block", NULL);
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
	connectedFloppyDisks = 0;
	struct udev_enumerate *uenum;
	struct udev_list_entry *devices, *devEntry;
	
	uenum = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(uenum, "input");
	udev_enumerate_add_match_subsystem(uenum, "block");
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
		} else if (strncmp(sysname, "sd", 2) == 0) {
			connectedFloppyDisks++;
		}
		udev_device_unref(dev);
	};
	connectedMice = 0;
	udev_enumerate_unref(uenum);

	piInputResetMSXDevices(connectedMice, connectedJoysticks);
}

#define TIME_OUT (100 * 1000)	/* milliseconds */

#define UFI_GOOD 0
#define UFI_ERROR -1
#define UFI_UNFORMATTED_MEDIA 1
#define UFI_FORMATTED_MEDIA 2
#define UFI_NO_MEDIA 3

#define UFI_PROTECTED 1
#define UFI_NOT_PROTECTED 0

static const unsigned char TEST_UNIT_READY_CMD[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const unsigned char READ_FORMAT_CAPACITIES_CMD[] = {
    0x23, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x40, 0x00, 0x00, 0x00
};

static const unsigned char INQUIRY_CMD[] = {
    0x12, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x40, 0x00, 0x00, 0x00
};

static const unsigned char MODE_SENSE_CMD[] = {
    0x5A, 0x00, 0x05, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x40, 0x00, 0x00, 0x00
};

static const unsigned char FORMAT_UNIT_CMD[] = {
    0x04, 0x17, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0C, 0x00, 0x00, 0x00
};

static const unsigned char FORMAT_UNIT_DATA[] = {
    0x00, 0xB0, 0x00, 0x08, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
}; 

static int ufi_invoke(int fd, const char *cmd, size_t cmd_size, char *data, size_t data_size, int direction)
{
    sg_io_hdr_t sg_io_hdr;
    unsigned char sense_buffer[32];

    memset(&sg_io_hdr, 0, sizeof(sg_io_hdr));
    sg_io_hdr.interface_id = 'S';
    sg_io_hdr.cmdp = (char *)cmd;
    sg_io_hdr.cmd_len = cmd_size;
    sg_io_hdr.dxfer_direction = direction;
    sg_io_hdr.dxferp = data;
    sg_io_hdr.dxfer_len = data_size;
    sg_io_hdr.sbp = sense_buffer;
    sg_io_hdr.mx_sb_len = sizeof(sense_buffer);
    sg_io_hdr.timeout = TIME_OUT;

    if (ioctl(fd, SG_IO, &sg_io_hdr) < 0) {
        return UFI_ERROR;
    }
    if (cmd[0] == TEST_UNIT_READY_CMD[0]) {
	if (sg_io_hdr.masked_status == CHECK_CONDITION &&
	    (sense_buffer[2] & 0xf) == 0x6 && sense_buffer[12] == 0x28 && sense_buffer[13] == 0x00) {
	    /* media change */
	    if (ioctl(fd, SG_IO, &sg_io_hdr) < 0) {
		return UFI_ERROR;
	    }
	}
	if (sg_io_hdr.masked_status == CHECK_CONDITION &&
	    (sense_buffer[2] & 0xf) == 0x3 && sense_buffer[12] == 0x30 && sense_buffer[13] == 0x01) {
	    /* unformatted media */
	    return UFI_UNFORMATTED_MEDIA;
	}
	if (sg_io_hdr.masked_status == CHECK_CONDITION &&
	    (sense_buffer[2] & 0xf) == 0x2 && sense_buffer[12] == 0x3a && sense_buffer[13] == 0x00) {
	    /* no media */
	    return UFI_NO_MEDIA;
	}
    }
    if (sg_io_hdr.masked_status != GOOD) {
#if 0		
	if (verbose) {
	    int i;
	    fprintf(stderr, "SCSI error(command=%02x, status=%02x)\n", *cmd, sg_io_hdr.masked_status);
	    for (i = 0; i < sizeof(sense_buffer); i++) {
		printf("%02x ", sense_buffer[i]);
		if (i % 16 == 15) printf("\n");
	    }
	}
#endif	
	errno = EPROTO;
	return UFI_ERROR;
    }
    return UFI_GOOD;
}

#define ufi_invoke_to(fd, cmd, data) \
  ufi_invoke(fd, cmd, sizeof(cmd), data, sizeof(data), SG_DXFER_TO_DEV)
#define ufi_invoke_from(fd, cmd, data) \
  ufi_invoke(fd, cmd, sizeof(cmd), data, sizeof(data), SG_DXFER_FROM_DEV)
#define ufi_invoke_no(fd, cmd) \
  ufi_invoke(fd, cmd, sizeof(cmd), NULL, 0, SG_DXFER_TO_DEV) 

static void udevMon(void *arg)
{
	fprintf(stderr, "udev monitor starting\n");
	
	int monfd = udev_monitor_get_fd(mon);
	struct udev_device *dev;
	fd_set fds;
	struct timeval tv;
	int ret, fd, res, prev_res;
	
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
				udev_device_unref(dev);
				}
			}
		}
		for(int i = 0; i < connectedFloppyDisks; i++)
		{
			char devname[100];
			sprintf(devname, "/dev/sd%c", 'a' + i);
			fd = open(devname, 3 | O_NDELAY);
			res = ufi_invoke_no(fd, TEST_UNIT_READY_CMD);
			if (res == 0 && prev_res != 0) {
				//	printf("disk_inserted\n");
				diskChange(i ,devname, 0);
			} else if (res == 3 && prev_res != 3) {
				diskChange(1, 0, 0);
			}
			prev_res = res;
			close(fd);
		}
		usleep(1000*1000);
	}
	fprintf(stderr, "udev monitor exiting\n");
}


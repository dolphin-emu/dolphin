/* -*- Mode: C; c-basic-offset:8 ; indent-tabs-mode:t -*- */
/*
 * Linux usbfs backend for libusb
 * Copyright (C) 2007-2009 Daniel Drake <dsd@gentoo.org>
 * Copyright (c) 2001 Johannes Erdfelt <johannes@erdfelt.com>
 * Copyright (c) 2013 Nathan Hjelm <hjelmn@mac.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "config.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "libusb.h"
#include "libusbi.h"
#include "linux_usbfs.h"

#include <linux/netlink.h>
#include <linux/filter.h>

#define KERNEL 1

static int linux_netlink_socket = -1;
static pthread_t libusb_linux_event_thread;

static void *linux_netlink_event_thread_main(void *arg);

struct sockaddr_nl snl = { .nl_family=AF_NETLINK, .nl_groups=KERNEL };

int linux_netlink_start_event_monitor(void)
{
	int ret;

	snl.nl_groups = KERNEL;

	linux_netlink_socket = socket(PF_NETLINK, SOCK_RAW|SOCK_CLOEXEC|SOCK_NONBLOCK, NETLINK_KOBJECT_UEVENT);
	if (-1 == linux_netlink_socket) {
		return LIBUSB_ERROR_OTHER;
	}

	ret = bind(linux_netlink_socket, (struct sockaddr *) &snl, sizeof(snl));
	if (0 != ret) {
		return LIBUSB_ERROR_OTHER;
	}

	/* TODO -- add authentication */
	/* setsockopt(linux_netlink_socket, SOL_SOCKET, SO_PASSCRED, &one, sizeof(one)); */

	ret = pthread_create(&libusb_linux_event_thread, NULL, linux_netlink_event_thread_main, NULL);
	if (0 != ret) {
		return LIBUSB_ERROR_OTHER;
	}

	return LIBUSB_SUCCESS;
}

int linux_netlink_stop_event_monitor(void)
{
	int r;

	if (-1 == linux_netlink_socket) {
		/* already closed. nothing to do */
		return LIBUSB_SUCCESS;
	}

	r = close(linux_netlink_socket);
	if (0 > r) {
		usbi_err(NULL, "error closing netlink socket. %s", strerror(errno));
		return LIBUSB_ERROR_OTHER;
	}

	pthread_cancel(libusb_linux_event_thread);

	linux_netlink_socket = -1;

	return LIBUSB_SUCCESS;
}

static const char *netlink_message_parse (const char *buffer, size_t len, const char *key)
{
	size_t keylen = strlen(key);
	size_t offset;

	for (offset = 0 ; offset < len && '\0' != buffer[offset] ; offset += strlen(buffer + offset) + 1) {
		if (0 == strncmp(buffer + offset, key, keylen) &&
		    '=' == buffer[offset + keylen]) {
			return buffer + offset + keylen + 1;
		}
	}

	return NULL;
}

/* parse parts of netlink message common to both libudev and the kernel */
static int linux_netlink_parse(char *buffer, size_t len, int *detached, const char **sys_name,
			       uint8_t *busnum, uint8_t *devaddr) {
	const char *tmp;
	int i;

	errno = 0;

	*sys_name = NULL;
	*detached = 0;
	*busnum   = 0;
	*devaddr  = 0;

	tmp = netlink_message_parse((const char *) buffer, len, "ACTION");
	if (tmp == NULL)
		return -1;
	if (0 == strcmp(tmp, "remove")) {
		*detached = 1;
	} else if (0 != strcmp(tmp, "add")) {
		usbi_dbg("unknown device action %s", tmp);
		return -1;
	}

	/* check that this is a usb message */
	tmp = netlink_message_parse(buffer, len, "SUBSYSTEM");
	if (NULL == tmp || 0 != strcmp(tmp, "usb")) {
		/* not usb. ignore */
		return -1;
	}

	tmp = netlink_message_parse(buffer, len, "BUSNUM");
	if (NULL == tmp) {
		/* no bus number (likely a usb interface). ignore*/
		return -1;
	}

	*busnum = (uint8_t)(strtoul(tmp, NULL, 10) & 0xff);
	if (errno) {
		errno = 0;
		return -1;
	}

	tmp = netlink_message_parse(buffer, len, "DEVNUM");
	if (NULL == tmp) {
		return -1;
	}

	*devaddr = (uint8_t)(strtoul(tmp, NULL, 10) & 0xff);
	if (errno) {
		errno = 0;
		return -1;
	}

	tmp = netlink_message_parse(buffer, len, "DEVPATH");
	if (NULL == tmp) {
		return -1;
	}

	for (i = strlen(tmp) - 1 ; i ; --i) {
		if ('/' ==tmp[i]) {
			*sys_name = tmp + i + 1;
			break;
		}
	}

	/* found a usb device */
	return 0;
}

static int linux_netlink_read_message(void)
{
	unsigned char buffer[1024];
	struct iovec iov = {.iov_base = buffer, .iov_len = sizeof(buffer)};
	struct msghdr meh = { .msg_iov=&iov, .msg_iovlen=1,
			     .msg_name=&snl, .msg_namelen=sizeof(snl) };
	const char *sys_name = NULL;
	uint8_t busnum, devaddr;
	int detached, r;
	size_t len;

	/* read netlink message */
	memset(buffer, 0, sizeof(buffer));
	len = recvmsg(linux_netlink_socket, &meh, 0);
	if (len < 32) {
		if (errno != EAGAIN)
			usbi_dbg("error recieving message from netlink");
		return -1;
	}

	/* TODO -- authenticate this message is from the kernel or udevd */

	r = linux_netlink_parse(buffer, len, &detached, &sys_name,
				&busnum, &devaddr);
	if (r)
		return r;

	usbi_dbg("netlink hotplug found device busnum: %hhu, devaddr: %hhu, sys_name: %s, removed: %s",
		 busnum, devaddr, sys_name, detached ? "yes" : "no");

	/* signal device is available (or not) to all contexts */
	if (detached)
		linux_hotplug_disconnected(busnum, devaddr, sys_name);
	else
		linux_hotplug_enumerate(busnum, devaddr, sys_name);

	return 0;
}

static void *linux_netlink_event_thread_main(void *arg)
{
	struct pollfd fds = {.fd = linux_netlink_socket,
			     .events = POLLIN};

	/* silence compiler warning */
	(void) arg;

	while (1 == poll(&fds, 1, -1)) {
		if (POLLIN != fds.revents) {
			break;
		}

		usbi_mutex_static_lock(&linux_hotplug_lock);
		linux_netlink_read_message();
		usbi_mutex_static_unlock(&linux_hotplug_lock);
	}

	return NULL;
}

void linux_netlink_hotplug_poll(void)
{
	int r;

	usbi_mutex_static_lock(&linux_hotplug_lock);
	do {
		r = linux_netlink_read_message();
	} while (r == 0);
	usbi_mutex_static_unlock(&linux_hotplug_lock);
}

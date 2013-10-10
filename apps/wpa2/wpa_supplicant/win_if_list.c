/*
 * win_if_list - Display network interfaces with description (for Windows)
 * Copyright (c) 2004-2006, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 *
 * This small tool is for the Windows build to provide an easy way of fetching
 * a list of available network interfaces.
 */

#include "includes.h"
#include <stdio.h>
#ifdef CONFIG_USE_NDISUIO
#include <winsock2.h>
#include <ntddndis.h>
#else /* CONFIG_USE_NDISUIO */
#include "pcap.h"
#include <winsock.h>
#endif /* CONFIG_USE_NDISUIO */

#ifdef USE_WINIFLIST
#include "win_if_list.h"

int win_if_list_free(win_if_t **list)
{
	int ret = -1;
	win_if_t *next, *tmp;

	do {
		if (!list)
			break;

		for (next = *list; next;) {
			tmp = next->next;
			free(next);
			next = tmp;
		}
	} while (0);

	if (list)
		*list = 0;

	return ret;
}
#endif /* USE_WINIFLIST */

#ifdef CONFIG_USE_NDISUIO

/* from nuiouser.h */
#define FSCTL_NDISUIO_BASE      FILE_DEVICE_NETWORK

#define _NDISUIO_CTL_CODE(_Function, _Method, _Access) \
	CTL_CODE(FSCTL_NDISUIO_BASE, _Function, _Method, _Access)

#define IOCTL_NDISUIO_QUERY_BINDING \
	_NDISUIO_CTL_CODE(0x203, METHOD_BUFFERED, \
			  FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_NDISUIO_BIND_WAIT \
	_NDISUIO_CTL_CODE(0x204, METHOD_BUFFERED, \
			  FILE_READ_ACCESS | FILE_WRITE_ACCESS)

typedef struct _NDISUIO_QUERY_BINDING
{
	ULONG BindingIndex;
	ULONG DeviceNameOffset;
	ULONG DeviceNameLength;
	ULONG DeviceDescrOffset;
	ULONG DeviceDescrLength;
} NDISUIO_QUERY_BINDING, *PNDISUIO_QUERY_BINDING;


static HANDLE ndisuio_open(void)
{
	DWORD written;
	HANDLE h;

	h = CreateFile(TEXT("\\\\.\\\\Ndisuio"),
		       GENERIC_READ | GENERIC_WRITE, 0, NULL,
		       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
		       INVALID_HANDLE_VALUE);
	if (h == INVALID_HANDLE_VALUE)
		return h;

#ifndef _WIN32_WCE
	if (!DeviceIoControl(h, IOCTL_NDISUIO_BIND_WAIT, NULL, 0, NULL, 0,
			     &written, NULL)) {
		printf("IOCTL_NDISUIO_BIND_WAIT failed: %d",
		       (int) GetLastError());
		CloseHandle(h);
		return INVALID_HANDLE_VALUE;
	}
#endif /* _WIN32_WCE */

	return h;
}


#ifndef USE_WINIFLIST
static void ndisuio_query_bindings(HANDLE ndisuio)
{
	NDISUIO_QUERY_BINDING *b;
	size_t blen = sizeof(*b) + 1024;
	int i, error;
	DWORD written;
	char name[256], desc[256];
	WCHAR *pos;
	size_t j, len;

	b = malloc(blen);
	if (b == NULL)
		return;

	for (i = 0; ; i++) {
		memset(b, 0, blen);
		b->BindingIndex = i;
		if (!DeviceIoControl(ndisuio, IOCTL_NDISUIO_QUERY_BINDING,
				     b, sizeof(NDISUIO_QUERY_BINDING), b,
				     (DWORD) blen, &written, NULL)) {
			error = (int) GetLastError();
			if (error == ERROR_NO_MORE_ITEMS)
				break;
			printf("IOCTL_NDISUIO_QUERY_BINDING failed: %d",
			       error);
			break;
		}

		pos = (WCHAR *) ((char *) b + b->DeviceNameOffset);
		len = b->DeviceNameLength;
		if (len >= sizeof(name))
			len = sizeof(name) - 1;
		for (j = 0; j < len; j++)
			name[j] = (char) pos[j];
		name[len] = '\0';

		pos = (WCHAR *) ((char *) b + b->DeviceDescrOffset);
		len = b->DeviceDescrLength;
		if (len >= sizeof(desc))
			len = sizeof(desc) - 1;
		for (j = 0; j < len; j++)
			desc[j] = (char) pos[j];
		desc[len] = '\0';

		printf("ifname: %s\ndescription: %s\n\n", name, desc);
	}

	free(b);
}


static void ndisuio_enum_bindings(void)
{
	HANDLE ndisuio = ndisuio_open();
	if (ndisuio == INVALID_HANDLE_VALUE)
		return;

	ndisuio_query_bindings(ndisuio);
	CloseHandle(ndisuio);
}

#else /* USE_WINIFLIST */
static int ndisuio_query_bindings(HANDLE ndisuio, win_if_t **list)
{
	int ret = 0;
	NDISUIO_QUERY_BINDING *b = 0;
	size_t blen = sizeof(*b) + 1024;
	int i, error;
	DWORD written;
	char name[256], desc[256];
	WCHAR *pos;
	size_t j, len;
	win_if_t *item = 0, *next;

	do {
		if (!list) {
			ret = -1;
			break;
		}
		*list = 0;

		b = malloc(blen);
		if (b == NULL) {
			ret = -1;
			break;
		}

		for (i = 0; ; i++) {
			memset(b, 0, blen);
			b->BindingIndex = i;
			if (!DeviceIoControl(ndisuio, IOCTL_NDISUIO_QUERY_BINDING,
						 b, sizeof(NDISUIO_QUERY_BINDING), b,
						 (DWORD) blen, &written, NULL)) {
				error = (int) GetLastError();
				if (error == ERROR_NO_MORE_ITEMS)
					break;
				printf("IOCTL_NDISUIO_QUERY_BINDING failed: %d",
					   error);
				ret = -1;
				break;
			}

			pos = (WCHAR *) ((char *) b + b->DeviceNameOffset);
			len = b->DeviceNameLength;
			if (len >= sizeof(name))
				len = sizeof(name) - 1;
			for (j = 0; j < len; j++)
				name[j] = (char) pos[j];
			name[len] = '\0';

			pos = (WCHAR *) ((char *) b + b->DeviceDescrOffset);
			len = b->DeviceDescrLength;
			if (len >= sizeof(desc))
				len = sizeof(desc) - 1;
			for (j = 0; j < len; j++)
				desc[j] = (char) pos[j];
			desc[len] = '\0';

			next = malloc(sizeof(win_if_t));
			if (!next)
				break;

			next->next = 0;
			snprintf(next->name, sizeof(next->name), "%s", dev->name);
			snprintf(next->description, sizeof(next->description), "%s", dev->description);

			if (item)
				item->next = next;
			if (!*list) *list = item;

			item = next;
		}
	} while (0);

	if (b)
		free(b);

	return ret;
}

static int ndisuio_enum_bindings(win_if_t **list)
{
	int ret = -1;
	HANDLE ndisuio == INVALID_HANDLE_VALUE;

	do {
		if (!list)
			break;

		ndisuio  = ndisuio_open();
		if (ndisuio == INVALID_HANDLE_VALUE)
			break;

		ret = ndisuio_query_bindings(ndisuio, list);
	} while (0);

	if (ndisuio != INVALID_HANDLE_VALUE)
		CloseHandle(ndisuio);

	if (ret && list)
		win_if_list_free(list);

	return ret;
}
#endif // USE_WINIFLIST

#else /* CONFIG_USE_NDISUIO */

static void show_dev(pcap_if_t *dev)
{
	printf("ifname: %s\ndescription: %s\n\n",
	       dev->name, dev->description);
}


#ifndef USE_WINIFLIST
static void pcap_enum_devs(void)
{
	pcap_if_t *devs, *dev;
	char err[PCAP_ERRBUF_SIZE + 1];

	if (pcap_findalldevs(&devs, err) < 0) {
		fprintf(stderr, "Error - pcap_findalldevs: %s\n", err);
		return;
	}

	for (dev = devs; dev; dev = dev->next) {
		show_dev(dev);
	}

	pcap_freealldevs(devs);
}

#else /* USE_WINIFLIST */
static int pcap_enum_devs(win_if_t **list)
{
	int ret = -1;
	pcap_if_t *devs = 0, *dev;
	char err[PCAP_ERRBUF_SIZE + 1];
	win_if_t *item = 0, *next;

	do {
		if (!list)
			break;
		*list = 0;

		if (pcap_findalldevs(&devs, err) < 0) {
			fprintf(stderr, "Error - pcap_findalldevs: %s\n", err);
			break;
		}

		for (dev = devs; dev; dev = dev->next) {
			next = malloc(sizeof(win_if_t));
			if (!next)
				break;

			next->next = 0;
			snprintf(next->name, sizeof(next->name), "%s", dev->name);
			snprintf(next->description, sizeof(next->description), "%s", dev->description);

			if (item)
				item->next = next;
			if (!*list) *list = item;

			item = next;
		}

		ret = 0;
	} while (0);

	pcap_freealldevs(devs);

	if (ret && list)
		win_if_list_free(list);

	return ret;
}
#endif /* USE_WINIFLIST */

#endif /* CONFIG_USE_NDISUIO */

#ifndef USE_WINIFLIST
int main(int argc, char *argv[])
{
#ifdef CONFIG_USE_NDISUIO
	ndisuio_enum_bindings();
#else /* CONFIG_USE_NDISUIO */
	pcap_enum_devs();
#endif /* CONFIG_USE_NDISUIO */

	return 0;
}

#else /* USE_WINIFLIST */
int win_if_enum_devs(win_if_t **list)
{
#ifdef CONFIG_USE_NDISUIO
	return ndisuio_enum_bindings(list);
#else /* CONFIG_USE_NDISUIO */
	return pcap_enum_devs(list);
#endif /* CONFIG_USE_NDISUIO */
}

#endif /* USE_WINIFLIST */


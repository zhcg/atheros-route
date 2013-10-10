/* This file is used for a trick to determine the version of the kernel
 * build tree. Simply grepping <linux/version.h> doesn't work, since
 * some distributions have multiple UTS_RELEASE definitions in that
 * file.
 * Taken from the lm_sensors project.
 *
 * $Id: //depot/sw/releases/Aquila_9.2.0_U11/apps/wpa2/madwifi.host/kernelversion.c#1 $
 */
#include <linux/version.h>

/* Linux 2.6.18+ uses <linux/utsrelease.h> */
#ifndef UTS_RELEASE
#include <linux/utsrelease.h>
#endif

char *uts_release = UTS_RELEASE;

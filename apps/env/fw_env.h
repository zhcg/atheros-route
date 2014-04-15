/*
 * (C) Copyright 2002
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/*
 * To build the utility with the run-time configuration
 * uncomment the next line.
 * See included "fw_env.config" sample file (TRAB board)
 * for notes on configuration.
 */
#define CONFIG_FILE     "/etc/fw_env.config"

//#define HAVE_REDUND /* For systems with 2 env sectors */
//#define DEVICE1_NAME      "/dev/mtd1"
//#define DEVICE2_NAME      "/dev/mtd2"
//#define DEVICE1_OFFSET    0x0000
//#define ENV1_SIZE         0x4000
//#define DEVICE1_ESIZE     0x4000
//#define DEVICE2_OFFSET    0x0000
//#define ENV2_SIZE         0x4000
//#define DEVICE2_ESIZE     0x4000

#define CONFIG_BAUDRATE		115200
#define CONFIG_BOOTDELAY	5	/* autoboot after 5 seconds	*/
/*wangyu modify for bootargs*/
#define CONFIG_IPADDR	192.168.1.1
#define CONFIG_SERVERIP	192.168.1.2
#define CONFIG_ETHADDR   0x00:0xaa:0xbb:0xcc:0xdd:0xee
#define CONFIG_BOOTCOMMAND_LU	\
		"tftp 0x80060000 ${dir}u-boot.bin&&erase 0x9f000000 +$filesize&&cp.b $fileaddr 0x9f000000 $filesize"
#define CONFIG_BOOTCOMMAND_LK	\
		"tftp 0x80060000 ${dir}vmlinux${bc}.lzma.uImage&&erase 0x9fe80000 +$filesize&&cp.b $fileaddr 0x9fe80000 $filesize"
#define CONFIG_BOOTCOMMAND_LF	\
		"tftp 0x80060000 ${dir}db12x${bc}-jffs2&&erase 0x9f050000 +0xe30000&&cp.b $fileaddr 0x9f050000 $filesize"
#define CONFIG_BOOTCOMMAND							\
	"bootm 0x9fe80000"
#define CONFIG_BOOTARGS 				\
	"console=ttyS0,115200 root=31:02 "				\
	"rootfstype=jffs2 init=/sbin/init " 					\
	"mtdparts=ath-nor0:256k(u-boot),64k(u-boot-env),14528k(rootfs),1408k(uImage),64k(mib0),64k(ART)"
/*wangyu modify for bootargs*/
extern		void  fw_printenv(int argc, char *argv[]);
extern unsigned char *fw_getenv  (unsigned char *name);
extern		int   fw_setenv  (int argc, char *argv[]);

extern unsigned	long  crc32	 (unsigned long, const unsigned char *, unsigned);

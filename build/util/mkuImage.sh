#!/bin/sh -x

#
# $1 == u-boot/tools path
# $2 == kernel tree path
# $3 == optional additions to filename

MKIMAGE=$1/mkimage
VMLINUX=$2/vmlinux
VMLINUXBIN=$2/arch/mips/boot/vmlinux.bin
# NOTE You can direct the outputs elsewhere by pre-defining TFTPPATH
if [ -z "$TFTPPATH" ]
then
    TFTPPATH=/tftpboot/`whoami`
fi
echo $0 Using TFTPPATH=$TFTPPATH  ###### DEBUG

ENTRY=`readelf -a ${VMLINUX}|grep "Entry"|head -1|cut -d":" -f 2`
LDADDR=`readelf -a ${VMLINUX}|grep "\[ 1\]"|head -1|cut -d" " -f 26`

# gzip -f ${VMLINUXBIN}

if [ $# -gt 3 ]
then

	${MKIMAGE} -A mips -O linux -T kernel -C gzip \
			-a 0x${LDADDR} -e ${ENTRY} -n "Linux Kernel Image"    \
					-d ${VMLINUXBIN}.gz ${IMAGEPATH}/vmlinux$3.gz.uImage
	cp ${IMAGEPATH}/vmlinux$3.gz.uImage ${TFTPPATH}
	cp ${IMAGEPATH}/vmlinux$3.gz.uImage ${TFTPPATH}/zImage.uImage
	if [ $4 = "lzma" ] 
	then
		echo "**** Generating vmlinux$3.lzma.uImage ********";
		${MKIMAGE} -A mips -O linux -T kernel -C lzma \
				-a 0x${LDADDR} -e ${ENTRY} -n "Linux Kernel Image"    \
						-d ${VMLINUXBIN}.lzma ${IMAGEPATH}/vmlinux$3.lzma.uImage ;
		cp ${IMAGEPATH}/vmlinux$3.lzma.uImage ${TFTPPATH} ;
		cp ${IMAGEPATH}/vmlinux$3.lzma.uImage ${TFTPPATH}/zImage.uImage ;
	fi
else

	${MKIMAGE} -A mips -O linux -T kernel -C gzip \
			-a 0x${LDADDR} -e ${ENTRY} -n "Linux Kernel Image"    \
					-d ${VMLINUXBIN}.gz ${IMAGEPATH}/vmlinux.gz.uImage
	cp ${IMAGEPATH}/vmlinux.gz.uImage ${TFTPPATH}
	cp ${IMAGEPATH}/vmlinux.gz.uImage ${TFTPPATH}/zImage.uImage

	if [ $# -eq 3 ]
	then
		if [ $3 = "lzma" ]
		then
			echo "**** Generating vmlinux.lzma.uImage ********";
			${MKIMAGE} -A mips -O linux -T kernel -C lzma \
					-a 0x${LDADDR} -e ${ENTRY} -n "Linux Kernel Image"    \
							-d ${VMLINUXBIN}.lzma ${IMAGEPATH}/vmlinux.lzma.uImage ;
			cp ${IMAGEPATH}/vmlinux.lzma.uImage ${TFTPPATH} ;
			cp ${IMAGEPATH}/vmlinux.lzma.uImage ${TFTPPATH}/zImage.uImage ;
		fi
	fi
fi

#!/bin/sh

SN=`fw_printenv | grep SN | awk -F'=' '{print $2}'`
sn_len=`expr length "$SN"`
#echo $sn_len
if [ "$sn_len" -eq  18 -a ! -e /configure_backup/.sn_backup ];then
	echo "WRITE IN"
	echo $SN > /configure_backup/.sn_backup
fi

if [ -e /configure_backup/.sn_backup ];then
	SN_backup=`cat /configure_backup/.sn_backup`
	echo "$SN_backup"
	sn_backup_len=`expr length "$SN_backup"`
	if [ "$sn_len" -ne  18 -a  "$sn_backup_len" -eq  18  ];then
		fw_setenv SN $SN_backup
		echo "WRITE OUT"
	fi
fi
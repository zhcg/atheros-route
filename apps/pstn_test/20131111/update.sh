#!/bin/sh
#把数据提交到服务器
ftp -in << FTPEOF
open 192.168.1.96
user yangjilong yangjilong0702
bin
#cd /home/yangjilong/20131011/5350/rt5350_for_base/RT288x_SDK/source/lib/lib_of_terminal_init/
#mput common_tools.* communication_stc.* database_management.* internetwork_communication.* network_config.* nvram_interface.* serial_communication.* terminal_authentication.* terminal_register.* unionpay_message.* safe_strategy.*

#cd /home/yangjilong/20131011/5350/rt5350_for_base/RT288x_SDK/source/user/terminal_init/
#mput spi_rt_main.c monitor_application.c terminal_init.c terminal_init_test.c

cd /home/yangjilong/20131011/5350/rt5350_for_base/RT288x_SDK/source/user/swipe_card/
mput swipe_card.c update.sh
bye
FTPEOF

echo "put ok!"

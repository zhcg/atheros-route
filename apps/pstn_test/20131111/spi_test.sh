#/bin/sh
mount -t nfs -o nolock 10.10.10.110:/mnt/mount/ /mnt/;cd /mnt/terminal_init_f1b/bin/;
export LD_LIBRARY_PATH=/mnt/terminal_init_f1b/lib:$LD_LIBRARY_PATH;
mkdir -p /var/terminal_init/log/;
./spi_rt_main &
./spi_rt_test 1 222 1 1000000;
#/bin/sh

make clean;make;make terminal_init;make terminal_init_test;make spi_rt_main;
#make spi_rt_test;make spi_rt_recv_test;make spi_rt_send_test;make spi_rt_pthread;make spi_rt_pthread_hook;make spi_recv_send_test;make spi_rt_select;
make swipe_card;make monitor_application;make install
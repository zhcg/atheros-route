/*************************************************************************
	> File Name: lc65xx-uart.h
	> Author: 
	> Mail: @163.com 
	> Created Time: 2017年03月15日 星期三 16时15分53秒
 ************************************************************************/
#ifndef _LC65XX_UART_H
#define _LC65XX_UART_H

#define	LC65XX_DEVICE	"/dev/ttyS1"
#define UART_BUF_LEN	32*1024

struct{
	int index;
	int earfcn;
	int cell_id;
	char rssi[8];
	int pathloss;
	char rsrp[8];
	char rsrq[8];
	char snr[8];
	int distance;
	char tx_power[8];
	int dl_throughput_total_tbs;
	int ul_throughput_total_tbs;
	int dlsch_tb_error_per;
	int mcs;
	int rb_num;
	int wide_cqi;
}at_drpr_status;

// AT Mode or Console ?
int	isATmode = 1;
int	isRadioRep = 0;

int	serial_fd = -1;
int	serial_recv_r = 0;
int	serial_recv_w = 0;
char serial_recvbuf[UART_BUF_LEN] = {0};

extern int lc65xx_read(char *prbuf, int len);
extern int lc65xx_write(char *pwbuf, int len);

int serial_init(void);
extern int create_serial_thread(void);

#endif


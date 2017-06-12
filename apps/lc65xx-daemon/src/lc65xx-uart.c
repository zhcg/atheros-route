/*************************************************************************
	> File Name: lc65xx-uart.c
	> Author: 
	> Mail: @163.com 
	> Created Time: 2017年03月15日 星期三 12时00分17秒
 ************************************************************************/

#include "common.h"
#include "lc65xx-uart.h"

int lc65xx_read(char *prbuf, int len)
{
	int ret = 0;
	
	if (prbuf == NULL)
		return 0;

	ret = serial_receive(serial_fd, (char *)prbuf, len);
	if (ret < 0)
		PRINT("read uart failed\n");
//	PRINT("%s, ret=%d, fd=%d\n",prbuf,ret,serial_fd);
	
	return ret;
}

int lc65xx_write(char *pwbuf, int len)
{
	int ret = 0;
	
	if (pwbuf == NULL)
		return 0;

	ret = serial_send(serial_fd,(char *)pwbuf,  len);
	if (ret < 0)
		PRINT("write uart failed\n");
//	PRINT(" ret=%d\n", ret);
	
	return ret;
}

int serial_init(void)
{
	int ret= -1;

	/* open serial port 0, /etc/ttyS0 */
	serial_fd = serial_open_file(LC65XX_DEVICE, 115200);
	if (serial_fd < 0)
	{
		PRINT("open LC65XX uart error ...\n");
		return -1;
	}
	
	ret = serial_set_attr(serial_fd, 8, PARITY_NONE, 1, FLOW_CONTROL_NONE);
	if ( ret != 1 )
	{
		PRINT("set LC65xx uart error ...\n");
		serial_close(serial_fd);
		return ret;
	}
	
	//serial_set_timeout(serial_fd, UART_TIMEOUT);
	
	sleep(1);

	return 0;
}

void recv_uart_data(void)
{
	int ret = -1;
	int read_ret = -1;
	char *w_ptr = serial_recvbuf;
	int valid_len = 0;
	char shell_buf[UART_BUF_LEN] = {0};

	ret = serial_init();
	if (ret)
	{
		PRINT("serial init failed, exit!\n");	
		exit(0);
	}

	/* at test */
	lc65xx_write("AT\r\n", strlen("AT\r\n"));

	while(1)	
	{
		if (isATmode)
		{
			if (serial_recv_w >= UART_BUF_LEN)
				serial_recv_w = 0;
			valid_len = UART_BUF_LEN - serial_recv_w;

			read_ret = lc65xx_read(w_ptr + serial_recv_w, valid_len);	
			if (read_ret > 0)
			{
				//PRINT("read_ret = %d \n", read_ret);
				serial_recv_w += read_ret;
				
				if (serial_recv_w >= UART_BUF_LEN)
					serial_recv_w = 0;
			}
			usleep(30*1000);
		}
		else
		{
			memset(shell_buf, 0, UART_BUF_LEN);
			read_ret = lc65xx_read(shell_buf, UART_BUF_LEN);
			printf("%s", shell_buf);
		}
	}
}

void parse_uart_data(void)
{
	char *r_ptr = serial_recvbuf;
	char cmdbuf[UART_BUF_LEN] = {0};
	char valid_count_buf[UART_BUF_LEN] = {0};
	int valid_len = 0;
	int i,j;
	int val;
	char tmp[64]={0};
	int len;

	while(1)
	{
		if (serial_recv_r >= UART_BUF_LEN)
			serial_recv_r = 0;
		if (serial_recv_w >= serial_recv_r)
			valid_len = serial_recv_w - serial_recv_r;
		else
			valid_len = UART_BUF_LEN - serial_recv_r;

		/* make sure string end with '\n' */
		memcpy(valid_count_buf, r_ptr+serial_recv_r, valid_len);
		*(valid_count_buf+valid_len+1) = 0;
		valid_len = strrchr(valid_count_buf, '\n') - valid_count_buf + 1;

		for(i = 0, j = 0; i < valid_len; i++)
		{
			if(*(r_ptr+serial_recv_r) != '\n')
			{
				cmdbuf[j++] = *(r_ptr+serial_recv_r);
				if(j >= UART_BUF_LEN)
				{
					j  = 0;
				}
			}
			else
			{
				cmdbuf[j] = '\n';
				cmdbuf[j+1] = '\0';
				// CR LF

				PRINT("RECV: %s \n\n", cmdbuf);

				if(NULL != strstr(cmdbuf, "^DRPRI:"))
				{
					/* parse single data report */ 
					sscanf(cmdbuf, "^DRPRI: %d,%d,%d,\"%[^\"]\",%d,\"%[^\"]\",\"%[^\"]\",\"%[^\"]\",%d,\"%[^\"]\",%d,%d,%d,%d,%d,%d", &at_drpr_status.index, \
							&at_drpr_status.earfcn,		\
							&at_drpr_status.cell_id,	\
							(char*)&at_drpr_status.rssi,		\
							&at_drpr_status.pathloss,	\
							(char*)&at_drpr_status.rsrp,		\
							(char*)&at_drpr_status.rsrq,		\
							(char*)&at_drpr_status.snr,		\
							&at_drpr_status.distance,	\
							(char*)&at_drpr_status.tx_power,	\
							&at_drpr_status.dl_throughput_total_tbs,\
							&at_drpr_status.ul_throughput_total_tbs,\
							&at_drpr_status.dlsch_tb_error_per,	\
							&at_drpr_status.mcs,		\
							&at_drpr_status.rb_num,		\
							&at_drpr_status.wide_cqi);

					PRINT("DRPRI: index=%d, rsrp=%s, snr=%s \n", at_drpr_status.index, at_drpr_status.rsrp, at_drpr_status.snr);
					if (at_drpr_status.index == 1)
					{
						len = sprintf(tmp, "echo \"DRPRI: index=%d, rsrp=%s, snr=%s\n", at_drpr_status.index, at_drpr_status.rsrp, at_drpr_status.snr);
					}
					else if (at_drpr_status.index == 2)
					{
						sprintf(tmp+len, "DRPRI: index=%d, rsrp=%s, snr=%s\" > /usr/www/radio_info.xml", at_drpr_status.index, at_drpr_status.rsrp, at_drpr_status.snr);
						system(tmp);
					}
				}
				else if(NULL != strstr(cmdbuf, "^DACSI:"))
				{
					sscanf(cmdbuf, "^DACSI: %d", &val);
					PRINT("DACSI: state=%d \n", val);
					sprintf(tmp, "echo %d > /usr/www/access_info.xml", val);
					system(tmp);
				}
				else if(NULL != strstr(cmdbuf, "^DSYSI:"))
				{
					sscanf(cmdbuf, "^DSYSI: %d", &val);
					PRINT("DSYSI: state=%d \n", !val);
					sprintf(tmp, "echo %d > /usr/www/access_info.xml", !val);
					system(tmp);
				}
				else if(NULL != strstr(cmdbuf, "OK"))
				{
					//PRINT("respond: OK \n");
					//if (isRadioRep==1)
					system("echo  > /usr/www/radio_info.xml");
				}
				else if(NULL != strstr(cmdbuf, "ERROR"))
				{
					//PRINT("respond: ERROR \n");
				}

				j = 0;
				memset(cmdbuf, 0, UART_BUF_LEN);
			}
			//usleep(5*1000);
			serial_recv_r++;
		}
	}
}

int create_serial_thread(void)
{
	pthread_t data_recvice_pthread;
	pthread_create(&data_recvice_pthread, NULL, (void*)recv_uart_data, NULL);

	pthread_t data_parse_pthread;
	pthread_create(&data_parse_pthread, NULL, (void*)parse_uart_data, NULL);
	return 0;
}

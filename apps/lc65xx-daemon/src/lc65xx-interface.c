/*************************************************************************
	> File Name: lc65xx-interface.c
	> Author: 
	> Mail: @163.com 
	> Created Time: 2017年03月15日 星期三 14时32分20秒
 ************************************************************************/

#include "common.h"
#include "lc65xx-interface.h"

int get_file_size(const char *path)
{
	int file_size = -1;
	struct stat stat_buff;
	if (stat(path, &stat_buff) < 0)
		return file_size;
	else
		file_size = stat_buff.st_size;

	return file_size;
}

int get_file_mtime(const char *path)
{
	int file_mtime = -1;
	struct stat stat_buff;
	if (stat(path, &stat_buff) < 0)
		return file_mtime;
	else
		file_mtime = stat_buff.st_mtime;

	return file_mtime;
}

int load_config(void)
{
	int fd = -1;
	int read_ret = -1;
	char buf[1024] = {0};
	char tmp[1024] = {0};
	char val[64] = {0};
	char cmd[64] = {0};
	char bandwidth[8] = {0};
	char power[8] = {0};
	char dl_rate[8] = {0};
	char ul_rate[8] = {0};
	char master_ip[16] = {0};
	char slave_ip[16] = {0};
	int num = 0;
	int i,j;
	
	eCMDState = CMD_OTHER;

	fd = open(LC65XX_CONF_FILE, O_RDWR);
	if (fd < 0)
	{
		PRINT("lc65xx.conf is not found\n");	
		return -1;
	}

	read_ret = read(fd, buf, sizeof(buf));

	printf("read config:\n");
	for(num = 0; num < read_ret; num++)
	{
		printf("%c", buf[num]);

	}
	printf("\n");
	
	for(i = 0, j = 0; i < read_ret; i++)
	{
		if(buf[i] != '\n')
		{
			tmp[j++] = buf[i];
		}
		else
		{
			tmp[j] = '\n';

			PRINT("readline: %s \n", tmp);
		
			if (NULL != strstr(tmp, "CONFIG"))
			{
				eCMDState = CMD_CONFIG_START;
				system("cp /etc/lc65xx.conf /usr/www/lc65xx_info.xml");
			}
			else if (NULL != strstr(tmp, "SHELL"))
				eCMDState = CMD_SHELL_START;

			switch (eCMDState)	
			{
				case CMD_CONFIG_START:
					PRINT("~~~~~~~~CMD_CONFIG_START~~~~~~~~~~~\n");
					{
						if (!isATmode)
						{
							/* switch lc65xx to at mode */
							PRINT("~~~~~~~~sw2at~~~~~~~~~~~\n");
							system("echo \"sw2at\" > /dev/wifiled");
							lc65xx_write("reboot\r\n", strlen("reboot\r\n"));
							/* wait reboot */
							sleep(20);

							isATmode = 1;
						}

						sprintf(cmd, "AT+CFUN=0\r\n");
					}
					eCMDState = CMD_CONFIG_PARSE;
					break;
				case CMD_CONFIG_PARSE:
					PRINT("~~~~~~~~CMD_CONFIG_PARSE~~~~~~~~~~~\n");
					if (NULL != strstr(tmp, "dev"))
					{
						sscanf(tmp, "devType=%[^\n]", val);
						if (NULL != strstr(val, "1"))
							//strcat(cmd, "AT^DAOCNDI=,0\r\n");
							sprintf(cmd, "AT^DAOCNDI=,0\r\n");
						if (NULL != strstr(val, "2"))
							sprintf(cmd, "AT^DAOCNDI=,1\r\n");
					}
					else if (NULL != strstr(tmp, "bandwidth"))
					{
						sscanf(tmp, "bandwidth=%[^\n]", bandwidth);
					}
					else if (NULL != strstr(tmp, "band"))
					{
						sscanf(tmp, "band=%[^\n]", val);
						if (NULL != strstr(val, "1.4"))
							sprintf(cmd, "AT^DAOCNDI=04\r\n");
						if (NULL != strstr(val, "2.4"))
							sprintf(cmd, "AT^DAOCNDI=08\r\n");
						if (NULL != strstr(val, "800"))
							sprintf(cmd, "AT^DAOCNDI=01\r\n");
					}
					else if (NULL != strstr(tmp, "localID"))
					{
						sscanf(tmp, "localID=%[^\n]", val);
						sprintf(cmd, "AT^DTTEST=F1%s\r\n", val);
					}
					else if (NULL != strstr(tmp, "oppositeID"))
					{
						sscanf(tmp, "oppositeID=%[^\n]", val);
						sprintf(cmd, "AT^DPDIL=\"%s\"\r\n", val);
					}
					else if (NULL != strstr(tmp, "passwd"))
					{
						sscanf(tmp, "passwd=%[^\n]", val);
						sprintf(cmd, "AT^DAPI=\"%s\"\r\n", val);
					}
					else if (NULL != strstr(tmp, "masterIP"))
					{
						sscanf(tmp, "masterIP=%[^\n]", master_ip);
					}
					else if (NULL != strstr(tmp, "slaveIP"))
					{
						sscanf(tmp, "slaveIP=%[^\n]", slave_ip);
						sprintf(cmd, "AT^NETIFCFG=1,\"%s\",\"%s\"\r\n", master_ip,slave_ip);
					}
					else if (NULL != strstr(tmp, "power"))
					{
						/* wait config ip */
						sleep(3);

						sscanf(tmp, "power=%[^\n]", power);
					}
					else if (NULL != strstr(tmp, "downRate"))
					{
						sscanf(tmp, "downRate=%[^\n]", dl_rate);
					}
					else if (NULL != strstr(tmp, "upRate"))
					{
						sscanf(tmp, "upRate=%[^\n]", ul_rate);
						sprintf(cmd, "AT^DRPS=,%s,\"%s\",%s,%s\r\n", bandwidth,power,dl_rate,ul_rate);

						eCMDState = CMD_CONFIG_END;
					}
					break;
				case CMD_CONFIG_END:
					PRINT("~~~~~~~~CMD_CONFIG_END~~~~~~~~~~~\n");

					isATmode = 0;
					/* reset lc65xx */
					sleep(1);
					lc65xx_write("AT^LCMFUN=0\r\n", strlen("AT^LCMFUN=0\r\n"));
					//system("echo lc65xx_reset > /dev/wifiled");
					sleep(20);

					isATmode = 1;
					eCMDState = CMD_OTHER;
					break;
			
				case CMD_SHELL_START:
					PRINT("~~~~~~~~CMD_SHELL_START~~~~~~~~~~~\n");
					{
						if (isATmode)
						{
							/* switch lc65xx to console mode */
							PRINT("~~~~~~~~sw2shell~~~~~~~~~~~\n");
							system("echo \"sw2shell\" > /dev/wifiled");

							isATmode = 0;
							/* system("echo lc65xx_reset > /dev/wifiled"); */
							lc65xx_write("AT^LCMFUN=0\r\n", strlen("AT^LCMFUN=0\r\n"));
							/* wait reboot */
							sleep(20);
						}

						lc65xx_write("su\r\n", strlen("su\r\n"));
						lc65xx_write("echo 4 > /proc/sys/kernel/printk\r\n", strlen("echo 4 > /proc/sys/kernel/printk\r\n"));
						lc65xx_write("mount -o remount,rw /system\r\n", strlen("mount -o remount,rw /system\r\n"));
					}
					eCMDState = CMD_SHELL_PARSE;
					break;
				case CMD_SHELL_PARSE:
					PRINT("~~~~~~~~CMD_SHELL_PARSE~~~~~~~~~~~\n");
					if (NULL != strstr(tmp, "cmd"))
					{
						sscanf(tmp, "cmd=%[^\n]", val);
						//sprintf(cmd, "echo \"%s\" >> /system/bin/sync_time.sh\r\n", val);

					}
					eCMDState = CMD_SHELL_END;
					break;
				case CMD_SHELL_END:
					PRINT("~~~~~~~~CMD_SHELL_END~~~~~~~~~~~\n");

					/* switch lc65xx to at mode */
					PRINT("~~~~~~~~sw2at~~~~~~~~~~~\n");
					system("echo \"sw2at\" > /dev/wifiled");
					lc65xx_write("reboot\r\n", strlen("reboot\r\n"));
					/* wait reboot */
					sleep(20);

					isATmode = 1;
					eCMDState = CMD_OTHER;
					break;

				case CMD_OTHER:
					PRINT("~~~~~~~~CMD_OTHER~~~~~~~~~~~\n");
					if (NULL != strstr(tmp, "radioRep"))
					{
						sscanf(tmp, "radioRep=%[^\n]", val);
						sprintf(cmd, "AT^DRPR=%s\r\n", val);

						isRadioRep = atoi(val);
					}
					break;
			}

			/* write AT to serial */
			lc65xx_write(cmd, strlen(cmd));
			printf("cmd: %s ",cmd);

			j = 0;
			memset(val, 0, 64);
			memset(cmd, 0, 64);
			memset(tmp, 0, 1024);
		}
		usleep(20*1000);
	}

	/* clear file */
//	ftruncate(fd, 0);
//	lseek(fd, 0, SEEK_SET);

	close(fd);

	return 0;
}

void handle_host_cmd(void)
{
	int ret = -1;

	while(1)
	{
		ret = get_file_mtime(LC65XX_CONF_FILE);
		if (ret > 0)
		{
			if (ret != last_mtime && last_mtime !=0)
				ret = load_config();
			else
				PRINT(" file mtime = %d\n", ret);

			last_mtime = ret;
		}

		sleep(3);
	}
}

int create_interface_thread(void)
{
	pthread_t handle_hostcmd_pthread;
	pthread_create(&handle_hostcmd_pthread, NULL, (void*)handle_host_cmd, NULL);
	return 0;
}



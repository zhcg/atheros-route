/*************************************************************************
	> File Name: lc65xx-interface.h
	> Author: 
	> Mail: @163.com 
	> Created Time: 2017年03月15日 星期三 16时19分49秒
 ************************************************************************/
#ifndef _LC65XX_INTERFACE_H
#define _LC65XX_INTERFACE_H


//#define	LC65XX_CONF_FILE "/etc/lc65xx.conf"
#define	LC65XX_CONF_FILE "/etc/lc65xx.conf"

enum{
	CMD_CONFIG_START=0,
	CMD_CONFIG_PARSE,
	CMD_CONFIG_END,

	CMD_SHELL_START,
	CMD_SHELL_PARSE,
	CMD_SHELL_END,

	CMD_OTHER,
}eCMDState;

int last_mtime = 0;

int get_file_size(const char *path);
int get_file_mtime(const char *path);

int load_config(void);

extern int isATmode;
extern int isRadioRep;

extern int lc65xx_write(char *pwbuf, int len);
extern int create_interface_thread(void);

#endif


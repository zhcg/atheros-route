/* files.h */
#ifndef _FILES_H
#define _FILES_H

struct config_keyword {
	const char *keyword;
	int (* const handler)(const char *line, void *var);
	void *var;
	const char *def;
};


int read_config(const char *file);
void write_leases(void);
void read_leases(const char *file);
int deal_offline_sta(uint8_t *hostname, uint8_t *chaddr, uint32_t yiaddr);

#endif

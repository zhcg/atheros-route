#ifndef __CLOCK_API_H__
#define __CLOCK_API_H__

struct clock_api {
	void (*_clock_init)(void);
};

void clock_module_install(struct clock_api *api);

#endif /* __CLOCK_API_H__ */

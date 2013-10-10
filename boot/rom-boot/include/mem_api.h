#ifndef _MEM_API_H
#define _MEM_API_H

#include <asm/types.h>

struct memory_api {
	void	*(*_memset)(void *, int, int);
	void	*(*_memcpy)(void *, const void *, int);
	int	(*_memcmp)(const void *, const void *, int);
};

void
mem_module_install(struct memory_api *api);

#endif


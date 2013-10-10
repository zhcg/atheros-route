#include <asm/types.h>
#include "rom.addrs.h"

bootrom_fn_t bootrom_fn[] = {
// Wasp 1.0
	{
		(void *)0xbfc01114, (void *)0xbfc04000, (void *)0xbfc045ac,
		(void *)0xbfc00720, (void *)0xbfc005d0
	},
// Wasp 1.1
	{
		(void *)0xbfc0114c, (void *)0xbfc0421c, (void *)0xbfc047a8,
		(void *)0xbfc00730, (void *)0xbfc005d4
	},
// Wasp 1.2, same as above
	{
		(void *)0xbfc0114c, (void *)0xbfc0421c, (void *)0xbfc047a8,
		(void *)0xbfc00730, (void *)0xbfc005d4
	},
// Wasp 1.3
	{
		(void *)0xbfc00c7c, (void *)0xbfc00f2c, (void *)0xbfc00ac8,
		(void *)0xbfc00730, (void *)0xbfc005d4,
	},
};

/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

/* Like strncpy but make sure the resulting string is always 0 terminated. */
char* FAST_FUNC safe_strncpy(char *dst, const char *src, size_t size)
{
	if (!size) return dst;
	dst[--size] = '\0';
	return strncpy(dst, src, size);
}

/* Like strcpy but can copy overlapping strings. */
void FAST_FUNC overlapping_strcpy(char *dst, const char *src)
{
	while ((*dst = *src) != '\0') {
		dst++;
		src++;
	}
}

extern
int safe_strtoi(char *arg, int* value)
{
    int error;
    long lvalue = *value;
    error = safe_strtol(arg, &lvalue);
    *value = (int) lvalue;
    return error;
}

extern
int safe_strtoul(char *arg, unsigned long* value)
{
    char *endptr;
    int errno_save = errno;
//    assert(arg!=NULL);
    errno = 0;
    *value = strtoul(arg, &endptr, 0);
    if (errno != 0 || *endptr!='\0' || endptr==arg) {
        return 1;
    }
    errno = errno_save;
    return 0;
}

extern
int safe_strtod(char *arg, double* value)
{
   char *endptr;
   int errno_save = errno;
//   assert(arg!=NULL);
   errno = 0;
   *value = strtod(arg, &endptr);
   if (errno != 0 || *endptr!='\0' || endptr==arg) {
       return 1;
   }
   errno = errno_save;
   return 0;
}

extern
int safe_strtol(char *arg, long* value)
{
    char *endptr;
    int errno_save = errno;

//    assert(arg!=NULL);
    errno = 0;
    *value = strtol(arg, &endptr, 0);
    if (errno != 0 || *endptr!='\0' || endptr==arg) {
        return 1;
    }
    errno = errno_save;
    return 0;
}



/* debug.h -- use to printf debug info
 * by bowenguo, 2008-11-25 */

#ifndef _DEBUG_H
#define _DEBUG_H

#ifdef _DEBUG
#define DBG(x)  do { fprintf x ; putc('\n', stderr); fflush(stderr); } while (0)
#else
#define DBG(x)
#endif /* DEBUG */

#endif

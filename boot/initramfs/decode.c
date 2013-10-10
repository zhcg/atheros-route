/*  decode.c -- lzma decode stream of data.
*       The main function is decode(getbuf)...
*       input is via repeated calls to (*getbuf) and ouput is via
*       repeated calls to unarchiveBuf() (see unarchive.c).
*
* Adapted from LzmaUtil/LzmaUtil.c Decode() function
*       (which file is marked public domain).
*/

#include <unistd.h>
#include <string.h>

#include "lzma/Types.h"
#include "lzma/LzmaDec.h"
// #include "lzma/Alloc.h"
/* From unarchive.c */
void unarchiveBuf(const unsigned char *buf, unsigned nbytes);


/* MEMORY ALLOCATION HACK!
* Using malloc would pull in too much code for "/init" (inittrampoline).
* So instead we just allocate memory from the kernel each time
* and never free it... the total memory leak is small, since 
* inittrampoline is a short lived program and the amount of memory 
* allocated is in any case pretty fixed.
*/
static inline void *decodeMalloc(unsigned size)
{
    #if 0       /* normal */
    return malloc(size+1/*to be sure*/);
    #else
    return sbrk(size+4/*to be sure*/);
    #endif
}
static inline void decodeFree(void *p) 
{
    #if 0       /* normal */
    free(p);
    #else
    /* leak memory! */
    #endif
}

static void *SzAlloc(void *p, size_t size) { p = p; return decodeMalloc(size); }
static void SzFree(void *p, void *address) {  p = p; decodeFree(address); }
static ISzAlloc g_Alloc = { SzAlloc, SzFree };
#define OUT_BUF_SIZE (1 << 16)

int decode(
        unsigned char *(*getbuf)(unsigned *nbytes))       /* callback to get next mem */
{
  UInt64 unpackSize;
  int thereIsSize; /* = 1, if there is uncompressed size in headers */
  int i;
  int res = 0;
  Byte *inBuf;
  Byte outBuf[OUT_BUF_SIZE];
  size_t inPos = 0; 
  unsigned inSize = 0;
  size_t outPos = 0;
  
  CLzmaDec state;

  /* header: 5 bytes of LZMA properties and 8 bytes of uncompressed size */
  unsigned char header[LZMA_PROPS_SIZE + 8];

  inBuf = (*getbuf)(&inSize);
  inPos = 0;

  /* Read and parse header */
  for (inPos = 0; inPos < sizeof(header); inPos++)
    header[inPos] = inBuf[inPos];

  unpackSize = 0;
  thereIsSize = 0;
  for (i = 0; i < 8; i++)
  {
    unsigned char b = header[LZMA_PROPS_SIZE + i];
    if (b != 0xFF)
      thereIsSize = 1;
    unpackSize += (UInt64)b << (i * 8);
  }

  LzmaDec_Construct(&state);
  res = LzmaDec_Allocate(&state, header, LZMA_PROPS_SIZE, &g_Alloc);
  if (res != SZ_OK)
    return res;
  {
    LzmaDec_Init(&state);
    for (;;)
    {
      if (inPos == inSize)
      {
        inBuf = (*getbuf)(&inSize);
        inPos = 0;
      }
      {
        SizeT inProcessed = inSize - inPos;
        SizeT outProcessed = OUT_BUF_SIZE - outPos;
        ELzmaFinishMode finishMode = LZMA_FINISH_ANY;
        ELzmaStatus status;
        if (thereIsSize && outProcessed > unpackSize)
        {
          outProcessed = (SizeT)unpackSize;
          finishMode = LZMA_FINISH_END;
        }

        res = LzmaDec_DecodeToBuf(&state, outBuf + outPos, &outProcessed,
            inBuf + inPos, &inProcessed, finishMode, &status);
        inPos += (UInt32)inProcessed;
        outPos += outProcessed;
        unpackSize -= outProcessed;

        unarchiveBuf(outBuf, outPos);
        outPos = 0;

        if (res != SZ_OK || (thereIsSize && unpackSize == 0))
          break;

        if (inProcessed == 0 && outProcessed == 0)
        {
          if (thereIsSize || status != LZMA_STATUS_FINISHED_WITH_MARK)
            res = SZ_ERROR_DATA;
          break;
        }
      }
    }
  }

  LzmaDec_Free(&state, &g_Alloc);
  return res;
}


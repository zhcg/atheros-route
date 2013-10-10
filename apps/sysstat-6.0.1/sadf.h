/*
 * sadf: System activity data formatter
 * (C) 1999-2005 by Sebastien Godard (sysstat <at> wanadoo.fr)
 */

#ifndef _SADF_H
#define _SADF_H


#define PT_NOFLAG  0x0000	/* Prevent undescribed '0' in render calls */
#define PT_USEINT  0x0001	/* Use the integer final arg, not double */
#define PT_NEWLIN  0x0002	/* Terminate the current output line */

#define NOVAL      0		/* For placeholder zeros */
#define DNOVAL     0.0		/* Wilma!  */

/* DTD version for XML output */
#define XML_DTD_VERSION	"1.0"


static char *seps[] =  {"\t", ";"};

/*
 * Conses are used to type independent passing
 * of variable optional data into our rendering routine.
 */

typedef enum e_tcons {iv, sv} tcons; /* Types of conses */

typedef struct {
   tcons t;			/* Type in {iv,sv} */
   union {
      unsigned long int i;
      char *s;
   } a, b;			/* Value pair, either ints or char *s */
} Cons;


#endif  /* _SADF_H */

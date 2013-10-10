#ifndef __TIME_H__
#define __TIME_H__

#define ulong unsigned long

int timer_init(void);
void reset_timer(void);
ulong get_timer(ulong base);
void set_timer(ulong t);
void udelay (unsigned long usec);
unsigned long long get_ticks(void);
ulong get_tbclk(void);

#endif /* __TIME_H__ */

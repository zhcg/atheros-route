ifneq ($(CONFIG_ATH_2X8),y)
obj-y += timekeeping.o ntp.o clocksource.o jiffies.o timer_list.o timecompare.o
else
obj-y += timekeeping.o ntp.o clocksource.o jiffies.o timecompare.o
endif

obj-$(CONFIG_GENERIC_CLOCKEVENTS_BUILD)		+= clockevents.o
obj-$(CONFIG_GENERIC_CLOCKEVENTS)		+= tick-common.o
obj-$(CONFIG_GENERIC_CLOCKEVENTS_BROADCAST)	+= tick-broadcast.o
obj-$(CONFIG_TICK_ONESHOT)			+= tick-oneshot.o
obj-$(CONFIG_TICK_ONESHOT)			+= tick-sched.o
obj-$(CONFIG_TIMER_STATS)			+= timer_stats.o

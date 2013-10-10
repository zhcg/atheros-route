
obj-y := handle.o manage.o spurious.o resend.o chip.o devres.o
obj-$(CONFIG_GENERIC_IRQ_PROBE) += autoprobe.o
ifneq ($(CONFIG_ATH_2X8),y)
obj-$(CONFIG_PROC_FS) += proc.o
endif
obj-$(CONFIG_GENERIC_PENDING_IRQ) += migration.o
obj-$(CONFIG_NUMA_IRQ_DESC) += numa_migrate.o
obj-$(CONFIG_PM_SLEEP) += pm.o

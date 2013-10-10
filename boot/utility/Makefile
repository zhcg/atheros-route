DIRS = adjust_dep adjust_time bin2hex imghdr patch_gen checksum

all:
	for i in $(DIRS) ; do make -C $$i || exit $? ; done

clean :
	for i in $(DIRS) ; do make -C $$i clean; done


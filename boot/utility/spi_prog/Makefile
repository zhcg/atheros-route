include ./include/Makefile.h

#
# Makefile of building magpie/k2 project
#
#

TESTS           = $(basename $(wildcard *.c))

all: $(TESTS)

$(TESTS):
	$(MAKE) TEST_NAME=$@ compile_test binary image

compile_test: 
	../../toolchain/tools/RB-2007.2-linux/XtensaTools/bin/xt-xcc -g -c $(TEST_NAME).c -o $(TEST_NAME).o

LINKFLAGS_sw =						\
	$(TARGET_LDFLAGS)					\
	-Wl,-T./mem.lds				\
	./include/_vectors.o				\
	./include/crt1-tiny.o				\
	./$(TEST_NAME).o					\
	-Wl,-static							\
	-Wl,--start-group					\
	./include/libhandlers-board.a		\
	-Wl,--end-group						\
	-lhal							    \
	-lgcc							    \
	-lc

build: binary image

#
#  	link all libraries
#
binary:
	$(CC) $(LINKFLAGS_sw) -Wl,-Map,$(TEST_NAME).map -o $(TEST_NAME).out 
	$(OBJDUMP) -Dlxs $(TEST_NAME).out > $(TEST_NAME).objdump

#
#	Find out where is the data_start_in_rom which means the end of ro_data segment,
#   and append the data_segment to there
#
ds_in_rom = `$(NM) $(TEST_NAME).out | grep "_data_start_in_rom" | cut -b -8`
ds_in_romN = `pcregrep "." ./temp_search_file2`

#
# 	final binary image
#
image:

	@echo "0x$(ds_in_rom)" > ./temp_search_file
	@sed 's/\(0x004\)\([0-9]*\)/0x008\2/' ./temp_search_file > ./temp_search_file2
	$(OBJCOPY)											    \
		--change-section-lma .lit4+0x400000				    \
		--change-section-vma .lit4+0x400000				    \
		--change-section-lma .rodata+0x400000			    \
		--change-section-vma .rodata+0x400000			    \
		--change-section-lma .dport0.usb_in_rom+0x400000	\
		--change-section-vma .dport0.usb_in_rom+0x400000	\
		--remove-section     .dport0.data 				    \
		--remove-section     .bss		 				    \
		--change-section-lma .data=$(ds_in_romN) 		    \
		--change-section-vma .data=$(ds_in_romN) 		    \
		-O binary $(TEST_NAME).out $(TEST_NAME).bin
	@$(RM) ./temp_search_file ./temp_search_file2 ./*.o

diss:
	$(OBJDUMP) -D *.out > test.b
	
clean:
	@$(RM) -rf ./*.out ./*.bin ./*.objdump ./*.map

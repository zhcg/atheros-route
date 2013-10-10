OUTPUT_FORMAT("elf32-tradbigmips", "elf32-tradbigmips", "elf32-tradbigmips")
OUTPUT_ARCH(mips)
ENTRY(_start)
SECTIONS
{
	. = 0x00000000;

	. = ALIGN(4);
	.text       :
	{
	  *(EXCLUDE_FILE(usb_suspend.o) .text)
	}

	. = ALIGN(4);
	.rodata  : { *(.rodata) }

	. = ALIGN(4);
	.data  : { *(.data) }

	. = ALIGN(4);
	.sdata  : { *(.sdata) }
        
	_gp = ALIGN(16);

	__got_start = .;
	.got  : { *(.got) }
	__got_end = .;

	.sdata  : { *(.sdata) }

	. = ALIGN(4);
    __bss_start = .;
	.sbss  : { *(.sbss) }
	.bss  : { *(.bss) }
    __bss_end = .;

	. = ALIGN(4);
    _end = .;
}

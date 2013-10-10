OUTPUT_FORMAT("elf32-tradbigmips", "elf32-tradbigmips", "elf32-tradbigmips")
OUTPUT_ARCH(mips)
ENTRY(_start)

SECTIONS
{
	. = 0xBFC00000;

	_rom_start = .;
	. = ALIGN(4);
	.text	:
	{
		*(.text)
	}

	. = ALIGN(4);
	.rodata	:
	{
		*(.rodata) *(.rodata.*)
	}

	. = ALIGN(4);

	_rom_end = .;

	. = 0xBD000000;

	_boot_data_start = .;

	.data	:
	{
		*(.indir_tbl.data)
		*(.data)
	}

	_gp = ALIGN(16);

	__got_start = .;
	.got	: { *(.got) }
	__got_end = .;

	boot_end_data = .;
	num_got_entries = (__got_end - __got_start) >> 2;

	_boot_data_end = .;

	. = ALIGN(4);
	.sbss	: { *(.sbss) }
	.bss	: { *(.bss) }
	boot_end = .;
}

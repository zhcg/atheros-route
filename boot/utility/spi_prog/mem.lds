/* This linker script generated from xt-genldscripts.tpp for LSP v3lsp */
/* Linker Script for ld -N */

/*
	1. Xtensa Exception Vector is 4KB - tensilica specific vector & symbol
					virtual address				0x8e0000 ~ 0x8e0fff (4KB)
					physical address			0x0e0000 ~ 0x0e0fff (4KB)

	2. IROM text segment is 64KB - text data
					virtual address				0x4e1000 ~ 0x0f1fff (64KB)
					physical address			0x0e1000 ~ 0x0f1fff (64KB)

	3. DROM rodata segment is 32KB - read only data
					virtual address				0x8f2000 ~ 0x8f7fff (32KB)
					physical address			0x0f2000 ~ 0x0f7fff (32KB)

	4. data, bss is beginning IRAM for 16KB	- dynamic allocated data segment
					virtual address				0x500000 ~ 0x503fff (16KB)
*/

MEMORY
{
  ResetVector_text_seg :                org = 0x00920000, len = 0x800
  WindowVectors_text_seg :              org = 0x00920800, len = 0x200
  Level2InterruptVector_lit_seg :       org = 0x00920A00, len = 0x20
  Level2InterruptVector_text_seg :      org = 0x00920A20, len = 0xE0
  Level3InterruptVector_lit_seg :       org = 0x00920B00, len = 0x20
  Level3InterruptVector_text_seg :      org = 0x00920B20, len = 0xE0
  DebugExceptionVector_lit_seg :        org = 0x00920C00, len = 0x20
  DebugExceptionVector_text_seg :       org = 0x00920C20, len = 0xE0
  NMIExceptionVector_lit_seg  :         org = 0x00920CE0, len = 0x4
  NMIExceptionVector_text_seg  :        org = 0x00920CE4, len = 0x1C
  KernelExceptionVector_lit_seg :       org = 0x00920D00, len = 0x40
  KernelExceptionVector_text_seg :      org = 0x00920D40, len = 0xC0
  UserExceptionVector_lit_seg :         org = 0x00920E00, len = 0x20
  UserExceptionVector_text_seg :        org = 0x00920E20, len = 0xE0
  DoubleExceptionVector_lit_seg :       org = 0x00920F00, len = 0x20
  DoubleExceptionVector_text_seg :      org = 0x00920F20, len = 0xE0
  irom_seg :                      	    org = 0x00921000, len = 0x2000
  drom_seg :                       	    org = 0x00523000, len = 0x1800
  dram_seg :                            org = 0x00524800, len = 0x1000
  dram1_seg :                           org = 0x00526000, len = 0x800
  dram2_seg :                           org = 0x00920100, len = 0x400
}

PHDRS
{
  ResetVector_text_phdr PT_LOAD;
  WindowVectors_text_phdr PT_LOAD;
  Level2InterruptVector_lit_phdr PT_LOAD;
  Level2InterruptVector_text_phdr PT_LOAD;
  Level3InterruptVector_lit_phdr PT_LOAD;
  Level3InterruptVector_text_phdr PT_LOAD;
  DebugExceptionVector_lit_phdr PT_LOAD;
  DebugExceptionVector_text_phdr PT_LOAD;
  NMIExceptionVector_lit_phdr PT_LOAD;
  NMIExceptionVector_text_phdr PT_LOAD;
  KernelExceptionVector_lit_phdr PT_LOAD;
  KernelExceptionVector_text_phdr PT_LOAD;
  UserExceptionVector_lit_phdr PT_LOAD;
  UserExceptionVector_text_phdr PT_LOAD;
  DoubleExceptionVector_lit_phdr PT_LOAD;
  DoubleExceptionVector_text_phdr PT_LOAD;
  irom_phdr PT_LOAD;
  drom_phdr PT_LOAD;
  dram_phdr PT_LOAD;
  dram1_phdr PT_LOAD;
  dram2_phdr PT_LOAD;
}

/*  Default entry point:  */
ENTRY(_ResetVector)
 _rom_store_table = 0; 
PROVIDE(_memmap_vecbase_reset = 0x920800);
/* Various memory-map dependent cache attribute settings: */
_memmap_cacheattr_wb_base = 0x00021001;
_memmap_cacheattr_wt_base = 0x00021001;
_memmap_cacheattr_bp_base = 0x00022002;
_memmap_cacheattr_unused_mask = 0xFFF00FF0;
_memmap_cacheattr_wb_trapnull = 0x22221221;
_memmap_cacheattr_wt_trapnull = 0x22221221;
_memmap_cacheattr_bp_trapnull = 0x22222222;
_memmap_cacheattr_wb_strict = 0xFFF21FF1;
_memmap_cacheattr_wt_strict = 0xFFF21FF1;
_memmap_cacheattr_bp_strict = 0xFFF22FF2;
_memmap_cacheattr_wb_allvalid = 0x22221221;
_memmap_cacheattr_wt_allvalid = 0x22221221;
_memmap_cacheattr_bp_allvalid = 0x22222222;
PROVIDE(_memmap_cacheattr_reset = _memmap_cacheattr_wb_trapnull);

SECTIONS
{
  .lit4 :
  {
    _lit4_start = ABSOLUTE(.);
    *(*.lit4)
    *(.gnu.linkonce.lit4.*)
    _lit4_end = ABSOLUTE(.);
  } >drom_seg :drom_phdr

  .ResetVector.literal :
  {
    _ResetVector_literal_start = ABSOLUTE(.);
    *(.ResetVector.literal)
    _ResetVector_literal_end = ABSOLUTE(.);
  } >drom_seg :drom_phdr

  .dport0.stack :
  {
    _reserved_stack_start = .;
    _stack_sentry = ALIGN(0x8);
    . = . + 2048;   /* 2KB for stack */
    __stack = .;
    _reserved_stack_end = .;
  } >dram1_seg :dram1_phdr

  .dport0.usb_in_rom :
  {
    _reserved_rom_data_start = .;
    _reserved_rom_data_end = .;
    _data_start_in_rom = 0x160;
  } >dram2_seg :dram2_phdr
  
  .dport0.data :
  {
    _reserved_data_start = 0x0;
    _indir_tbl = 0x0;
    /*_host_interest = 0x400;*/
    . = 0x400;
    _reserved_data_end = .;
    _dport0_data_start = ABSOLUTE(.);
    *(.dport0.data)
    *(.dport.data)
    _dport0_data_end = ABSOLUTE(.);
  } >dram_seg :dram_phdr

 .dport0.bss :
  {
    . = ALIGN (8);
    _dport0_bss_start = ABSOLUTE(.);
    *(.dport0.bss)
    . = ALIGN (8);
    _dport0_bss_end = ABSOLUTE(.);
  } >dram_seg :dram_phdr

  .dram0.rodata :
  {
    _dram0_rodata_start = ABSOLUTE(.);
    *(.dram0.rodata)
    *(.dram.rodata)
    _dram0_rodata_end = ABSOLUTE(.);
  } >drom_seg :drom_phdr

  .rodata :
  {
    _rodata_start = ABSOLUTE(.);
    *(.rodata)
    *(.rodata.*)
    *(.gnu.linkonce.r.*)
    *(.rodata1)
    __XT_EXCEPTION_TABLE__ = ABSOLUTE(.);
    *(.xt_except_table)
    *(.gcc_except_table)
    *(.gnu.linkonce.e.*)
    *(.gnu.version_r)
    . = ALIGN(4);		/* this table MUST be 4-byte aligned */
    _bss_table_start = ABSOLUTE(.);
    LONG(_dport0_bss_start)
    LONG(_dport0_bss_end)
    LONG(_bss_start)
    LONG(_bss_end)
    _bss_table_end = ABSOLUTE(.);
    _rodata_end = ABSOLUTE(.);
	/* _data_start_in_rom = ALIGN(8); */
  } >drom_seg :drom_phdr

  .dram0.literal :
  {
    _dram0_literal_start = ABSOLUTE(.);
    *(.dram0.literal)
    *(.dram.literal)
    _dram0_literal_end = ABSOLUTE(.);
  } >drom_seg :drom_phdr

  .dram0.data :
  {
    _dram0_data_start = ABSOLUTE(.);
    *(.dram0.data)
    *(.dram.data)
    _dram0_data_end = ABSOLUTE(.);
  } >dram_seg :dram_phdr

  .data :
  {
    _data_start = ABSOLUTE(.);
    *(.data)
    *(.data.*)
    *(.gnu.linkonce.d.*)
    *(.data1)
    *(.sdata)
    *(.sdata.*)
    *(.gnu.linkonce.s.*)
    *(.sdata2)
    *(.sdata2.*)
    *(.gnu.linkonce.s2.*)
    *(.jcr)
    *(.eh_frame)
    /*  C++ constructor and destructor tables, properly ordered:  */
    KEEP (*crtbegin.o(.ctors))
    KEEP (*(EXCLUDE_FILE (*crtend.o) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
    KEEP (*crtbegin.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.o) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
    /*  C++ exception handlers table:  */
    __XT_EXCEPTION_DESCS__ = ABSOLUTE(.);
    *(.xt_except_desc)
    *(.gnu.linkonce.h.*)
    __XT_EXCEPTION_DESCS_END__ = ABSOLUTE(.);
    *(.xt_except_desc_end)
    *(.dynamic)
    *(.gnu.version_d)
    _data_end = ABSOLUTE(.);
  } >dram_seg :dram_phdr

  .bss :
  {
    . = ALIGN (8);
    _bss_start = ABSOLUTE(.);
    *(.dynsbss)
    *(.sbss)
    *(.sbss.*)
    *(.gnu.linkonce.sb.*)
    *(.scommon)
    *(.sbss2)
    *(.sbss2.*)
    *(.gnu.linkonce.sb2.*)
    *(.dynbss)
    *(.bss)
    *(.bss.*)
    *(.gnu.linkonce.b.*)
    *(COMMON)
    *(.dram0.bss)
    . = ALIGN (8);
    _bss_end = ABSOLUTE(.);
    . += 0x80;  /* for usb device descriptor */
    _end = ALIGN(0x8);
    PROVIDE(end = ALIGN(0x8));
  } >dram_seg :dram_phdr


  .ResetVector.text :
  {
    _ResetVector_text_start = ABSOLUTE(.);
    KEEP (*(.ResetVector.text))
    _ResetVector_text_end = ABSOLUTE(.);
  } >ResetVector_text_seg :ResetVector_text_phdr

  .WindowVectors.text :
  {
    _WindowVectors_text_start = ABSOLUTE(.);
    KEEP (*(.WindowVectors.text))
    _WindowVectors_text_end = ABSOLUTE(.);
  } >WindowVectors_text_seg :WindowVectors_text_phdr

  .KernelExceptionVector.literal :
  {
    _KernelExceptionVector_literal_start = ABSOLUTE(.);
    *(.KernelExceptionVector.literal)
    _KernelExceptionVector_literal_end = ABSOLUTE(.);
  } >KernelExceptionVector_lit_seg :KernelExceptionVector_lit_phdr

  .KernelExceptionVector.text :
  {
    _KernelExceptionVector_text_start = ABSOLUTE(.);
    KEEP (*(.KernelExceptionVector.text))
    _KernelExceptionVector_text_end = ABSOLUTE(.);
  } >KernelExceptionVector_text_seg :KernelExceptionVector_text_phdr

  .UserExceptionVector.literal :
  {
    _UserExceptionVector_literal_start = ABSOLUTE(.);
    *(.UserExceptionVector.literal)
    _UserExceptionVector_literal_end = ABSOLUTE(.);
  } >UserExceptionVector_lit_seg :UserExceptionVector_lit_phdr

  .UserExceptionVector.text :
  {
    _UserExceptionVector_text_start = ABSOLUTE(.);
    KEEP (*(.UserExceptionVector.text))
    _UserExceptionVector_text_end = ABSOLUTE(.);
  } >UserExceptionVector_text_seg :UserExceptionVector_text_phdr

  .DoubleExceptionVector.literal :
  {
    _DoubleExceptionVector_literal_start = ABSOLUTE(.);
    *(.DoubleExceptionVector.literal)
    _DoubleExceptionVector_literal_end = ABSOLUTE(.);
  } >DoubleExceptionVector_lit_seg :DoubleExceptionVector_lit_phdr

  .DoubleExceptionVector.text :
  {
    _DoubleExceptionVector_text_start = ABSOLUTE(.);
    KEEP (*(.DoubleExceptionVector.text))
    _DoubleExceptionVector_text_end = ABSOLUTE(.);
  } >DoubleExceptionVector_text_seg :DoubleExceptionVector_text_phdr

  .Level2InterruptVector.literal :
  {
    _Level2InterruptVector_literal_start = ABSOLUTE(.);
    *(.Level2InterruptVector.literal)
    _Level2InterruptVector_literal_end = ABSOLUTE(.);
  } >Level2InterruptVector_lit_seg :Level2InterruptVector_lit_phdr

  .Level2InterruptVector.text :
  {
    _Level2InterruptVector_text_start = ABSOLUTE(.);
    KEEP (*(.Level2InterruptVector.text))
    _Level2InterruptVector_text_end = ABSOLUTE(.);
  } >Level2InterruptVector_text_seg :Level2InterruptVector_text_phdr

  .Level3InterruptVector.literal :
  {
    _Level3InterruptVector_literal_start = ABSOLUTE(.);
    *(.Level3InterruptVector.literal)
    _Level3InterruptVector_literal_end = ABSOLUTE(.);
  } >Level3InterruptVector_lit_seg :Level3InterruptVector_lit_phdr

  .Level3InterruptVector.text :
  {
    _Level3InterruptVector_text_start = ABSOLUTE(.);
    KEEP (*(.Level3InterruptVector.text))
    _Level3InterruptVector_text_end = ABSOLUTE(.);
  } >Level3InterruptVector_text_seg :Level3InterruptVector_text_phdr

  .DebugExceptionVector.literal :
  {
    _DebugExceptionVector_literal_start = ABSOLUTE(.);
    *(.DebugExceptionVector.literal)
    _DebugExceptionVector_literal_end = ABSOLUTE(.);
  } >DebugExceptionVector_lit_seg :DebugExceptionVector_lit_phdr

  .DebugExceptionVector.text :
  {
    _DebugExceptionVector_text_start = ABSOLUTE(.);
    KEEP (*(.DebugExceptionVector.text))
    _DebugExceptionVector_text_end = ABSOLUTE(.);
  } >DebugExceptionVector_text_seg :DebugExceptionVector_text_phdr

  .NMIExceptionVector.literal :
  {
    _NMIExceptionVector_literal_start = ABSOLUTE(.);
    *(.NMIExceptionVector.literal)
    _NMIExceptionVector_literal_end = ABSOLUTE(.);
  } >NMIExceptionVector_lit_seg :NMIExceptionVector_lit_phdr

  .NMIExceptionVector.text :
  {
    _NMIExceptionVector_text_start = ABSOLUTE(.);
    KEEP (*(.NMIExceptionVector.text))
    _NMIExceptionVector_text_end = ABSOLUTE(.);
  } >NMIExceptionVector_text_seg :NMIExceptionVector_text_phdr

  .iram0.text :
  {
    _iram0_text_start = ABSOLUTE(.);
    *(.iram0.literal .iram.literal .iram.text.literal .iram0.text .iram.text)
    _iram0_text_end = ABSOLUTE(.);
  } >irom_seg :irom_phdr

  .text :
  {
    _stext = .;
    _text_start = ABSOLUTE(.);
    *(.entry.text)
    KEEP (*(.init))
    *(.init)
    *(.literal .text .literal.* .text.* .stub .gnu.warning .gnu.linkonce.literal.* .gnu.linkonce.t.*.literal .gnu.linkonce.t.*)
    *(.fini.literal)
    KEEP (*(.fini))
    *(.gnu.version)
    _text_end = ABSOLUTE(.);
    _text_end_magic = .;
    _etext = .;
    /* _data_start_in_rom = ALIGN(8); */
  } >irom_seg :irom_phdr

  .debug  0 :  { *(.debug) }
  .line  0 :  { *(.line) }
  .debug_srcinfo  0 :  { *(.debug_srcinfo) }
  .debug_sfnames  0 :  { *(.debug_sfnames) }
  .debug_aranges  0 :  { *(.debug_aranges) }
  .debug_pubnames  0 :  { *(.debug_pubnames) }
  .debug_info  0 :  { *(.debug_info) }
  .debug_abbrev  0 :  { *(.debug_abbrev) }
  .debug_line  0 :  { *(.debug_line) }
  .debug_frame  0 :  { *(.debug_frame) }
  .debug_str  0 :  { *(.debug_str) }
  .debug_loc  0 :  { *(.debug_loc) }
  .debug_macinfo  0 :  { *(.debug_macinfo) }
  .debug_weaknames  0 :  { *(.debug_weaknames) }
  .debug_funcnames  0 :  { *(.debug_funcnames) }
  .debug_typenames  0 :  { *(.debug_typenames) }
  .debug_varnames  0 :  { *(.debug_varnames) }

  .xt.insn 0 :
  {
    KEEP (*(.xt.insn))
    KEEP (*(.gnu.linkonce.x.*))
  }
  .xt.prop 0 :
  {
    KEEP (*(.xt.prop))
    KEEP (*(.xt.prop.*))
    KEEP (*(.gnu.linkonce.prop.*))
  }
  .xt.lit 0 :
  {
    KEEP (*(.xt.lit))
    KEEP (*(.xt.lit.*))
    KEEP (*(.gnu.linkonce.p.*))
  }
  .xt.profile_range 0 :
  {
    KEEP (*(.xt.profile_range))
    KEEP (*(.gnu.linkonce.profile_range.*))
  }
  .xt.profile_ranges 0 :
  {
    KEEP (*(.xt.profile_ranges))
    KEEP (*(.gnu.linkonce.xt.profile_ranges.*))
  }
  .xt.profile_files 0 :
  {
    KEEP (*(.xt.profile_files))
    KEEP (*(.gnu.linkonce.xt.profile_files.*))
  }
}

/* PROVIDE (app_start = 0x00906000); */

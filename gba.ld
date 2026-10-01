OUTPUT_ARCH(arm)
ENTRY(_start)
MEMORY { ROM (rx) : ORIGIN = 0x08000000, LENGTH = 32M
         IWRAM (rwx) : ORIGIN = 0x03000000, LENGTH = 32K }
SECTIONS {
  .text : { *(.text.start) *(.text*) *(.rodata*) . = ALIGN(4); } > ROM
  __data_lma = .;
  .data : AT(__data_lma) { __data_start = .; *(.data*) . = ALIGN(4); __data_end = .; } > IWRAM
  .bss (NOLOAD) : { __bss_start = .; *(.bss*) *(COMMON) . = ALIGN(4); __bss_end = .; } > IWRAM
  /DISCARD/ : { *(.ARM.exidx*) *(.comment) }
}

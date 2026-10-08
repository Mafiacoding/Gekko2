/* Experimental link address; a compatible RAM loader must reserve this
 * complete region first. No NAND installer or fixed IOS patch is provided. */
OUTPUT_FORMAT("elf32-bigarm")
OUTPUT_ARCH(arm)
ENTRY(_start)
PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); }
SECTIONS {
 . = WORKER_BASE;
 .init : { KEEP(*(.init)) } :text
 .text : { *(.text*) } :text
 .rodata : { *(.rodata*) } :text
 . = ALIGN(4096);
 .data : { *(.data*) } :data
 .bss (NOLOAD) : { *(.bss*) *(COMMON) } :data
 .ios_info_table : { KEEP(*(.ios_info_table)) } :data
 /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) }
}

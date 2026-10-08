/* GPL-3.0+. RAM loader supplies SP and loads/zeros all ELF PT_LOAD spans.
 * IOS metadata layout follows d2x-cios Custom IOS Module Toolkit. */
.section .init,"ax",%progbits
.arm
.global _start
.type _start,%function
_start:
 b gekko2_ios_main
.section .bss.stack,"aw",%nobits
.balign 32
.space 0x3000
.global ios_thread_stack
ios_thread_stack:
.section .ios_info_table,"a",%progbits
.long 0,0x28,6
.long 0x0b,4
.long 0x09,_start
.long 0x7d,0x40
.long 0x7e,0x3000
.long 0x7f,ios_thread_stack
.section .note.GNU-stack,"",%progbits

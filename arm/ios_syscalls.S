/* ABI adaptation from Custom IOS Library (GPL-2.0-or-later):
 * Copyright (C) 2008 neimod, 2009 WiiGator/Waninkoko, 2010 Hermes,
 * 2011 davebaol. Source: wiidev/d2x-cios/source/cios-lib/syscalls.s.
 * Only the resource-manager and data-cache calls are needed. */
.arm
.text
.macro ios_call number name
.global \name
.type \name,%function
\name:
 .long 0xe6000010 + (\number << 5)
 bx lr
.endm
ios_call 0x0a ios_queue_create
ios_call 0x0e ios_queue_receive
ios_call 0x1b ios_register
ios_call 0x2a ios_ack
ios_call 0x3f ios_invalidate
ios_call 0x40 ios_flush
.section .note.GNU-stack,"",%progbits

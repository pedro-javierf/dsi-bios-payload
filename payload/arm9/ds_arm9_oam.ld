/*--------------------------------------------------------------------------------
	This Source Code Form is subject to the terms of the Mozilla Public License,
	v. 2.0. If a copy of the MPL was not distributed with this file, You can
	obtain one at https://mozilla.org/MPL/2.0/.
--------------------------------------------------------------------------------*/
OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY {
	ewramhi	: ORIGIN = 0x02200000, LENGTH = 1M
	oam	: ORIGIN = 0x07000000, LENGTH = 2K
}

PHDRS {
	main    PT_LOAD FLAGS(7);
	dtcm    PT_LOAD FLAGS(7);
	itcm    PT_LOAD FLAGS(7);
	vectors PT_LOAD FLAGS(7);
	twl     PT_LOAD FLAGS(0x100007);
}

__und_sp = 0x02ee4000;

__start_code_oam = ORIGIN(oam) + 0x80;

SECTIONS
{
	. = __start_code_oam

	.text ALIGN (4):
	{
		__text_start = ABSOLUTE(.);
		KEEP(*(.text.entry))
		*(.text* .rodata*)
		__text_end = ABSOLUTE(.);
	} >oam :main = 0xff

	.data ALIGN (4) :
	{
		__data_start = ABSOLUTE(.);
		*(.data*)
		. = ALIGN(4);
		__data_end = ABSOLUTE(.) ;
	} >oam :main = 0xff

	__bss_vma = . ;
	.bss __bss_vma (NOLOAD):
	{
		__bss_start = ABSOLUTE(.);
		*(.bss* .sbss*)
		. = ALIGN(4);    /* REQUIRED. LD is flaky without it. */
		__bss_end = ABSOLUTE(.) ;

		__dumpspace = ABSOLUTE(.);
	} >oam :NONE
}


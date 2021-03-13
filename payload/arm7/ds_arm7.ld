OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

PHDRS {
	crt0  PT_LOAD FLAGS(7);
	arm7  PT_LOAD FLAGS(7);
	arm7i PT_LOAD FLAGS(0x100007);
}


MEMORY {
	ewram  : ORIGIN = 0x02380000, LENGTH = 12M - 512K
	rom    : ORIGIN = 0x08000000, LENGTH = 32M
	iwram  : ORIGIN = 0x037f8000, LENGTH = 96K
	iwramhi	: ORIGIN = 0x0380c000, LENGTH = 16K

	macmem : ORIGIN = 0x04804000, LENGTH = 8K - 4*32

	twl_ewram : ORIGIN = 0x02e80000, LENGTH = 512K - 64K
	twl_iwram : ORIGIN = 0x03000000, LENGTH = 256K
}

__iwram_start	=	ORIGIN(iwram);
__iwram_top	=	ORIGIN(iwram)+ LENGTH(iwram);

__sp_irq	=	__iwram_top - 0x100;
__sp_svc	=	__sp_irq - 0x100;
__sp_usr	=	__sp_svc - 0x100;

__irq_flags	=	0x04000000 - 8;
__irq_flagsaux	=	0x04000000 - 0x40;
__irq_vector	=	0x04000000 - 4;

__und_sp = 0x04000000 - 0x100; /* good enough */

SECTIONS
{
	. = ORIGIN(macmem);

	__dumpspace = ABSOLUTE(ORIGIN(macmem)); /* we only need 32k of it anyway, and we're way up */

	.text ALIGN (4) :
	{
		__text_start = ABSOLUTE(.);
		KEEP (*(.text.entry))
		. = ALIGN(256);
		__nopsled_start = ABSOLUTE(.);
		KEEP (*(.text.nopsled))
		__nopsled_end = ABSOLUTE(.);
		*(.text* .rodata*)
		__text_end = ABSOLUTE(.);
		. = ALIGN(4);  /* REQUIRED. LD is flaky without it. */
	} >macmem :arm7

	.data ALIGN(4) : {
		__data_start = ABSOLUTE(.);
		*(.data*)
		. = ALIGN(4);
		__data_end = ABSOLUTE(.) ;
	} >macmem :arm7

	__bss_vma = . ;
	.bss ALIGN(4) (NOLOAD) :
	{
		__bss_start = ABSOLUTE(.);
		*(.bss* .sbss*)
		. = ALIGN(4);    /* REQUIRED. LD is flaky without it. */
		__bss_end = ABSOLUTE(.);
	} >macmem :NONE
}

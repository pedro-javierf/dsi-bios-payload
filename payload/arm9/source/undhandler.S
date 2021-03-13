@ vim: set ft=armv5 noet:

	.arch armv5te
	.cpu  arm946e-s

@ r15=pc r14=lr r13=sp r12=ip r11=fp r10=sl r9=sb

	.extern undhandler9_c

	.section .text.entry, "ax", %progbits
	.arm
	.global undhandler_9
	.type   undhandler_9, %function
	.global _start
	.type   _start, %function
_start:
undhanlder_9:
	@ save ctx etc
	ldr   r12, =regbak
	stmia r12, {r0-r11}
	str   sp , [r12, #(oldsp - regbak)]

	mrs   r13, cpsr
	str   r13, [r12, #(oldcpsr - regbak)]
	@ get a stack
	ldr r13, .Lundstack

	@@ reenable MPU - nah
	@mcr p15, 0, r0, c1, c0, 0
	@orr r0, #1
	@mcr p15, 0, r0, c1, c0, 0
	ldr r0, =((0<<19) /* ITCM load mode */ \
			| (1<<18) /* ITCM enable */ \
			| (0<<17) /* DTCM load mode */ \
			| (1<<16) /* DTCM enable */ \
			| (0<<15) /* thumb modeswitch disable (0=>enabled) */ \
			| (0<<14) /* round robin replacement (0=>pseudorandom) */ \
			| (1<<13) /* alt. vector (0xffff0000 IVT) (needed for Bios9i) (0=>0x00000000) */ \
			| (0<<12) /* icache enable (0: disabled) */ \
			| (0<< 7) /* endianness (0: little, 1: big) */ \
			| (0<< 2) /* dcache enable (0: disabled) */ \
			| (0<< 0) /* PU enable (0: disabled) */ \
	)
	mcr p15, 0, r0, c1, c0, 0

	@ grab some info from the BIOS stack to see where the exn happened
	@ exn pc
	ldr   r0, =0x02fffd90
	ldr   r2, [r0, #8]
	str   r2, [r12, #(pcbak - regbak)]
	@ exn ip, spsr
	ldmia r0, {r2, r12}
	@ banked cpsr, lr, sp
	@ first: modeswitch
	mrs   r3, cpsr
	bic   r4, r3, #0x1f
	and   r2, r2, #0x1f
	cmp   r2, #0x10
	moveq r2, #0x1f
	orr   r4, r2
	msr   cpsr, r4
	@ now back up ip, sp and lr
	ldr   r0, =ipbak
	stmia r0, {r12-r14}
	@ go back
	msr   cpsr, r3

	@ call C code
	blx undhandler9_c

	@ restore regs
	ldr   r12, =regbak
	ldmia r12, {r0-r11}
	ldr   sp, [r12, #(oldsp-regbak)]
	@ return to BIOS
	mov   pc, lr

.Lundstack:
	.4byte _start @__und_sp @ from linker script @ 0x02ff4000 ?
	.pool

	.section .bss.regbak, "aw", %nobits
	.global regbak
	.type   regbak, %object
regbak:
	.space 12*4
	.type ipbak, %object
ipbak:
	.space 4
	.type spbak, %object
spbak:
	.space 4
	.type lrbak, %object
lrbak:
	.space 4
	.type pcbak, %object
pcbak:
	.space 4
	.type oldsp, %object
oldsp:
	.space 4
oldcpsr:
	.space 4


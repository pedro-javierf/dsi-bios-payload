@ vim: set ft=armv4 noet:

	.arch armv4t
	.cpu  arm7tdmi

@ r15=pc r14=lr r13=sp r12=ip r11=fp r10=sl r9=sb

	.extern undhandler7_c
	.extern reboot_me

	.section .text.entry, "ax", %progbits
	.arm
	.global undhandler_7
	.type   undhandler_7, %function
	.global _start
#ifndef DIRECT_LINK
_start: @ 0x04804000
#endif
undhandler_7:
	ldr r2, =0x214e5454 @ "TTN!"
	cmp r0, r2
	pusheq {r1}
	beq .Lfillallram@b .Lfillallram @

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

	@ grab some info from the BIOS stack to see where the exn happened
	@ exn pc
	ldr   r0, =0x03ffffdc
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
	bl undhandler7_c

	@ SHOULD NEVER HAPPEN
1:	bal 1b
	@@ restore regs
	@ldr   r12, =regbak
	@ldmia r12, {r0-r11}
	@ldr   sp, [r12, #(oldsp-regbak)]
	@@ return to BIOS
	@mov   pc, lr

.Lundstack:
	@.4byte _start
	.4byte __und_sp @ from linker script (EWRAM)
	.pool

.Lfillallram:
	@ usable RAM:
	@  32k  WRAM-0: 0x037fff... (EWRAM)
	@  64k  WRAM-1: 0x038000... (IWRAM)
	@ 256k NWRAM-A: 0x030200...
	@ 256k NWRAM-B: 0x030400...
	@ 256k NWRAM-C: 0x030600...
	@ 256k VRAM C/D:0x060000...
	@ we can use 0x000010e0(r0=src, r1=dst, r2=len) (from BIOS) as a fast memcpy impl.

.macro call_bios_memcpy
	push {r12}@mov r7, r12
	bl .veneer_call_r11 @bx r11
	pop {r12}@mov r12, r7
.endm

	ldr r11, =0x000010e0 @ bios_memcpy
	mov r12, #0x03800000
	@ first, copy ourselves to the first 256b of IWRAM
	ldr r0, =nopsled
	sub r1, r12, #0x8000
	mov r2, #0x100
	call_bios_memcpy

	@ and then to the next consecutive 256b
	ldr r0, =nopsled
	sub r1, r12, #0x7f00
	mov r2, #0x100
	call_bios_memcpy

	@ now copy the next 512b to have 1k filled
	sub r0, r12, #0x8000
	sub r1, r12, #0x7e00
	mov r2, #0x0200
	call_bios_memcpy
	@ copy 1k -> 2k filled
	sub r0, r12, #0x8000
	sub r1, r12, #0x7c00
	mov r2, #0x0400
	call_bios_memcpy
	@ copy 2k -> 4k filled
	sub r0, r12, #0x8000
	sub r1, r12, #0x7800
	mov r2, #0x0800
	call_bios_memcpy
	@ copy 4k -> 8k filled
	sub r0, r12, #0x8000
	sub r1, r12, #0x7000
	mov r2, #0x1000
	call_bios_memcpy
	@ copy 8k -> 16k filled
	sub r0, r12, #0x8000
	sub r1, r12, #0x6000
	mov r2, #0x2000
	call_bios_memcpy
	@ copy 16k -> 32k filled
	sub r0, r12, #0x8000
	sub r1, r12, #0x4000
	mov r2, #0x4000
	call_bios_memcpy

	@ we now have filled EWRAM. we now copy EWRAM twice to fill IWRAM:
	sub r0, r12, #0x8000
	add r1, r12, #0x0000
	mov r2, #0x8000
	call_bios_memcpy
	sub r0, r12, #0x8000
	add r1, r12, #0x8000
	mov r2, #0x7C00 @ except the last 1k contains a "magic" area that causes stuff to reboot...
	call_bios_memcpy

	@ assuming our libnds entrypoint sets up NWRAM correctly, because I'm not doing that here
	ldr r10, =0x03040000 @ NWRAM-A
	add r9, r10,#0x40000 @ NWRAM-B
	add r8, r10,#0x80000 @ NWRAM-C

	@ we can now fill 256k NWRAM-A with 8 32k copies of EWRAM
	sub r0, r12, #0x8000
	add r1, r10, #0x00000 @ NWRAM-A + 0k
	mov r2, #0x8000
	call_bios_memcpy
	sub r0, r12, #0x8000
	add r1, r10, #0x08000 @ NWRAM-A + 32k
	mov r2, #0x8000
	call_bios_memcpy
	@ except we now copy the first 64k of NWRAM-A to the second 64k
	add r0, r10, #0x0000
	add r1, r10, #0x10000 @ NWRAM-A + 64k
	mov r2, #0x10000
	call_bios_memcpy
	@ except we now copy the first half of NWRAM-A to its second half
	add r0, r10, #0x0000
	add r1, r10, #0x20000 @ NWRAM-A + 128k
	mov r2, #0x20000
	call_bios_memcpy

	@ copy NWRAM-A to NWRAM-B
	mov r0, r10
	add r1, r10, #0x40000 @ NWRAM-B + 0k
	mov r2, #0x40000
	call_bios_memcpy
	@ and to NWRAM-C
	mov r0, r10
	add r1, r10, #0x80000 @ NWRAM-C + 0k
	mov r2, #0x40000
	call_bios_memcpy
	@ and to VRAM banks C/D
	mov r0, r10
	mov r1, #0x06000000
	mov r2, #0x40000
	call_bios_memcpy

	@ reinstall und. insn handlers
	ldr r8 , =0x0380ffdc
	ldr r9 , =0x03ffffdc
	ldr r10, =0x037fffdc
	@ldr r11, =undhandler_7
	sub r11, pc, #(.-undhandler_7)
	str r11, [r8 ]
	str r11, [r8 , #0xc]
	str r11, [r9 ]
	str r11, [r9 , #0xc]
	str r11, [r10]
	str r11, [r10, #0xc]

.Lskip_fill_nwram:
	@ we're set now!

	pop {r1}
	cmp  r1, #0

	bleq on_hang_c @ to do CAMLED signal stuff
	blne reboot_me @ reboot, and hope that the glitching happened correctly

1:	bal  1b
	.4byte 0xe7f000f0 @ undefined

.veneer_call_r11:
	bx r11

	.pool
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

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

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

	.section .text.nopsled, "ax", %progbits
	.arm
	.global nopsled
	@.balign 0x100 @ done in the linker script
nopsled: @ 0x04804100
nopsled_arm:
@	mov r0, #0x04000000
@	mov r1, #0xff
@	orr r0, #0x00004C00
@
@	strb r1, [r0, #1] @ DATADIR: set all pins to out
@	strb r1, [r0, #2] @ IEDGE: interrupt on rising edge
@	mvn  r1, r1
@	strb r1, [r0, #3] @ IE: interrupt enable (disable all)
@
@1:	mvn  r1, r1
@	strb r1, [r0] @ DATAOUT
@	b    1b

@ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @

@	mov r0, #0x04000000
@	orr r0, #0x00000300
@	orr r0, #0x00000004
@	mov r1, #3
@	str r1, [r0]
@
@	mov r0, #0x04800000
@	orr r0, #0x00004000
@	bx  r0

@ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @

	eors r0, r0          @ 000030e0                @ movs r0, r0 ; b +0x64 (ok)
	@ give these regs some
	@ convenient values
	eor  r0, #0x04000000 @ 010320e2@ lsls r1, r0, #0xc ; b +0x244 == +0x44 (ok)
	mov  r1, #3          @ 0310a0e3@ asrs r3, r0, #0x20; b +0x344 == +0x44 (ok)
	@ <= (le) is now true (see below)
	cmp  r1, r0          @ 000051e1      @ movs r0, r0 ; b +0x4a6 == +0xa6 (ok)

	@ initialize POWCNT2 wifi which turns on wifi RAM
	@ so that we can actually jump to our payload
	orr r0, #0x300       @ 030c80e3  @ lsrs r3, r0, #0x10 ; b +0x504 == +4 (ok)
	str r1, [r0, #4]     @ 041080e5  @ asrs r4, r0, #0x20 ; b -0x4fc == +4 (ok)

	@ r0 = 0x04804000 (entry), r0 so that we
	@ won't ever trigger the filler code
	eor  r0, r0          @ 000020e0      @ movs r0, r0 ; b +0x44           (ok)
	eor  r0, #0x04800000 @ 120520e2@lsls r2, r2, #0x14 ; b +0x444 == +0x44 (ok)
	eor  r0, #0x00004000 @ 010920e2  @ lsrs r1, r0, #4 ; b +0x444 == +0x44 (ok)

	@ doesn't do anything, but jumps
	@ over the illegal insn in thumb
	and  r0, r0          @ 000000e0      @ movs r0, r0 ; b +4              (ok)
	@ "le" so that our first 'cmp' had
	@ to be executed before doing anything
	bxle r0              @ 10ff2fd1                 @ illegal! ; bne +0x62 (!!)
nopsled_arm_end:

	@ this is a nop in both arm and thumb
	@ except we're using '0x03801e80' as fill value because it:
	@ 1. is a valid WRAM address, so it's useful for 'stack smashing' stuff with
	@    sp confusion caused by a hardware glitch. it's also aimed to the middle
	@    of the nopsled, so accidental stack writes hopefully won't botch anything
	@ 2. it's a practical nop in both ARM and Thumb:
	@    801e8003     orreq r1, r0, #0x80, #28
	@    801e         subs  r0, r0, #2
	@    8003         lsls  r0, r0, #0xe
	.fill (0x100 - (nopsled_arm_end-nopsled_arm) - (nopsled_thumb_end-nopsled_thumb))/4, 4, 0x03801e80

	.thumb
	.thumb_func
nopsled_thumb:
	@bx  pc     @ switch to ARM mode! (pc is currently at nopsled_thumb_end
	@mov r8, r8 @ which is where nopsled_arm is also thanks to the mirroring)

	@ except the above won't work in ARM mode, so let's do this instead:
	add r0, pc, #0     @ 00a0 @ andlo r10, r4, r0
	add r0, #4         @ 0430
	bx  r0             @ 0047 @ andeq r4, r0, r0, lsl #14
	.2byte 0 @ align   @ 0000
nopsled_thumb_end:

nopsled_end: @ 0x04804200

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

	.arm
	.section .text.mimic, "ax", %progbits
	.global   mimic_b9
mimic_b9:

	@ indirection also seen in the real bootrom
	b 1f
1:	b 1f
1:

	push {r8,r9,r12,lr}
	mov   r9, sp
	mrs   r8, cpsr

	@ 3DS here does a RESET11 release here, let's OR SCFG_EXT here instead
	ldr r3, =0x04004008
	ldr r1, [r3]
	orr r1, #(1<<31)
	str r1, [r3]

	@ set up CPU status
	mov r0, #0xd3
	msr cpsr_fsxc, r0
	ldr sp, =0x03800000
	mov r0, #0xd2
	msr cpsr_fsxc, r0
	ldr r0, =0x037fff00
	mov sp, r0
	mov r0, #0xdf
	msr cpsr_fsxc, r0
	ldr r0, =0x037ffb00
	mov sp, r0

	@ normally, a call to mpu_init is done here, but we can't really do that on
	@ an arm7tdmi... so let's skip that instead (A7 bootrom won't do it either)

	bl setup_ram_vectors

.Lretme:
	msr  cpsr, r8
	mov  sp, r9
	pop {r8,r9,r12,lr}
	bx   lr

setup_ram_vectors:
	@@add  r0, pc, #0x34
	adr  r0, .Lb9datatabmid
	ldr  r0, [r0]
	@add  r2, pc, #0x1C
	adr  r2, .Lb9datatab
	mov  r3, #0x08000000 @ TODO @ actually, let's just let it do some noops (GBA ROM memory -> open-bus)
	@add r12, r3, #0x30
	add r12, r3, #(.Lb9datatabend - .Lb9datatab)

1:	ldr   r1, [r2], #4
	stmia r3!, {r0, r1}
	cmp   r3, r12
	blt   1b
	bx    lr

	@ TODO
.Lb9datatab:
	.4byte 0xffff0078 @ 00 @ loop to self
	.4byte 0xffff0078 @ 04
	.4byte 0xffff00dc @ 08 @ called by FIQ
	.4byte 0xffff0078 @ 0C
	.4byte 0xffff00dc @ 10 @ called by SWI
	.4byte 0xffff0080 @ 14 @ loop to self
	.4byte 0xffff0078 @ 18 @ called by UND
.Lb9datatabmid:
	.4byte 0xe51ff004 @ 1C @ ldr pc, =0xffff0078
	.4byte 0xffff0078 @ 20 @ called by PFETCH ABRT
	.4byte 0xe51ff004 @ 24 @ ldr pc, =0xffff0080
	.4byte 0xffff0080 @ 28 @ called by DATA ABRT
	.4byte 0xe3a0f302 @ 2C @ called by IRQ @ mov pc, #0x08000000
.Lb9datatabend:

	.global mimic_b9_end
mimic_b9_end:


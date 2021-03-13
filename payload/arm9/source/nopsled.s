@ vim: set ft=armv4 noet:

/*
 * memory regions available:
 * ITCM: clearedon A9 RST???
 * DTCM: addr unclear...
 * MRAM: disabled...
 * WRAM: how is this mapped????
 * VRAM: how is this mapped????
 *NWRAM: how is this mapped?????????
 *
 * OAM, CGRAM?????? (lol, but actually feasible ig?)
 */

@ 0x0800 bytes
@ jump to: 0x022ffffc

.arm

#define START_ORG (0x0000)
#define END_ORG   (0x0100)

/*#define BR_TAR (0x03fffffc)
#define BR_TAR_D (4)*/
/*#define BR_TAR (0x022ffffc)
#define BR_TAR_D (4)*/

@ == 0x01002000 (== 0x00002000) == 1/4th of ITCM
/*#define BR_TAR   (0x01ffa000)
#define BR_TAR_D (0x00006000)*/

/*#define BR_TAR (0x00002000)
#define BR_TAR_D (0)*/

@ OAM! because why not!
#define BR_TAR (0x07000080)
#define BR_TAR_D (-0x80)

#define BR_TAR_S (BR_TAR+BR_TAR_D)

_start:
	eors r0, r0          @ 000030e0 @ movs r0, r0       ; b .+0x064
	eor  r0, #(BR_TAR_S) @ 070420e2 @ lsls r7, r0, #0x10; b .+0x444 == .+0x044
#if BR_TAR_D < 0
	add  r0, #(-BR_TAR_D)@ 800080e2 @ something similarly uninteresting
#elif BR_TAR_D > 0
	sub  r0, #(BR_TAR_D) @ 060a40e2 @ lsls r6, r0, #0x08; b .+0x482 == .+0x082
#endif
	@ doesn't do anything, but jumps over the illegal insn in thumb
	and  r0, r0          @ 000000e0 @ movs r0, r0       ; b .+0x004
	mov  pc, r0          @ 00f0a0e1 @ invalid           ; b .+0x342 == .+0x042
_start_end:

.fill (END_ORG - (_start_end-_start) - (_end_end-_end)), 1, 0

.thumb
.thumb_func
_end:
	add  r0, pc, #0 @ 00a0 @ andlo r10, r3, r0
	add  r0, #3     @ 0330
	bx   r0         @ 0047 @ andeq r4, r0, r0, lsl #14
	.2byte 0        @ 0000 @ align
_end_end:

@ wraparound...


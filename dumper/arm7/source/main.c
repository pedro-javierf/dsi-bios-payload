/*---------------------------------------------------------------------------------

	default ARM7 core

		Copyright (C) 2005 - 2010
		Michael Noland (joat)
		Jason Rogers (dovoto)
		Dave Murphy (WinterMute)

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any
	damages arising from the use of this software.

	Permission is granted to anyone to use this software for any
	purpose, including commercial applications, and to alter it and
	redistribute it freely, subject to the following restrictions:

	1.	The origin of this software must not be misrepresented; you
		must not claim that you wrote the original software. If you use
		this software in a product, an acknowledgment in the product
		documentation would be appreciated but is not required.

	2.	Altered source versions must be plainly marked as such, and
		must not be misrepresented as being the original software.

	3.	This notice may not be removed or altered from any source
		distribution.

---------------------------------------------------------------------------------*/
#include <nds.h>
/*#include <dswifi7.h>
#include <maxmod7.h>*/

#define MAINRAM_MAGIC_ADDRESS (*(volatile uint32_t*)0x02069420)

#include "payload7_bin.h"
#define PAYLOAD7_ORG (0x04804000)
#define PAYLOAD7_RAM ((volatile uint8_t*)PAYLOAD7_ORG)
#define PAYLOAD7_RAM8 ((volatile uint8_t*)PAYLOAD7_ORG)
#define PAYLOAD7_RAM16 ((volatile uint16_t*)PAYLOAD7_ORG)
#define PAYLOAD7_RAM32 ((volatile uint32_t*)PAYLOAD7_ORG)

typedef uint32_t (*wifiram_armcode_f)(uint32_t x);
typedef uint32_t (*wifiram_thumbcode_f)(uint32_t x);

inline static void call_payload(uint32_t v, int reboot) {
	void (*payload_fn)(uint32_t, int) = (void*)PAYLOAD7_ORG;
	payload_fn(v, reboot);
}

void VcountHandler() {
	inputGetAndSend();
}

volatile bool exitflag = false;

void powerButtonCB() {
	exitflag = true;
}

#define EXCEPTION_VECTOR  (*(VoidFn*)0x0380ffdc)
#define EXCEPTION_VECTOR2 (*(VoidFn*)0x03ffffdc)
#define EXCEPTION_VECTOR3 (*(VoidFn*)0x037fffdc)

#define REG_GPIO_DATA (*(volatile uint8_t*)0x04004c00)
#define REG_GPIO_DIR  (*(volatile uint8_t*)0x04004c01)
#define REG_GPIO_EDGE (*(volatile uint8_t*)0x04004c02)
#define REG_GPIO_IE   (*(volatile uint8_t*)0x04004c03)

#define SD_DATA_PORT_SEL (*(volatile uint16_t*)0x04004802)
#define SD_CARD_CLK_CTL  (*(volatile uint16_t*)0x04004824)
#define SD_SOFT_RESET    (*(volatile uint16_t*)0x040048E0)

extern void on_hang_c(void);
extern void undhandler_7(uint32_t magic, int lock);

static uint16_t memtest_control_ram16[0x400];
static uint32_t memtest_control_ram32[0x200];

static void wait_vbl_spin(void) {
	while (REG_VCOUNT != 160) ;
	while (REG_VCOUNT == 160) ;
}

static void send_magic_signal(uint32_t v) {
	MAINRAM_MAGIC_ADDRESS = v;
	while (MAINRAM_MAGIC_ADDRESS != 0) ;//swiWaitForVBlank();
}
//static void memtest_wifiram(void) {
//	// wifi RAM doesn't get single byte writes
//
//	// pattern test: u16, u32
//	for (size_t i = 0; i < (0x800>>1); ++i) PAYLOAD7_RAM16[i] = 0xa5c3;
//	for (size_t i = 0; i < (0x800>>1); ++i) {
//		if (PAYLOAD7_RAM16[i] != 0xa5c3) send_magic_signal(0x69080000 | (i<<1));
//	}
//	send_magic_signal(0x69000000);
//	for (size_t i = 0; i < (0x800>>1); ++i) PAYLOAD7_RAM16[i] = 0x5a3c;
//	for (size_t i = 0; i < (0x800>>1); ++i) {
//		if (PAYLOAD7_RAM16[i] != 0x5a3c) send_magic_signal(0x69180000 | (i<<1));
//	}
//	send_magic_signal(0x69100000);
//
//	for (size_t i = 0; i < (0x800>>2); ++i) PAYLOAD7_RAM32[i] = 0xaa55cc33;
//	for (size_t i = 0; i < (0x800>>2); ++i) {
//		if (PAYLOAD7_RAM32[i] != 0xaa55cc33) send_magic_signal(0x69280000 | (i<<2));
//	}
//	send_magic_signal(0x69200000);
//	for (size_t i = 0; i < (0x800>>2); ++i) PAYLOAD7_RAM32[i] = 0x55aa33cc;
//	for (size_t i = 0; i < (0x800>>2); ++i) {
//		if (PAYLOAD7_RAM32[i] != 0x55aa33cc) send_magic_signal(0x69380000 | (i<<1));
//	}
//	send_magic_signal(0x69300000);
//
//	// self-address test: u16, u32
//	for (size_t i = 0; i < (0x800>>1); ++i) PAYLOAD7_RAM16[i] = (uint16_t)&PAYLOAD7_RAM16[i];
//	for (size_t i = 0; i < (0x800>>1); ++i) {
//		if (PAYLOAD7_RAM16[i] != (uint16_t)&PAYLOAD7_RAM16[i]) send_magic_signal(0x69480000 | (i<<1));
//	}
//	send_magic_signal(0x69400000);
//
//	for (size_t i = 0; i < (0x800>>2); ++i) PAYLOAD7_RAM32[i] = (uint32_t)&PAYLOAD7_RAM32[i];
//	for (size_t i = 0; i < (0x800>>2); ++i) {
//		if (PAYLOAD7_RAM32[i] != (uint32_t)&PAYLOAD7_RAM32[i]) send_magic_signal(0x69580000 | (i<<2));
//	}
//	send_magic_signal(0x69500000);
//
//	// moving inversion: write pattern, check & inv increasing, check decreasing
//	// also bios crc16 test while we're at it
//	for (size_t i = 0; i < (0x800>>1); ++i) {
//		PAYLOAD7_RAM16[i] = 0xa5c3 ^ i;
//		memtest_control_ram16[i] = 0xa5c3 ^ i;
//	}
//	for (size_t i = 0; i < (0x800>>1); ++i) {
//		if (PAYLOAD7_RAM16[i] != (0xa5c3^i)) send_magic_signal(0x69680000 | (i<<1));
//
//		PAYLOAD7_RAM16[i] = ~(0xa5c3^i);
//	}
//	for (size_t i = (0x800>>1); i > 0; --i) {
//		if (PAYLOAD7_RAM16[i-1] != (uint16_t)~(0xa5c3^(i-1))) send_magic_signal(0x696c0000 | ((i-1)<<1));
//
//		PAYLOAD7_RAM16[i-1] = (0xa5c3^(i-1));
//	}
//	if (swiCRC16(0x1337, memtest_control_ram16, 0x800) !=
//	    swiCRC16(0x1337, PAYLOAD7_RAM16       , 0x800))
//		send_magic_signal(0x696fffff);
//	else
//		send_magic_signal(0x69600000);
//
//	for (size_t i = 0; i < (0x800>>2); ++i) {
//		PAYLOAD7_RAM32[i] = 0xaa55cc33 ^ (i | (i<<16));
//		memtest_control_ram32[i] = 0xaa55cc33 ^ (i | (i<<16));
//	}
//	for (size_t i = 0; i < (0x800>>2); ++i) {
//		if (PAYLOAD7_RAM32[i] != (0xaa55cc33^(i|(i<<16)))) send_magic_signal(0x69780000 | (i<<2));
//
//		PAYLOAD7_RAM32[i] = ~(0xaa55cc33^(i|(i<<16)));
//	}
//	for (size_t i = (0x800>>2); i > 0; --i) {
//		if (PAYLOAD7_RAM32[i-1] != ~(0xaa55cc33^((i-1)|((i-1)<<16)))) send_magic_signal(0x697c0000 | ((i-1)<<2));
//
//		PAYLOAD7_RAM32[i-1] = (0xaa55cc33^((i-1)|((i-1)<<16)));
//	}
//	if (swiCRC16(0x1337, memtest_control_ram32, 0x800) !=
//	    swiCRC16(0x1337, PAYLOAD7_RAM32       , 0x800))
//		send_magic_signal(0x697fffff);
//	else
//		send_magic_signal(0x69700000);
//
//	wifiram_armcode_f fntestarm = (wifiram_armcode_f)PAYLOAD7_ORG;
//	wifiram_thumbcode_f fntestthb = (wifiram_thumbcode_f)(PAYLOAD7_ORG+1);
//
//	// can we run code on wifi ram? (ARM, THUMB)
//	for (size_t i = 0; i < (0x800>>2); ++i) PAYLOAD7_RAM32[i] = 0xe2800001; // add r0, #1
//	PAYLOAD7_RAM32[(0x800>>2)-1] = 0xe12fff1e; // bx lr
//	if (fntestarm(0x69420) == (0x69420+((0x800>>2)-1)))
//		send_magic_signal(0x69800000);
//	else
//		send_magic_signal(0x698fffff);
//
//	for (size_t i = 0; i < (0x800>>1); ++i) PAYLOAD7_RAM16[i] = 0x1c40; // adds r0, #1
//	PAYLOAD7_RAM16[(0x800>>1)-1] = 0x4770; // bx lr
//	if (fntestthb(0x69420) == (0x69420+((0x800>>1)-1)))
//		send_magic_signal(0x69900000);
//	else
//		send_magic_signal(0x699fffff);
//
//	// decay/bit fade test: fill ones, sleep, check if changed;; fill zeros, sleep, check if changed
//	for (size_t i = 0; i < (0x800>>1); ++i) PAYLOAD7_RAM16[i] = 0xffff;
//	for (size_t i = 0; i < 60/*frames*/*60/*seconds*/*30/*minutes*/; ++i) wait_vbl_spin();
//	for (size_t i = 0; i < (0x800>>1); ++i) {
//		if (PAYLOAD7_RAM16[i] != 0xffff) send_magic_signal(0x69a80000 | (i<<1));
//	}
//	send_magic_signal(0x69a00000);
//
//	for (size_t i = 0; i < (0x800>>1); ++i) PAYLOAD7_RAM16[i] = 0x0000;
//	for (size_t i = 0; i < 60/*frames*/*60/*seconds*/*30/*minutes*/; ++i) wait_vbl_spin();
//	for (size_t i = 0; i < (0x800>>1); ++i) {
//		if (PAYLOAD7_RAM16[i] != 0x0000) send_magic_signal(0x69b80000 | (i<<1));
//	}
//	send_magic_signal(0x69b00000);
//
//	for (size_t i = 0; i < (0x800>>1); ++i) PAYLOAD7_RAM16[i] = 0xa5c3^i;
//	for (size_t i = 0; i < 60/*frames*/*60/*seconds*/*30/*minutes*/; ++i) wait_vbl_spin();
//	for (size_t i = 0; i < (0x800>>1); ++i) {
//		if (PAYLOAD7_RAM16[i] != (0xa5c3^i)) send_magic_signal(0x69c80000 | (i<<1));
//	}
//	send_magic_signal(0x69c00000);
//
//	for (size_t i = 0; i < (0x800>>1); ++i) PAYLOAD7_RAM16[i] = ~(0xa5c3^i);
//	for (size_t i = 0; i < 60/*frames*/*60/*seconds*/*30/*minutes*/; ++i) wait_vbl_spin();
//	for (size_t i = 0; i < (0x800>>1); ++i) {
//		if (PAYLOAD7_RAM16[i] != (uint16_t)~(0xa5c3^i)) send_magic_signal(0x69d80000 | (i<<1));
//	}
//	send_magic_signal(0x69d00000);
//}

int main() {
	// clear sound registers
	dmaFillWords(0, (void*)0x04000400, 0x100);

	REG_SOUNDCNT |= SOUND_ENABLE;
	writePowerManagement(PM_CONTROL_REG, ( readPowerManagement(PM_CONTROL_REG) & ~PM_SOUND_MUTE ) | PM_SOUND_AMP );
	powerOn(POWER_SOUND|(1<<1));

	readUserSettings();
	ledBlink(0);
	i2cWriteRegister(0x4A/*BPTWL*/, 0x31/*cam led*/, 0/*off*/);

	i2cWriteRegister(0x4A/*BPTWL*/, 0x21/*unused*/, 'N');
	i2cWriteRegister(0x4A/*BPTWL*/, 0x21/*unused*/, 'O');
	i2cWriteRegister(0x4A/*BPTWL*/, 0x21/*unused*/, 'P');
	i2cWriteRegister(0x4A/*BPTWL*/, 0x21/*unused*/, 'E');

	irqInit();
	// Start the RTC tracking IRQ
	initClockIRQ();
	fifoInit();
	//touchInit();

	SetYtrigger(80);

	//installWifiFIFO();
	//installSoundFIFO();

	installSystemFIFO();

	irqSet(IRQ_VCOUNT, VcountHandler);

	irqEnable( IRQ_VBLANK | IRQ_VCOUNT);

	setPowerButtonCB(powerButtonCB);

	// * DISABLE NAND CLK
	SD_DATA_PORT_SEL |= 1; // select NAND
	asm volatile("nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop\n");
	SD_SOFT_RESET = 0; // reset stuff
	asm volatile("nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop\n");
	SD_CARD_CLK_CTL = 0; // disable SD11_CLK
	// * POWER OFF NAND (SDIO)
	REG_SCFG_CLK &= ~(1<<0); // disable SD/SDIO controller
	REG_SCFG_EXT &= ~(1<<18); // *extremely* disable SD/MMC

	// clear GPIOs
	// NOTE: HARDWARE RESET SETS GPIO HIGH!!!!
	REG_GPIO_EDGE = 0xff;
	REG_GPIO_IE   = 0;
	REG_GPIO_DIR = 7|(1<<4)|(1<<7);
	REG_GPIO_DATA = 1<<4;

	REG_IME=0;
	REG_MBK9 = 0; // allow A9 to map stuff
	REG_MBK6 = (0x04<<4)|(3<<12)|(0x08<<20); // 256k A from 0x0304..0 to 0x0308..0
	REG_MBK7 = (0x10<<3)|(3<<12)|(0x18<<19); // 256k B from 0x0308..0 to 0x030C..0
	REG_MBK8 = (0x18<<3)|(3<<12)|(0x20<<19); // 256k C from 0x030C..0 to 0x0310..0
	// signal that we want stuff from the A9 now
	send_magic_signal(0x1337);

	// wait for A9 to set up regs correctly
	while (MAINRAM_MAGIC_ADDRESS != 0xACAB) ;
	MAINRAM_MAGIC_ADDRESS = 0;//x8ACAB;

	//memtest_wifiram();

	// install payload
	size_t payload_len = payload7_bin_size;
	if (payload_len & 3) payload_len += (4 - (payload_len & 3));
	const uint32_t* payload7_32 = (const uint32_t*)(void*)payload7_bin;
	for (size_t i = 0; i < payload_len/4; ++i) {
		PAYLOAD7_RAM32[i] = payload7_32[i];
	}

	// sanity check
	for (size_t i = 0; i < payload_len/4; ++i) {
		if (PAYLOAD7_RAM32[i] != payload7_32[i]) {
			send_magic_signal(0xACAB0000|(i*4));
			for(;;);
			//goto NOPE;
		}
	}
	send_magic_signal(0xACAB+1);
NOPE:;

	// set up UND exception vector
	// we set it to a random place in IWRAM, which will hopefully be
	// untouched by the bootrom before a glitch happens, so that the und
	// handler will land into our nopsled, which then enabled wifi RAM and
	// jumps to the actual payload code
	EXCEPTION_VECTOR3 = EXCEPTION_VECTOR2 = EXCEPTION_VECTOR =
		(VoidFn*)
			PAYLOAD7_ORG
			/*0x0380ae00*/
			//undhandler_7
			/*0x037fff00*/
			;

	REG_IME=0;
	call_payload(0x214e5454, -1); // magic number to fill all RAM, and reboot/do dumper code (hopefully)
	//call_payload(0x214e5454, 0); // magic number to fill all RAM, and hang (on_hang_c) (used for testing the glitching setup)

	for (;;) ;
	return -1;
}


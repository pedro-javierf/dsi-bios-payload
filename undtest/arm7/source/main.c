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

#include "payload7_bin.h"
#define PAYLOAD7_ORG (/*0x0380c000*/0x04804000)
#define PAYLOAD7_RAM ((volatile uint8_t*)PAYLOAD7_ORG)
#define PAYLOAD7_RAM32 ((volatile uint32_t*)PAYLOAD7_ORG)
#define WIFI_RAM ((volatile uint32_t*)0x04804000)

inline static void call_payload(uint32_t v, int reboot) {
	void (*payload_fn)(uint32_t, int) = (void*)PAYLOAD7_ORG;
	payload_fn(v, reboot);
}

// undefined insn:
// .2byte 0x    deff 'udf 0xff'
// .4byte 0xe7f000f0 'udf 0'

// TODO: INSTALL PAYLOAD!

void VblankHandler(void) {
	//Wifi_Update();
}


void VcountHandler() {
	inputGetAndSend();
}

volatile bool exitflag = false;

void powerButtonCB() {
	exitflag = true;
}

__attribute__((__target__("arm"  ), __noreturn__, __noinline__))
static void und_a() {
	asm volatile(".4byte 0xe7f000f0"); __builtin_unreachable();
}
__attribute__((__target__("thumb"), __noreturn__, __noinline__))
static void und_t() {
	asm volatile(".2byte 0xdeff"); __builtin_unreachable();
}

#define MAINRAM_MAGIC_ADDRESS (*(volatile uint32_t*)0x02069420)

__attribute__((__target__("arm"), __noreturn__, __noinline__))
static void undhandler_7(void) {
	MAINRAM_MAGIC_ADDRESS = 0xACAB;

	// TODO: anything else locks up the console. probably need some way to
	//       properly context-switch etc. while fixing a stack & other stuff
	for (;;) {
		//i2cWriteRegister(0x4A/*BPTWL*/, 0x31/*cam led*/, 0x01/*on*/);
		//swiDelay(0x1000000);

		/*for (int iii = 0; iii < 8; ++iii) {
			// i2cSelectDevice
			while (REG_I2CCNT & 0x80) ; //i2cWaitBusy();
			REG_I2CDATA = 0x4A;
			REG_I2CCNT  = 0xC2;
			while (REG_I2CCNT & 0x80) ; //i2cWaitBusy();
			if ((REG_I2CCNT >> 4) & 0x01) {
				// i2cSelectRegister
				for (int j = 0; j < 0x180; ++j) ; // i2cDelay
				REG_I2CDATA = 0x31;
				REG_I2CCNT  = 0xC0;
				while (REG_I2CCNT & 0x80) ; //i2cWaitBusy();
				if ((REG_I2CCNT >> 4) & 0x01) {
					for (int j = 0; j < 0x180; ++j) ; // i2cDelay
					REG_I2CDATA = 0x01;
					// i2cStop(0)
					REG_I2CCNT = 0xC0;
					for (int j = 0; j < 0x180; ++j) ; // i2cDelay
					REG_I2CCNT = 0xC5;

					while (REG_I2CCNT & 0x80) ; //i2cWaitBusy();
					if ((REG_I2CCNT >> 4) & 0x01) {
						continue; // aieee!
					}
				}
			}
			REG_I2CCNT = 0xC5;
		}

		for (int i = 0; i < 0x1000000; ++i) ;*/
	}
}

#define EXCEPTION_VECTOR  (*(VoidFn*)0x0380ffdc)
#define EXCEPTION_VECTOR2 (*(VoidFn*)0x03ffffdc)
#define EXCEPTION_VECTOR3 (*(VoidFn*)0x037fffdc)

int main() {
	// clear sound registers
	dmaFillWords(0, (void*)0x04000400, 0x100);

	REG_SOUNDCNT |= SOUND_ENABLE;
	writePowerManagement(PM_CONTROL_REG, ( readPowerManagement(PM_CONTROL_REG) & ~PM_SOUND_MUTE ) | PM_SOUND_AMP );
	powerOn(POWER_SOUND | (1<<1/*wifi*/));

	readUserSettings();
	ledBlink(0);
	int led = 0;
	i2cWriteRegister(0x4A/*BPTWL*/, 0x31/*cam led*/, led);

	irqInit();
	// Start the RTC tracking IRQ
	initClockIRQ();
	fifoInit();
	//touchInit();

	//mmInstall(FIFO_MAXMOD);

	/*WIFI_RAM[0] = 0x214e5454;
	for (int i = 1; i < 16; ++i) WIFI_RAM[i] = i ^ 0x13370;
	if (WIFI_RAM[0] == 0x214e5454 && WIFI_RAM[1] == 0x13371 && WIFI_RAM[15]==0x1337f)
		MAINRAM_MAGIC_ADDRESS = 0xACAB;*/

	SetYtrigger(80);

	//installWifiFIFO();
	//installSoundFIFO();

	installSystemFIFO();

	irqSet(IRQ_VCOUNT, VcountHandler);
	//irqSet(IRQ_VBLANK, VblankHandler);

	irqEnable( IRQ_VBLANK | IRQ_VCOUNT | IRQ_NETWORK);

	setPowerButtonCB(powerButtonCB);

	size_t payload_len = payload7_bin_size;
	if (payload_len & 3) payload_len += (4 - (payload_len & 3));
	const uint32_t* payload7_32 = (const uint32_t*)(void*)payload7_bin;

	void (*bios_memcpy)(const void* src, volatile void* dst, size_t len) = 0x000010e0;
	bios_memcpy(payload7_bin, PAYLOAD7_RAM32, payload7_bin_size);
	/*for (size_t i = 0; i < payload_len/4; ++i) {
		PAYLOAD7_RAM32[i] = payload7_32[i];
	}*/
	EXCEPTION_VECTOR3 = EXCEPTION_VECTOR2 = EXCEPTION_VECTOR = /*undhandler_7;// */PAYLOAD7_ORG;
	for (size_t i = 0; i < payload_len/4; ++i) {
		if (PAYLOAD7_RAM32[i] != payload7_32[i]) {
			MAINRAM_MAGIC_ADDRESS = 0xACAB0000|(i*4);
			goto NOPE;
		}
	}

	MAINRAM_MAGIC_ADDRESS = 0xACAB;
NOPE:;

	// Keep the ARM7 mostly idle
	uint32_t old = 0;
	while (!exitflag) {
		uint32_t keys = ~((uint32_t)REG_KEYINPUT | ((uint32_t)REG_KEYXY << 10));
		uint32_t diff = keys & ~old;

		if (keys & (KEY_SELECT | KEY_START)) {
			exitflag = true;
		}

		if (keys & KEY_B) call_payload(0x1337, 69); // call payload as test
		/*if (keys & KEY_A) call_payload(0x214e5454, 0); // fill all RAM, hang
		if (keys & KEY_X) call_payload(0x214e5454,-1); // fill all RAM, reboot*/
		if (keys & KEY_R) und_t();

		if (diff & KEY_Y) {
			led = 1-led;
			i2cWriteRegister(0x4A/*BPTWL*/, 0x31/*cam led*/, led);
			MAINRAM_MAGIC_ADDRESS = 0xACAB;
		}

		swiWaitForVBlank();
		old = keys;
	}
	return 0;
}


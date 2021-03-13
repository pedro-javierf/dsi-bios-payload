
#include <nds.h>
#include <stdio.h>

#include "payload9_bin.h"
#define PAYLOAD9_ORG (0x02200000)
#define PAYLOAD9_RAM ((volatile uint8_t*)PAYLOAD9_ORG)

#define MAINRAM_MAGIC_ADDRESS (*(volatile uint32_t*)0x02069420)

__attribute__((__target__("arm"  ), __noreturn__, __noinline__))
static void und_a() {
	asm volatile(".4byte 0xe7f000f0"); __builtin_unreachable();
}
__attribute__((__target__("thumb"), __noreturn__, __noinline__))
static void und_t() {
	asm volatile(".2byte 0xdeff"); __builtin_unreachable();
}

__attribute__((__target__("thumb"), __noreturn__, __noinline__))
static void undhandler_9(void) {
	for (int i = 0; ; ++i)
		*(volatile uint16_t*)BG_PALETTE = i|0x8000;
}

__attribute__((__target__("arm"  ), , __noinline__, __section__(".itcm.text.main.c$memtest")))
static int memtest(void) {
	int ret = 0;

	uint32_t xx = 0xACAB;
	MAINRAM_MAGIC_ADDRESS = xx;
	/*DC_FlushAll();
	DC_InvalidateAll();*/
	uint32_t yy = MAINRAM_MAGIC_ADDRESS;

	if (xx == yy) ret |= 1;

	xx = 0x1337;
	MAINRAM_MAGIC_ADDRESS = xx;
	/*DC_FlushAll();
	DC_InvalidateAll();*/
	yy = MAINRAM_MAGIC_ADDRESS;

	if (xx == yy) ret |= 2;

	return ret;
}

__attribute__((__target__("arm"), __noinline__, __section__(".itcm.text.main.c$inhibit_caching")))
static void inhibit_caching() {
	DC_FlushAll();
	DC_InvalidateAll();
	IC_InvalidateAll();

	uint32_t cp15_enflags
	=	(0<<19) // ITCM load mode
	|	(1<<18) // ITCM enable
	|	(0<<17) // DTCM load mode
	|	(1<<16) // DTCM enable
	|	(0<<15) // thumb modeswitch disable (0=>enabled)
	|	(0<<14) // round robin replacement (0=>pseudorandom)
	|	(1<<13) // alt. vector (0xffff0000 IVT) (needed for Bios9i) (0=>0x00000000)
	|	(0<<12) // icache enable (0: disabled)
	|	(0<< 7) // endianness (0: little, 1: big)
	|	(0<< 2) // dcache enable (0: disabled)
	|	(0<< 0) // PU enable (0: disabled)
	;
	asm volatile("mcr p15, 0, %[cp15flags], c1, c0, 0\n"
			:
			:[cp15flags]"r"(cp15_enflags));
}

__attribute__((__target__("arm"  ), __noreturn__, __noinline__, __section__(".itcm.text.main.c$exmemtest")))
static void exmemtest(void) {
	BG_PALETTE[0] = 0x8000|(0x1f<<5); // green ('hello we are alive here at least')

	REG_IME = 0;

	uint16_t col = 0x801f;

	if (!(memtest() & 1)) {
		col = 0x8000|(0x1f<<10); // blue
		goto END;
	}

	REG_EXMEMCNT = REG_EXMEMCNT & ~(1<<13);

	for (int i = 0; i < 0x10000; ++i) asm volatile("nop"); // wait a bit

	int r = memtest();

	if (r & 1) col |= (0x1f<< 5); // green => yellow
	if (r & 2) col |= (0x1f<<10); // blue  => purple (or white)

	if (REG_EXMEMCNT & (1<<13)) col = 0x8000|(0x1f<<5)|(0x1f<<10); // wtf? -> cyan

END:
	*BG_PALETTE = col;
	for (;;) asm volatile("nop");
}

int main(void) {
	inhibit_caching();

	int oldKeys = 0;

	consoleDemoInit(); // videoSetModeSub(MODE_0_2D); vramSetBankC(VRAM_C_SUB_BG);

	REG_DISPCNT = (REG_DISPCNT | (1<<16)/*normal bg/obj display mode*/)
		& ~((1<<7/*forced blank*/)|(0xff00/*disable bgs, obj, wins*/));

	iprintf("hello\n");

	/*size_t plsz = ((size_t)payload9_bin_end - (size_t)payload9_bin);
	for (size_t i = 0; i < plsz; ++i) {
		PAYLOAD9_RAM[i] = payload9_bin[i];
	}*/
	/*EXCEPTION_VECTOR = PAYLOAD9_ORG;
	MAINRAM_MAGIC_ADDRESS = 0;*/
	/*DC_FlushRange(PAYLOAD9_RAM, plsz);
	IC_InvalidateRange(PAYLOAD9_RAM, plsz);*/

	iprintf("world\n");

	while (1) {
		swiWaitForVBlank();
		scanKeys();

		//DC_InvalidateRange(&MAINRAM_MAGIC_ADDRESS, 4);//All();//
		uint32_t val = *(volatile uint32_t*)memUncached(&MAINRAM_MAGIC_ADDRESS);
		if (val == 0xACAB) {
			iprintf("ACAB!\n");
			*(volatile uint32_t*)memUncached(&MAINRAM_MAGIC_ADDRESS) = 0;
			//DC_FlushRange(&MAINRAM_MAGIC_ADDRESS, 4);//All();
		} else if ((val&0xFFFF0000) == 0xACAB0000) {
			iprintf("-> 0x%08lx\n", val);
			*(volatile uint32_t*)memUncached(&MAINRAM_MAGIC_ADDRESS) = 0;
		}

		int keys = keysDown();

		int diff = keys & ~oldKeys;

		if (keys & KEY_START) break;
		//if (keys & KEY_A) /*exmemtest();*/ und_t(); // NO. not even useful
		if (diff & KEY_Y) *BG_PALETTE = ~*BG_PALETTE;

		oldKeys = keys;
	}

	return 0;
}


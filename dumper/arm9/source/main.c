
#include <nds.h>
#include <stdio.h>

#define MAINRAM_MAGIC_ADDRESS ((volatile uint32_t*)memUncached((void*)0x02069420))

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

int main(void) {
	//consoleDemoInit(); // videoSetModeSub(MODE_0_2D); vramSetBankC(VRAM_C_SUB_BG);
	videoSetMode(MODE_0_2D);
	vramSetBankA(VRAM_A_MAIN_BG);
	PrintConsole* def = consoleGetDefault();
	consoleInit(def, def->bgLayer, BgType_Text4bpp, BgSize_T_256x256, def->mapBase,
			def->gfxBase, true, true);
	iprintf("hi!\n");
	swiWaitForVBlank();
	swiWaitForVBlank();
	swiWaitForVBlank();

	volatile uint32_t* magic = (volatile uint32_t*)memUncached((void*)MAINRAM_MAGIC_ADDRESS);
	//inhibit_caching();

	*magic = 0;
	printf("done init handsh\n");

	REG_IME=0;
	for (int i = 0; i < 4; ++i) {
		REG_MBK1[i]=(1<<0)|(1<<8)|(i<<2);

		REG_MBK2[i]=(1<<0)|(1<<8)|(i<<2);
		REG_MBK3[i]=(1<<0)|(1<<8)|((i+4)<<2);

		REG_MBK4[i]=(1<<0)|(1<<8)|(i<<2);
		REG_MBK5[i]=(1<<0)|(1<<8)|((i+4)<<2);
	}
	WRAM_CR = 3; // all WRAM to A7

	VRAM_C_CR = VRAM_C_ARM7_0x06000000; // VRAM C,D to A7
	VRAM_D_CR = VRAM_D_ARM7_0x06020000;

	*magic = 0xACAB; // tell A7 the magic can happen
	//while (*magic != 0x8ACAB) swiWaitForVBlank();
	//iprintf("done final handsh\n");

	REG_IME = 1;
	for (;;) {
		uint32_t val = *magic;
		if (val == 0xACAB+1) {
			*magic = 0;
			iprintf("ready!\n");
		} else if (val) /*if ((val & 0xFF000000) == 0x69000000 || (val & 0xFFFF0000) == 0xACAB0000)*/ {
			*magic = 0;
			iprintf("-> 0x%08lx\n", val);
		}
		swiWaitForVBlank();
	}
}


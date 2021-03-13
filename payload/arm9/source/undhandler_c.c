
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define OUTBAK_VIDEO (1)
#define OUTBAK_CART  (2)

#define OUT_BACKEND (OUTBAK_VIDEO)

#define REG_POSTFLG (*(volatile uint8_t*)0x04000300)

#define REG_SCFG_A9ROM (*(volatile uint16_t*)0x04004000)
#define REG_SCFG_CLK   (*(volatile uint16_t*)0x04004004)
#define REG_SCFG_RST   (*(volatile uint16_t*)0x04004006)
#define REG_SCFG_EXT   (*(volatile uint32_t*)0x04004008)
#define REG_SCFG_MC    (*(volatile uint16_t*)0x04004010)

#define REG_DISPSTAT (*(volatile uint16_t*)0x04000004)
#define REG_VCOUNT   (*(volatile uint16_t*)0x04000006)

#define REG_IME (*(volatile uint32_t*)0x04000208)

#define REG_DMAxSAD(x)  (*(volatile uint32_t*)(0x040000b0 + 12*(x)))
#define REG_DMAxDAD(x)  (*(volatile uint32_t*)(0x040000b4 + 12*(x)))
#define REG_DMAxCNT(x)  (*(volatile uint32_t*)(0x040000b8 + 12*(x)))
#define REG_DMAxFILL(x) (*(volatile uint32_t*)(0x040000e0 +  4*(x)))

#define REG_NDMAGCNT      (*(volatile uint32_t*)0x04004100)
#define REG_NDMAxSAD(x)   (*(volatile uint32_t*)(0x04004104 + 0x1c*(x)))
#define REG_NDMAxDAD(x)   (*(volatile uint32_t*)(0x04004108 + 0x1c*(x)))
#define REG_NDMAxTCNT(x)  (*(volatile uint32_t*)(0x0400410c + 0x1c*(x)))
#define REG_NDMAxWCNT(x)  (*(volatile uint32_t*)(0x04004110 + 0x1c*(x)))
#define REG_NDMAxBCNT(x)  (*(volatile uint32_t*)(0x04004114 + 0x1c*(x)))
#define REG_NDMAxFDATA(x) (*(volatile uint32_t*)(0x04004118 + 0x1c*(x)))
#define REG_NDMAxCNT(x)   (*(volatile uint32_t*)(0x0400411c + 0x1c*(x)))

extern uint8_t __dumpspace[];
extern uint32_t regbak[18]; // r0-r12, sp, lr, pc, oldpc, oldcpsr
// +POSTFLG?

static void wait_one_vbl(void) {
	while (REG_VCOUNT >= 192) asm volatile("nop");
	while (REG_VCOUNT <  192) asm volatile("nop");
}

__attribute__((__target__("arm"), __noreturn__, __section__(".text.undhandler9_c")))
void undhandler9_c(void);

/** VIDEO ********************************************************************/

#define REG_POWCNT1       (*(volatile uint32_t*)0x04000304)
#define REG_MASTER_BRIGHT (*(volatile uint16_t*)0x0400006c)
#define REG_DISPCNT       (*(volatile uint32_t*)0x04000000)
#define REG_DISPCNT_SUB   (*(volatile uint32_t*)0x04001000)
#define REG_DISPCAPCNT    (*(volatile uint32_t*)0x04000064)
#define REG_BGCTRL        ( (volatile uint16_t*)0x04000008)
#define REG_BGCTRL_SUB    ( (volatile uint16_t*)0x04001008)

#define REG_VRAMSTAT      (*(volatile uint8_t *)0x04000240)
#define REG_VRAMCNT_A     (*(volatile uint8_t *)0x04000240)
#define REG_VRAMCNT_B     (*(volatile uint8_t *)0x04000241)
#define REG_VRAMCNT_C     (*(volatile uint8_t *)0x04000242)
#define REG_VRAMCNT_D     (*(volatile uint8_t *)0x04000243)

#define REG_BG_PALETTE     ((volatile uint16_t*)0x05000000)
#define REG_BG_PALETTE_SUB ((volatile uint16_t*)0x05000400)

#define BACKBUFFER_A ((volatile uint16_t*)0x06000000) // BG 3 main
#define BACKBUFFER_B ((volatile uint16_t*)0x06820000) // LCDC

__attribute__((__target__("arm"), __section__(".text.video_init")))
static void video_init(bool initvram) {
	// TODO: maybe use mode 2 display (direct VRAM display) instead??

	REG_VRAMCNT_A = (1<<0) | (0<<3) | (1<<7); // alloc A to PPU (main BG)
	REG_VRAMCNT_B = (0<<0) | (0<<3) | (1<<7); // alloc B to A9 LCDC
	REG_VRAMCNT_C = (2/*4*/<<0) | (0<<3) | (1<<7); // alloc C to A7 ~~sub BG~~
	REG_VRAMCNT_D = (2<<0) | (1<<3) | (1<<7); // alloc D to A7

	REG_POWCNT1 = 1 | (1<<1) | (1<<2) | (1<<3) | (1<<9) | (1<<15); // enable LCD, 2DA, 3Drender, 3Dgeom, 2DB, A on top
	if (initvram) {
		REG_DISPCNT = 3 | (0<<3) | (1<<16) | (1<<7) | (1<<11) | (1<<18); // BG3 only, fblank, mode 5 2D, normal PPU display | set bit11 to 2 for LCDC display, bit18/19 controls bank A..D
	} else {
		REG_DISPCNT = 3 | (0<<3) | (1<<16) | (1<<7) | (0<<11); // only fallback color, fblank, mode 5 2D, normal PPU display
	}
	REG_DISPCNT_SUB = 5 | (0<<3) | (1<<16) | (1<<7) | (0<<11); // no bg/obj/wnd, fblank, mode 5 2D, normal PPU display
	if (initvram) {
		//REG_BGCTRL[3] = 0 | (1<<14) | (0<<8) | (1<<2)|(1<<7)/*|(4<<16) :: libnds nonsense?*/; // highest prior, first screen base, direct color
		//REG_BGCTRL_SUB[3] = 0 | (1<<14) | (0<<8) | (1<<2)|(1<<7)/*|(4<<16) :: libnds nonsense?*/; // highest prior, first screen base, direct color
	}
	REG_DISPCAPCNT = 0; // no capture/blending/...

	// memset relevant VRAM
	// TODO: speed it up w/ stmia? DMA?
	if (initvram) {
		//wait_one_vbl();
		for (size_t y = 0; y < 256*256; ++y) {
			//BACKBUFFER_A[y]=0;
			BACKBUFFER_B[y]=0;
		}
	}
}
__attribute__((__target__("arm"), __noreturn__, __section__(".text.video_send")))
static void video_send(const void* payload, size_t len_bytes) {
#define MAG_X (2)
#define MAG_Y (2)
	const uint8_t* pl = payload;
	//goto FLASH;

	//wait_one_vbl();
	for (size_t i = 0; i < len_bytes; ++i) {
		uint8_t val = pl[i];

		for (size_t j = 0; j < 8; ++j) {
			uint16_t col = (val & (1<<j)) ? 0x801f : 0xffff; // 1->red, 0->white

			for (size_t y = 0; y < MAG_Y; ++y) {
				for (size_t x = 0; x < MAG_X; ++x) {
					//BACKBUFFER_A[256*(i*MAG_Y+y) + j*MAG_X+x] = col;
					BACKBUFFER_B[256*(i*MAG_Y+y) + j*MAG_X+x] = col;
				}
			}
		}
	}

FLASH:
	// no more fblank
	REG_DISPCNT = 3 | (0<<3) | (2<<16) | (0<<7) | (1<<11) | (1<<18); // BG3 only, no fblank, mode 5 2D, VRAM LCDC display
	REG_DISPCNT_SUB = 3 | (1<<16) | (0<<7) | (0<<11); // no bg/obj/wnd, no fblank, mode 5 2D, normal PPU display

	for (uint32_t i = 0; ; ++i) {
		//*REG_BG_PALETTE     = ( i)|0x8000;
		*REG_BG_PALETTE_SUB = (-i)|0x8000;
	}
	__builtin_unreachable();
}

/** NTR CART OUTPUT **********************************************************/

#define REG_EXMEMCNT   (*(volatile uint16_t*)0x04000204)
#define REG_AUXSPICNT  (*(volatile uint16_t*)0x040001a0)
#define REG_AUXSPIDATA (*(volatile uint16_t*)0x040001a2)
#define REG_ROMCTRL    (*(volatile uint32_t*)0x040001a4)
#define REG_CART_CMD   (*(volatile uint64_t*)0x040001a8)
#define REG_CART_DATA  (*(volatile uint32_t*)0x04100010)

__attribute__((__target__("arm"), __section__(".text.cart_init")))
static void cart_init(void) {
	REG_EXMEMCNT = REG_EXMEMCNT & ~(1<<11); // NTR cart slot to A9

	if (REG_SCFG_EXT & (1<<31)) { // if scfg is possible
		// power off cart | skippable?
		while ((REG_SCFG_MC & (3<<2)) == (3<<2)) ;
		if ((REG_SCFG_MC & (3<<2)) != (2<<2)) {
			REG_SCFG_MC = (REG_SCFG_MC & ~(3<<2)) | (3<<2);
			while ((REG_SCFG_MC & (3<<2)) != 0) ;
		}
		// power on cart
		while ((REG_SCFG_MC & (3<<2)) != (3<<2)) ;
		if ((REG_SCFG_MC & (3<<2)) == (0<<2)) {
			for (int i = 0; i < 100*1000* 1; ++i) asm volatile("nop"); // ~1 ms (hopefully)
			REG_SCFG_MC = (REG_SCFG_MC & ~(3<<2)) | (1<<2);
			for (int i = 0; i < 100*1000*10; ++i) asm volatile("nop"); // ~10 ms (hopefully)
			REG_SCFG_MC = (REG_SCFG_MC & ~(3<<2)) | (2<<2);
			for (int i = 0; i < 100*1000*27; ++i) asm volatile("nop"); // ~27 ms (hopefully)
			REG_ROMCTRL = 0x20000000|(1<<27);
			for (int i = 0; i < 100*1000*120; ++i) asm volatile("nop"); // ~120 ms (hopefully)
		}
	} else REG_ROMCTRL = 0x20000000|(1<<27);
}

__attribute__((__target__("arm"), __noreturn__, __section__(".text.cart_send")))
static void cart_send(const void* payload, size_t len_bytes) {
	const uint8_t* pl = payload;

	// won't hurt I guess, plus makes things more visible
	video_init(false);

	REG_AUXSPICNT = (3<<0) | (1<<6) | (1<<13) | (1<<15);
	REG_CART_CMD = 0x000000000000009FULL; // 'dummy' cmd // big-endian!

	for (size_t i = 0; i < len_bytes; ++i) {
		if (i == len_bytes - 1)
			REG_AUXSPICNT = (3<<0) | (0<<6) | (1<<13) | (1<<15); // 512 kbaud, serial, release chipsel, no irq, enable slot
		else if (i != 0)
			REG_AUXSPICNT = (3<<0) | (1<<6) | (1<<13) | (1<<15); // 512 kbaud, serial, keep chipsel, no irq, enable slot

		REG_CART_DATA = pl[i]; // init SPI "read" with a "dummy" value
		while (REG_AUXSPICNT & (1<<7)) ; // wait until xfer finished

		// flash something visible
		*REG_BG_PALETTE     = ~*REG_BG_PALETTE    ;
		*REG_BG_PALETTE_SUB = ~*REG_BG_PALETTE_SUB;
	}

	for (uint32_t i = 0; ; ++i) {
		*REG_BG_PALETTE     =   i |0x8000;
		*REG_BG_PALETTE_SUB = (-i)|0x8000;
	}
	__builtin_unreachable();
}

/** main *********************************************************************/

__attribute__((__section__(".data.payload")))
static uint8_t payload[] = {
	'H','e','l','l','o',' ','f','r','o','m',' ','A','R','M','9',0, // 16 bytes
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, // r12-r15
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, // oldpc, oldcpsr, postflg, scfg a9rom/clk
	0,0,0,0  // scfg ext
};

__attribute__((__target__("arm"), __noreturn__, __section__(".text.undhandler9_c")))
void undhandler9_c(void) {
	REG_IME = 0;
	REG_DMAxCNT(3) = REG_DMAxCNT(2) = REG_DMAxCNT(1) = REG_DMAxCNT(0) = 0;
	REG_NDMAxCNT(3) = REG_NDMAxCNT(2) = REG_NDMAxCNT(1) = REG_NDMAxCNT(0) = 0;

	uint32_t* pl = (uint32_t*)&payload[16];
	pl[0] = regbak[12];
	pl[1] = regbak[13];
	pl[2] = regbak[14];
	pl[3] = regbak[15];

	pl[4] = regbak[16];
	pl[5] = regbak[17];
	pl[6] = REG_POSTFLG;
	pl[7] = REG_SCFG_A9ROM | ((uint32_t)REG_SCFG_CLK << 16);

	pl[8] = REG_SCFG_EXT;

	// TODO: what can we use as output?
	// * NTR card dummy/header read commands? (piggyback 0-writes meant for reads)
	// * video output?
	// * cameras??? (needs A7 init...)

#if OUT_BACKEND == OUTBAK_CART
	cart_init();
	cart_send(payload, sizeof(payload));
#else /* OUTBAK_VIDEO: default */
	video_init(true);
	/*for (uint32_t i = 0; ; ++i) {
		*REG_BG_PALETTE     =  i |0x8000;
		*REG_BG_PALETTE_SUB =(-i)|0x8000;
	}*/
	video_send(payload, sizeof(payload));
#endif

	for (;;) ;
	__builtin_unreachable();
}



#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define OUTBAK_SPI    (1)
#define OUTBAK_I2C    (2)
#define OUTBAK_GPIO   (3)
#define OUTBAK_CAMLED (4)

#define OUT_BACKEND (OUTBAK_SPI)

#define BITBANG_PREAMBLE_LENGTH (0x10) // periods
#define BITBANG_PERIOD          (0x08) // vbls

#define REG_POSTFLG (*(volatile uint8_t*)0x04000300)

#define REG_SCFG_ROM (*(volatile uint16_t*)0x04004000)
#define REG_SCFG_CLK (*(volatile uint16_t*)0x04004004)
#define REG_SCFG_RST (*(volatile uint16_t*)0x04004006)
#define REG_SCFG_EXT (*(volatile uint32_t*)0x04004008)
#define REG_SCFG_MC  (*(volatile uint16_t*)0x04004010)

#define REG_DISPSTAT (*(volatile uint16_t*)0x04000004)
#define REG_VCOUNT   (*(volatile uint16_t*)0x04000006)

#define REG_IME (*(volatile uint32_t*)0x04000208)

#define REG_DMAxSAD(x)  (*(volatile uint32_t*)(0x040000b0 + 12*(x)))
#define REG_DMAxDAD(x)  (*(volatile uint32_t*)(0x040000b4 + 12*(x)))
#define REG_DMAxCNT(x)  (*(volatile uint32_t*)(0x040000b8 + 12*(x)))

#define REG_NDMAGCNT      (*(volatile uint32_t*)0x04004100)
#define REG_NDMAxSAD(x)   (*(volatile uint32_t*)(0x04004104 + 0x1c*(x)))
#define REG_NDMAxDAD(x)   (*(volatile uint32_t*)(0x04004108 + 0x1c*(x)))
#define REG_NDMAxTCNT(x)  (*(volatile uint32_t*)(0x0400410c + 0x1c*(x)))
#define REG_NDMAxWCNT(x)  (*(volatile uint32_t*)(0x04004110 + 0x1c*(x)))
#define REG_NDMAxBCNT(x)  (*(volatile uint32_t*)(0x04004114 + 0x1c*(x)))
#define REG_NDMAxFDATA(x) (*(volatile uint32_t*)(0x04004118 + 0x1c*(x)))
#define REG_NDMAxCNT(x)   (*(volatile uint32_t*)(0x0400411c + 0x1c*(x)))

#define BIOS_SIZE_FULL (0x10000)

extern uint32_t regbak[18]; // r0-r12, sp, lr, pc, oldpc, oldcpsr
// +POSTFLG?

/*#define bios_memcpy(src, dst, len) ((void (*)(const void*, volatile void*, size_t))(0x000010e0))*/
__attribute__((__always_inline__))
inline static void bios_memcpy(const void* src, volatile void* dst, size_t len) {
	void (*bios_memcpy_fptr)(const void*, volatile void*, size_t) = 0x10e0;
	bios_memcpy_fptr(src, dst, len);
}

__attribute__((__target__("arm"), __always_inline__))
inline static void swi_wait_by_loop(int32_t count) {
	register int32_t _count asm("r0") = count;
	asm volatile("swi 0x030000\n"::"r"(_count));
}

__attribute__((__target__("arm"))) void reboot_me(void);

static void wait_one_vbl(void) {
	while (REG_VCOUNT >= 192) asm volatile("nop");
	while (REG_VCOUNT <  192) asm volatile("nop");
}

/*#define bitbang_wait() wait_one_vbl()*/
#define bitbang_wait() swi_wait_by_loop(0x20BA/2)

__attribute__((__target__("arm"), __noreturn__, __section__(".text.undhandler7_c")))
void undhandler7_c(void);

// "backends":
// * SPI? --> piggyback pwman battery reads
// * I2C? --> write to unused bptwl reg
// * GPIO? --> yes!
// * camera led? --> slow but visible at least
// * RTC??? --> annoying (TODO)
// * eMMC?? SD/MMC?? SDIO?? --> even more annoying (TODO)
// * ...?

/** SPI **********************************************************************/

// "read" from pwman device reg. 1 with data in the "dummy" values

#define REG_SPICNT  (*(volatile uint16_t*)0x040001c0)
#define REG_SPIDATA (*(volatile uint8_t *)0x040001c0)

__attribute__((__target__("arm"), __section__(".text.spi_init")))
static void spi_init(void) {
	// pwman device, 1 MHz, enable, hold CS
	REG_SPICNT = (2<<0) | (0<<8) | (0<<10) | (1<<11) | (1<<15);
}

__attribute__((__target__("arm"), /*__noreturn__,*/ __section__(".text.spi_send")))
static uint8_t spi_send(const void* payload, size_t len_bytes) {
	const uint8_t* pl = payload;

	uint8_t acc = 0;

	for (size_t i = 0; i < len_bytes; ++i) {
		if (i != 0)
			REG_SPICNT = (2<<0) | (0<<8) | (0<<10) | (1<<11) | (1<<15);
		REG_SPIDATA = 1 | (1<<7); // register 1 (battery), read (bit 7)

		if (i == len_bytes - 1)
			REG_SPICNT = (2<<0) | (0<<8) | (0<<10) | (0<<11) | (1<<15); // release CS after xfer
		REG_SPIDATA = pl[i]; // "read" the battery level --> send our data!
		acc ^= REG_SPIDATA;

		while (REG_SPICNT & (1<<7)) ; // wait until ready
	}

	//for (;;) ;
	return acc;
}

/** I2C (driver) *************************************************************/

#define REG_I2CDATA (*(volatile uint8_t*)0x04004500)
#define REG_I2CCNT  (*(volatile uint8_t*)0x04004501)

#define wait_busy() do{while(REG_I2CCNT&0x80);}while(0)
#define get_result() ({wait_busy();(REG_I2CCNT>>4)&1;})
#define delay() do{wait_busy();for (int iii = 0; iii < del; ++iii) asm volatile("nop");}while(0)
#define stop(x) do{if(del){REG_I2CCNT = ((x)<<5)|0xc0;delay();REG_I2CCNT=0xc5;}else REG_I2CCNT = ((x)<<5)|0xc1;}while(0)

__attribute__((__target__("arm"), __section__(".text.i2c_write")))
static void i2c_write(int dev, int reg, uint8_t data) {
	int del = (dev == 0x4A) ? 0x180 : 0;

	for (int i = 0; i < 8; ++i) {
		// select device
		wait_busy();
		REG_I2CDATA = dev;
		REG_I2CCNT = 0xc2;
		if (get_result()) {
			// select register
			delay();
			REG_I2CDATA = reg;
			REG_I2CCNT = 0xc0;
			if (get_result()) {
				delay();
				REG_I2CDATA = data;
				stop(0);
				if (get_result()) return;
			}
		}

		REG_I2CCNT = 0xc5;
	}
}
__attribute__((__target__("arm"), __section__(".text.i2c_read")))
static int i2c_read(int dev, int reg) {
	int del = (dev == 0x4A) ? 0x180 : 0;

	for (int i = 0; i < 8; ++i) {
		// select device
		wait_busy();
		REG_I2CDATA = dev;
		REG_I2CCNT = 0xc2;
		if (get_result()) {
			// select register
			delay();
			REG_I2CDATA = reg;
			REG_I2CCNT = 0xc0;
			if (get_result()) {
				delay();
				// select device again?
				REG_I2CDATA = dev|1;
				REG_I2CCNT = 0xc2;
				if (get_result()) {
					delay();
					stop(1);
					wait_busy();
					return REG_I2CDATA;
				}
			}
		}

		REG_I2CCNT = 0xc5;
	}

	return -1;
}

#undef stop
#undef delay
#undef get_result
#undef wait_busy

/** I2C (payload) ************************************************************/

__attribute__((__target__("arm"), __section__(".text.i2c_init")))
static void i2c_init(void) {
	// ....
}
__attribute__((__target__("arm"), /*__noreturn__,*/ __section__(".text.i2c_send")))
static void i2c_send(const void* payload, size_t len_bytes) {
	// apparently BPTWL reg 0x21 is unused and can contain random data?
	const uint8_t* pl = payload;

	for (size_t i = 0; i < len_bytes; ++i) {
		i2c_write(0x4A, 0x21, pl[i]);
	}

	//for (;;) ;
}

/** GPIO *********************************************************************/

#define REG_GPIO_DATA (*(volatile uint8_t*)0x04004c00)
#define REG_GPIO_DIR  (*(volatile uint8_t*)0x04004c01)
#define REG_GPIO_EDGE (*(volatile uint8_t*)0x04004c02)
#define REG_GPIO_IE   (*(volatile uint8_t*)0x04004c03)

__attribute__((__target__("arm"), __section__(".text.gpio_init")))
static void gpio_init(void) {
	// disable interrupts
	REG_GPIO_EDGE = 0xff;
	REG_GPIO_IE   = 0;

	//REG_GPIO_DIR = 7|(1<<4)|(1<<7); // enable all GPIO18, GPIO330, GPIO333 as writeable
	REG_GPIO_DIR = 1<<4; // we're only using GPIO330 now
}

// TODO: which bitbang code to use? currently extremely boring unipolar NRZ
//       could maybe try (unipolar) manchester or unipolar RZ with differing
//       duty cycles, from which the diff. between a hang/EOT and just repeated
//       will be visible at least
__attribute__((__target__("arm"), /*__noreturn__,*/ __section__(".text.gpio_send")))
static void gpio_send(const void* payload, size_t len_bytes) {
	//const uint8_t* pl = payload;

	#define mkdata(b) (((b)<<0)|((b)<<1)|((b)<<2)|((b)<<4)|((b)<<7))
	/*#define mkdata(b) ((b)<<4)*/
	for (;;) {
	REG_GPIO_DATA = mkdata(1);
	asm volatile("nop");
	REG_GPIO_DATA = mkdata(0);
	}
	/*// generate a sync wave first
	for (int i = 0; i < BITBANG_PREAMBLE_LENGTH/2; ++i) {
		REG_GPIO_DATA = mkdata(1);
		for (int j = 0; j < BITBANG_PERIOD/2; ++j) bitbang_wait();
		REG_GPIO_DATA = mkdata(0);
		for (int j = 0; j < BITBANG_PERIOD/2; ++j) bitbang_wait();
	}
	// invert the phase
	for (int i = 0; i < BITBANG_PREAMBLE_LENGTH/2; ++i) {
		REG_GPIO_DATA = mkdata(0);
		for (int j = 0; j < BITBANG_PERIOD/2; ++j) bitbang_wait();
		REG_GPIO_DATA = mkdata(1);
		for (int j = 0; j < BITBANG_PERIOD/2; ++j) bitbang_wait();
	}

	// now send the data!
	for (int i = 0; i < len_bytes; ++i) {
		for (int j = 0; j < 8; ++j) {
			REG_GPIO_DATA = mkdata((pl[i]&(1<<j))?1:0);
			for (int iii = 0; iii < BITBANG_PERIOD; ++iii) bitbang_wait();
		}
	}*/

	//for (;;) ;
}

/** camera led ***************************************************************/

__attribute__((__target__("arm"), __section__(".text.camled_init")))
static void camled_init(void) {
	i2c_init();
}

__attribute__((__target__("arm"), /*__noreturn__,*/ __section__(".text.camled_send")))
static void camled_send(const void* payload, size_t len_bytes) {
	const uint8_t* pl = payload;

	for (int i = 0; i < BITBANG_PREAMBLE_LENGTH/2; ++i) {
		i2c_write(0x4A, 0x31, 1);
		for (int j = 0; j < BITBANG_PERIOD/2; ++j) bitbang_wait();
		i2c_write(0x4A, 0x31, 0);
		for (int j = 0; j < BITBANG_PERIOD/2; ++j) bitbang_wait();
	}
	// invert the phase
	for (int i = 0; i < BITBANG_PREAMBLE_LENGTH/2; ++i) {
		i2c_write(0x4A, 0x31, 0);
		for (int j = 0; j < BITBANG_PERIOD/2; ++j) bitbang_wait();
		i2c_write(0x4A, 0x31, 1);
		for (int j = 0; j < BITBANG_PERIOD/2; ++j) bitbang_wait();
	}

	// now send the data!
	for (int i = 0; i < len_bytes; ++i) {
		for (int j = 0; j < 8; ++j) {
			i2c_write(0x4A, 0x31, (pl[i]&(1<<j))?1:0);
			for (int iii = 0; iii < BITBANG_PERIOD; ++iii) bitbang_wait();
		}
	}

	/*for (;;) {
		i2c_write(0x4A, 0x31, 0); // turn off the LED
		for (int j = 0; j < 30; ++j) wait_one_vbl();
		i2c_write(0x4A, 0x31, 1);
		for (int j = 0; j < 30; ++j) wait_one_vbl();
	}*/
}

/** main *********************************************************************/

__attribute__((__section__(".data.payload")))
static uint8_t payload[] = {
	'H','e','l','l','o',' ','f','r','o','m',' ','A','R','M','7',0, // 16 bytes
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, // r12-r15
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, // oldpc, oldcpsr, postflg, scfg rom/clk
	0,0,0,0  // scfg ext
};

__attribute__((__section__(".data.bromhdr")))
static uint8_t bromhdr[] = {
	'B','o','o','t','R','O','M',' ','f','o','l','l','o','w','s',0  // 16 bytes
};

__attribute__((__section__(".bss.brombuf"), __aligned__(16)))
static uint8_t brombuf[0x1000];

__attribute__((__target__("arm"), __noreturn__, __section__(".text.undhandler7_c")))
void undhandler7_c(void) {
	REG_IME = 0;
	REG_DMAxCNT(3) = REG_DMAxCNT(2) = REG_DMAxCNT(1) = REG_DMAxCNT(0) = 0;
	REG_NDMAxCNT(3) = REG_NDMAxCNT(2) = REG_NDMAxCNT(1) = REG_NDMAxCNT(0) = 0;

	// NOTE: WE CANNOT USE 8-BIT WRITES FOR THIS, AS THIS MEMORY IS BACKED BY
	// WIFI RAM, WHICH DISCARDS 8-BIT WRITES
	uint32_t* pl = (uint32_t*)&payload[16];
	pl[0] = regbak[12]; // ipbak
	pl[1] = regbak[13]; // spbak
	pl[2] = regbak[14]; // lrbak
	pl[3] = regbak[15]; // pcbak

	pl[4] = regbak[16]; // oldsp
	pl[5] = regbak[17]; // oldcpsr
	pl[6] = REG_POSTFLG;
	pl[7] = REG_SCFG_ROM | ((uint32_t)REG_SCFG_CLK << 16);

	pl[8] = REG_SCFG_EXT;

	/*
	 * During normal operation:
	 * pl[0] = 0xe7830c0f
	 * pl[1] = 0x096a0993
	 * pl[2] = 0x0a89635c
	 * pl[3] = 0x5de0a573
	 * pl[4] = 0x2832cc37
	 * pl[5] = 0xc9781528
	 * pl[6] = 0x00000000
	 * pl[7] = 0x00860101
	 * pl[8] = 0x93fbff00
	 */
	/*
	 * BootROM Exploit:
	 * pl[0] = 0x00000000 \
	 * pl[1] = 0x00000000 | oops
	 * pl[2] = 0x00000000 |
	 * pl[3] = 0x00000000 /
	 * pl[4] = 0x037eab30
	 * pl[5] = 0x8000001f
	 * pl[6] = 0x00000000
	 * pl[7] = 0x00870000
	 * pl[8] = 0x93ffff00
	 */

	REG_SCFG_EXT |= (1<<22/*I2C*/) | (1<<23/*GPIO*/) | (1<<31/*keep SCFG enabled*/);

#if OUT_BACKEND == OUTBAK_CAMLED
	camled_init();
	for (;;) {
		i2c_write(0x4A, 0x31, 0); // turn off the LED
		for (int j = 0; j < 30; ++j) wait_one_vbl();
		i2c_write(0x4A, 0x31, 1);
		for (int j = 0; j < 30; ++j) wait_one_vbl();
	}
	//camled_send(payload, sizeof payload);
#elif OUT_BACKEND == OUTBAK_GPIO
	gpio_init();
	gpio_send(payload, sizeof payload);
#elif OUT_BACKEND == OUTBAK_I2C
	i2c_init();
	i2c_send(payload, sizeof payload);
#else /* default: SPI */
	spi_init();
	spi_send(payload, sizeof payload);
#endif

	bool is_during_stage1 = (REG_SCFG_ROM & 0x0100/*ARM7 ROM disable flag*/) == 0x0000;
	if (is_during_stage1) {
#if (OUT_BACKEND != OUTBAK_I2C) && (OUT_BACKEND != OUTBAK_CAMLED)
		camled_init(); // calls i2c_init
#endif
		// we're golden! let's celebrate by blinking an LED!
		for (size_t i = 0; i < 8; ++i) {
			i2c_write(0x4A, 0x31, 1);
			wait_one_vbl();//bitbang_wait();
			i2c_write(0x4A, 0x31, 0);
			wait_one_vbl();//bitbang_wait();
		}

#if OUT_BACKEND == OUTBAK_SPI
		spi_init();
#endif
		while (1) {
#if OUT_BACKEND == OUTBAK_I2C
			i2c_send(bromhdr, sizeof bromhdr);
#else /* default: SPI */
			spi_send(bromhdr, sizeof bromhdr);
#endif
			for (size_t jjj = 0; jjj < (BIOS_SIZE_FULL / sizeof(brombuf)); ++jjj) {
				// now, the buffer resides in wifi nvram, so not knowing about
				// the implementation of this BIOS routine could probably fail.
				// however, the routine at 0x10e0 seems to always copy at word
				// granularity (or more, using ldm/stm to xfer 8 words at once),
				// so there's no hazard in using this here rn
				bios_memcpy(
					(const void*)(jjj * sizeof(brombuf) + 0x0/*BIOS org*/),
					brombuf, sizeof brombuf
				);
#if OUT_BACKEND == OUTBAK_I2C
				i2c_send(brombuf, sizeof brombuf);
#else /* default: SPI */
				spi_send(brombuf, sizeof brombuf);
#endif
			}

#if OUT_BACKEND == OUTBAK_I2C
			i2c_send(payload, sizeof payload);
#else /* default: SPI */
			spi_send(payload, sizeof payload);
#endif
		}
	} else {
		gpio_init();

		// sad, this didn't seem to have worked
		for (size_t i = 0; i < 8; ++i) {
			REG_GPIO_DATA = 1<<4;
			bitbang_wait();
			REG_GPIO_DATA = 0<<4;
			bitbang_wait();
		}

		reboot_me();
	}

	for (;;) ;
	__builtin_unreachable();
}

/** reboot_me ****************************************************************/

struct __attribute__((__packed__)) nocash_hdr {
	char magic[12];
	uint16_t crclen;
	uint16_t crc;
	uint32_t flg;
	uint16_t bgcol_main;
	uint16_t bgcol_sub;
	uint8_t _reserved[0x20];
	uint16_t path[0x104];
};

#define NOCASH_AUTOBOOT ((volatile struct nocash_hdr*)0x02000800)

static const unsigned char ali_mag[] = "AutoLoadInfo";
static const unsigned char bootto [] = "sdmc:/bootcode.dsi";

__attribute__((__target__("arm"), __always_inline__))
inline static uint16_t swi_crc16(uint16_t start, const volatile void* stuff, size_t len) {
	register uint32_t _start asm("r0") = start;
	register uint32_t _stuff asm("r1") = (uint32_t)stuff;
	register uint32_t _len   asm("r2") = (uint32_t)len  ;
	asm volatile("swi 0x0e0000\n":"+r"(_start):"r"(_stuff),"r"(_len):"r3");
	return (uint16_t)_start;
}

__attribute__((__target__("arm"), __noreturn__, __section__(".text.reboot_me")))
void reboot_me(void) {
	// set up to reboot into ourselves again if the glitching would've failed

	// we don't need a 'vram-safe' (and wifiram-safe) memcpy here because this
	// is only writing to MRAM/'FCRAM'
	REG_IME = 0;
	for (size_t i = 0; i < 12; ++i) {
		NOCASH_AUTOBOOT->magic[i] = ali_mag[i];
	}
	NOCASH_AUTOBOOT->crclen = 0x3f0;
	NOCASH_AUTOBOOT->flg = 1;
	NOCASH_AUTOBOOT->bgcol_main = 0;
	NOCASH_AUTOBOOT->bgcol_sub  = 0;

	for (size_t i = 0; i < sizeof(bootto); ++i)
		NOCASH_AUTOBOOT->path[i] = (uint16_t)bootto[i];

	NOCASH_AUTOBOOT->crc = swi_crc16(0xffff, &NOCASH_AUTOBOOT->flg, 0x3F0);

	i2c_init();
	i2c_write(0x4A, 0x70, 1); // warmboot
	i2c_write(0x4A, 0x11, 1); // reset!

	for (;;) asm volatile("nop");
	__builtin_unreachable();
}

/** on_hang_c ****************************************************************/

extern uint32_t mimic_b9[];
extern uint32_t mimic_b9_end[];

__attribute__((__target__("arm"), __always_inline__))
inline static void raise_und(void) {
	asm volatile(".4byte 0xe7f000f0\n");
	__builtin_unreachable();
}

const int magic_fill = 0x214e5454;
const int magic_handle = ~magic_fill;
extern void undhandler_7(uint32_t magic, int lock);

__attribute__((__target__("arm"), __section__(".text.on_hang_c")))
void on_hang_c(void) {
	//undhandler7_c();
	camled_init();

	const int32_t one_ms = 0x20BA;

	void (*mimic_in_wram)(void) = 0x03800000;
	//void (*nopsled_in_wram)(void) = 0x0380ae81;//00;//0x03800000 - 0x8000;

	bios_memcpy(mimic_b9, (volatile void*)mimic_in_wram,
			(size_t)mimic_b9_end - (size_t)mimic_b9);

#if 0
	for (size_t i = 0; i < 0x10; ++i)
#else
	while (1)
#endif
	{
		// turn on cam led
		i2c_write(0x4A, 0x31, 1);
		swi_wait_by_loop(one_ms/2);
		mimic_in_wram();
		swi_wait_by_loop(one_ms/2);
		i2c_write(0x4A, 0x31, 0);
		swi_wait_by_loop(one_ms);
	}
	//undhandler_7(~magic_fill, -1);
	//nopsled_in_wram();
	//raise_und();

	//i2c_write(0x4A, 0x31, 1);
	for (;;);
}


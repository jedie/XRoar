/* The GP32s sound is generated by a DAC (Philips UDA1330A) which is controlled
 * with an L3 bus interface and fed audio data using the IIS (I2S) bus.
 * The L3 bus is simulated by "waggling the bits" from 3 data pins on IO port
 * E.  The IIS bus is itself usually fed by setting up a DMA.  This file only
 * provides routines for setting up the IIS bus and the UDA chip.
 *
 * Documentation on the UDA1330A DAC (including communicating with it over
 * L3 bus) can, at time of writing, be found here (download the datasheet):
 * http://www.semiconductors.philips.com/pip/UDA1330ATS_N2.html
 *
 * The IIS bus is on the same die as the CPU.  Mr_Spiv is hosting a copy of
 * the datasheet for that here:
 * http://www.cs.helsinki.fi/u/jikorhon/condev/gp32/dl/um_s3c2400x_rev1.1.pdf
 * */

#include <stdlib.h>

#include "../types.h"
#include "udaiis.h"
#include "gp32.h"

/* Bits used on IO port E for the L3 bus */
#define L3DATA (1 << 11)
#define L3MODE (1 << 10)
#define L3CLOCK (1 << 9)
#define L3DELAY 16

/* Used for configuring the UDA1330A */
#define UDA_ADDR_DATA (0x14 | 0)
#define UDA_ADDR_STATUS (0x14 | 2)
#define UDA_NOMUTE (0x80)
#define UDA_MUTE (0x80 | (1 << 2))
static uint32_t u_deemph;
static uint32_t u_mute;

/* Delay macro - inserts a bunch of NOPs */
#define NOP(r) { int i; for (i = (r); i; i--) { asm ("nop"); } }

static void l3_init(void) {
	/* IO Port E, PE9-PE11 are used to send data over L3 bus */
	rPEDAT |= L3MODE | L3CLOCK;  /* L3DATA only matters when writing */
	rPEUP  |= L3MODE | L3CLOCK | L3DATA;  /* disable pull-up on used pins */
	/* Set PE9-PE11 as outputs */
	rPECON  = (rPECON & ~(0x3f<<18)) | (1<<18) | (1<<20) | (1<<22);
}

static void l3_set_addr(uint8_t addr) {
	int i;
	rPEDAT = (rPEDAT & ~L3MODE) | L3CLOCK;
	NOP(L3DELAY);           //tsu(L3) > 190ns
	for (i = 0; i < 8; i++) {
		if (addr & 1)
			rPEDAT |= L3DATA;
		else
			rPEDAT &= ~L3DATA;
		rPEDAT &= ~L3CLOCK;
		NOP(L3DELAY);
		rPEDAT |= L3CLOCK;
		NOP(L3DELAY);
		addr >>= 1;
	}
	rPEDAT |= L3MODE;
	NOP(L3DELAY);
}

static void l3_write_data(uint8_t data) {
	int i;
	rPEDAT |= L3MODE | L3CLOCK;
	NOP(L3DELAY);
	for (i = 0; i < 8; i++) {
		if (data & 1)
			rPEDAT |= L3DATA;
		else
			rPEDAT &= ~L3DATA;
		rPEDAT &= ~L3CLOCK;
		NOP(L3DELAY);
		rPEDAT |= L3CLOCK;
		NOP(L3DELAY);
		data >>= 1;
	}
	rPEDAT &= ~L3MODE;
	NOP(L3DELAY);
	rPEDAT |= L3MODE;
}

/* rate is overwritten with the actual rate, prescaler is returned
 * (with bit 31 set if master clock should be PCLK/384) */
static uint32_t calcrate(uint32_t pclk, uint32_t *rate) {
	uint32_t pd256 = pclk / 256;
	uint32_t pd384 = pclk / 384;
	uint32_t ps256 = (pd256-(*rate>>1)) / *rate;
	uint32_t ps384 = (pd384-(*rate>>1)) / *rate;
	uint32_t r256, r384;
	if (ps256 > 31) ps256 = 31;
	if (ps384 > 31) ps384 = 31;
	r256 = pd256 / (ps256 + 1);
	r384 = pd384 / (ps384 + 1);
	if (abs(*rate - r256) <= abs(*rate - r384)) {
		*rate = r256;
		return ps256;
	} else {
		*rate = r384;
		return ps384 | (1<<31);
	}
}

static uint32_t iis_init(uint32_t pclk, uint32_t *rate) {
	uint32_t prescale = calcrate(pclk, rate);
	uint32_t mclk = (prescale >> 31);

	prescale &= 0x1f;
	/****** IIS Initialize ******/

	rIISPSR = 0;
	rIISCON = 0;
	rIISMOD = 0;
	rIISFIFCON = 0;

	rIISCON = (1 << 5) |	/* Transmit DMA service request enable */
		  (0 << 4) |	/* Receive DMA service request enable */
		  (0 << 3) |	/* Transmit channel idle command */
		  (1 << 2) |	/* Receive channel idle command */
		  (1 << 1) |	/* IIS prescaler enable */
		  (0 << 0);	/* IIS interface enable (start) */

	rIISMOD = (0 << 8) |	/* Mode select (0=master,1=slave) */
		  (2 << 6) |	/* Tx/Rx mode (0=none,1=Rx,2=Tx,3=duplex) */
		  (0 << 5) |	/* Active level (0=low for left chan,1=high) */
		  (0 << 4) |	/* Serial i/f format (0=IIs,1=MSB) */
		  (0 << 3) |	/* Serial bits per channel (0=8bit,1=16bit) */
		  (mclk << 2) |	/* Master clock frequency (0=256fs,1=384fs) */
		  (1 << 0);	/* Serial clock freq (0=16fs,1=32fs,2=48fs) */

	rIISFIFCON= (1 << 11) |	/* Tx fifo mode (0=normal,1=DMA) */
		    (0 << 10) |	/* Rx fifo mode (0=normal,1=DMA) */
		    (1 << 9) |	/* Tx fifo enable */
		    (0 << 8);	/* Rx fifo enable */
		    
	rIISPSR = (prescale << 5) | (prescale << 0);

	rIISCON |= (1 << 0);	/* IIS interface enable (start) */
	return mclk ? 1 : 2;
}

static void uda_init(int sysclk) {
	/* IO Port G: PG2 = CDCLK, PG3 = I2SSDO */
	rPGCON |= (rPGCON & ~(0xf << 4)) | (2 << 4) | (2 << 6);
	l3_init();  /* Initialise L3 bus (some pins on IO port E) */
	l3_set_addr(UDA_ADDR_STATUS);
	l3_write_data((sysclk<<4) | (0<<1));
	uda_volume(63);
	u_deemph = UDA_NODE;
	u_mute = UDA_NOMUTE;
	l3_set_addr(UDA_ADDR_DATA);
	l3_write_data(u_deemph | u_mute);
}

void udaiis_init(uint32_t pclk, uint32_t *rate) {
	int sysclk;
	sysclk = iis_init(pclk, rate);
	uda_init(sysclk);
}

void uda_volume(unsigned int volume) {
	if (volume > 63) volume = 63;
	l3_set_addr(UDA_ADDR_DATA);
	l3_write_data(~volume & 0x3f);
}

void uda_mute(int mute) {
	if (mute)
		u_mute = UDA_MUTE;
	else
		u_mute = UDA_NOMUTE;
	l3_set_addr(UDA_ADDR_DATA);
	l3_write_data(u_deemph | u_mute);
}

void uda_deemph(int deemph) {
	u_deemph = deemph;
	l3_set_addr(UDA_ADDR_DATA);
	l3_write_data(u_deemph | u_mute);
}

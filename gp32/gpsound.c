#include <gpmem.h>
#include "gp32.h"
#include "udaiis.h"
#include "gpsound.h"
#include "../types.h"

static void playbuffer(uint8_t *src, uint32_t size);

static uint_least32_t frame_bytes;
static uint16_t *buffer[2];

void gpsound_init(uint32_t pclk, uint32_t *rate) {
	rDMASKTRIG2 = (1<<2) + (0<<1) + 0;
	udaiis_init(pclk, rate); /* set up IIS bus and sound chip */
}

/* On input, size = samples per frame, then used to compute length in bytes */
/* Returns 2 element array of pointers to buffers, where to write audio data */
/* TODO: Cope with 16-bit audio */
uint16_t **gpsound_buffers(int size) {
	/* Buffer should be on 4k boundary so we can turn off writeback
	 * for it */
	uint32_t bytes = ((size*4) + 0xfff) & ~0xfff;
	uint8_t *tmp = malloc((size*4) + 0xfff);
	buffer[0] = (uint16_t *)(((uint32_t)tmp + 0xfff) & ~0xfff);
	buffer[1] = buffer[0] + size;
	swi_mmu_change(buffer[0], buffer[0] + bytes - 1, MMU_NCB);
	frame_bytes = size * 4;
	return buffer;
}

/* Sets up a DMA to send data from a buffer (src) to the IIS fifo (which
 * feeds the DAC). */
static void playbuffer(uint8_t *src, uint32_t size) {
	/* DMA source: */
	rDISRC2 = (0<<30) |	/* Source on system bus */
	          (0<<29) |	/* Auto-increment */
	          (uint32_t)src;	/* Start address */
	/* DMA destination: */
	rDIDST2 = (1<<30) |	/* Dest on peripheral bus */
	          (1<<29) |	/* Fixed address */
	          (uint32_t)IISFIF;	/* IIS FIFO */
	/* DMA control: */
	rDCON2  = (1<<30) |	/* Handshake mode */
	          (0<<29) |	/* Sync DREQ/DACK to PCLK */
	          (0<<28) |	/* IRQ when finished */
	          (0<<27) |	/* Unit transfer (1 = burst) */
	          (0<<26) |	/* Single service (1 = whole service) */
	          (0<<24) |	/* Source = I2SSDO */
	          (1<<23) |	/* H/W request mode */
	          (0<<22) |	/* Auto-reload */
	          (0<<20) |	/* Transfer units (0=bytes,1=hwords,2=words) */
	          (size);	/* Transfer size (bytes) */
	/* Start DMA: */
	rDMASKTRIG2 = (0<<2) |	/* No stop */
	              (1<<1) |	/* DMA on */
	              (0<<0);	/* No S/W request */
}

void gpsound_start(void) {
	playbuffer((uint8_t *)buffer[0], frame_bytes);
}

void gpsound_stop(void) {
         /****** IIS Tx Stop ******/
         rIISCON=0x0;        /* IIS stop */
         rDMASKTRIG2=(1<<2); /* DMA2 stop */
         rIISFIFCON=0x0;     /* for FIFO flush */
}

/* Make the SDK happy */
void GpProcSound(void);
void GpProcSound(void) { }

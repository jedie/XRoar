#include <string.h>
#include <nds.h>

#include "types.h"
#include "nds/ndsipc.h"

/* ndslib headers calculate sample rate incorrectly (at least according
 * to gbatek, so 32768Hz here actually ends up being 32728Hz */
#define SAMPLE_RATE 32768
#define FRAME_SIZE 256

static touchPosition tempPos;
static int playing_buf = 0;

static void vblank_handler(void);
static void timer3_handler(void);

/**************************************************************************/

static void vblank_handler(void) {
	static int lastbut = -1;
	//uint16_t but, x=0, y=0, xpx=0, ypx=0, z1=0, z2=0;
	uint16_t but, xpx=0, ypx=0;

	but = REG_KEYXY;

	if (!( (but ^ lastbut) & (1 << 6))) {
		touchReadXY(&tempPos);
		if (tempPos.rawx == 0 || tempPos.rawy == 0) {
			but |= (1 << 6);
			lastbut = but;
		} else {
			//x   = tempPos.x;
			//y   = tempPos.y;
			xpx = tempPos.px;
			ypx = tempPos.py;
			//z1  = tempPos.z1;
			//z2  = tempPos.z2;
		}
	} else {
		lastbut = but;
		but |= (1 <<6);
	}

	//IPC->touchX   = x;
	//IPC->touchY   = y;
	IPC->touchXpx = xpx;
	IPC->touchYpx = ypx;
	//IPC->touchZ1  = z1;
	//IPC->touchZ2  = z2;
	IPC->buttons  = but;
}

static void timer3_handler(void) {
	playing_buf ^= 1;
	REG_IPC_SYNC = (playing_buf << 8);
}

/**************************************************************************/

int main(int argc, char **argv) {
	uint32_t sound_buf;
	(void)argc;
	(void)argv;

	/* Reset the clock if needed */
	rtcReset();

	/* enable sound */
	powerOn(POWER_SOUND);
	REG_SOUNDCNT = SOUND_ENABLE | SOUND_VOL(0x7f);
	REG_SOUNDBIAS = 0x200;

	/* Stop power LED from blinking (DS-X sets this up) */
	writePowerManagement(0, readPowerManagement(0) & 0xcf);

	irqInit();
	irqSet(IRQ_VBLANK, vblank_handler);

	IPC->buttons = ~0;

	/* handshake with ARM9 to receive sound buffer address */
	REG_IPC_FIFO_CR = (1 << 3) | (1 << 15);  /* enable FIFO, clear sendq */
	while ((REG_IPC_SYNC & 15) != 14);
	REG_IPC_SYNC = (14 << 8);
	while ((REG_IPC_SYNC & 15) != 0);
	REG_IPC_SYNC = (0 << 8);
	sound_buf = REG_IPC_FIFO_RX;

	SCHANNEL_CR(0) = 0;
	SCHANNEL_REPEAT_POINT(0) = 0;
	SCHANNEL_LENGTH(0) = (FRAME_SIZE * 2) >> 2;
	SCHANNEL_TIMER(0) = SOUND_FREQ(SAMPLE_RATE);
	SCHANNEL_SOURCE(0) = sound_buf;

	playing_buf = 0;
	TIMER3_CR = 0;
	TIMER3_DATA = SOUND_FREQ(SAMPLE_RATE)*2;
	irqSet(IRQ_TIMER3, timer3_handler);

	/* Start audio and tracking timer */
	SCHANNEL_CR(0) = (127 << 0) | (64 << 16) | (1 << 27) | (1 << 31);
	TIMER3_CR = (2 << 0) | (1 << 6) | (1 << 7);

	irqEnable(IRQ_VBLANK | IRQ_TIMER3);

	/* Try to keep the ARM7 mostly idle */
	while (1) {
		swiIntrWait(1, IRQ_VBLANK | IRQ_TIMER3);
	}
}

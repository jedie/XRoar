#include <nds.h>

#include "types.h"

static touchPosition tempPos;

static void vcount_handler(void);

/**************************************************************************/

static void vcount_handler(void) {
	static int lastbut = -1;
	uint16_t but, x=0, y=0, xpx=0, ypx=0, z1=0, z2=0;

	but = REG_KEYXY;

	if (!( (but ^ lastbut) & (1 << 6))) {
		tempPos = touchReadXY();
		if (tempPos.x == 0 || tempPos.y == 0) {
			but |= (1 << 6);
			lastbut = but;
		} else {
			x   = tempPos.x;
			y   = tempPos.y;
			xpx = tempPos.px;
			ypx = tempPos.py;
			z1  = tempPos.z1;
			z2  = tempPos.z2;
		}
	} else {
		lastbut = but;
		but |= (1 <<6);
	}

	IPC->touchX   = x;
	IPC->touchY   = y;
	IPC->touchXpx = xpx;
	IPC->touchYpx = ypx;
	IPC->touchZ1  = z1;
	IPC->touchZ2  = z2;
	IPC->buttons  = but;
}

/**************************************************************************/

int main(int argc, char **argv) {
	(void)argc;
	(void)argv;

	/* Reset the clock if needed */
	rtcReset();

	/* enable sound */
	powerON(POWER_SOUND);
	SOUND_CR = SOUND_ENABLE | SOUND_VOL(0x7f);
	SOUND_BIAS = 0x200;

	/* Stop power LED from blinking (DS-X sets this up) */
	writePowerManagement(0, readPowerManagement(0) & 0xcf);

	irqInit();
	SetYtrigger(80);
	irqSet(IRQ_VCOUNT, vcount_handler);
	irqEnable(IRQ_VBLANK | IRQ_VCOUNT);

	IPC->mailBusy = 0;

	SCHANNEL_CR(0) = 0;
	SCHANNEL_REPEAT_POINT(0) = 0;
	SCHANNEL_SOURCE(0) = (0x02800000 - 256);
	SCHANNEL_LENGTH(0) = 256 >> 2;
	SCHANNEL_TIMER(0) = SOUND_FREQ(22050);
	SCHANNEL_CR(0) = (127 << 0)
		| (64 << 16)
		| (1 << 27)
		| (1 << 31);

	/* Keep the ARM7 out of main RAM */
	while (1) {
		swiIntrWait(1, IRQ_VBLANK | IRQ_VCOUNT);
	}
}

#include <nds.h>

#include "types.h"

static touchPosition tempPos;

static void startSound(int sampleRate, const void *data, uint32_t bytes, uint8_t channel, uint8_t vol,  uint8_t pan, uint8_t format);
static int getFreeSoundChannel(void);
static void VcountHandler(void);
static void VblankHandler(void);

/**************************************************************************/

static void startSound(int sampleRate, const void* data, uint32_t bytes, uint8_t channel, uint8_t vol,  uint8_t pan, uint8_t format) {
	SCHANNEL_TIMER(channel)  = SOUND_FREQ(sampleRate);
	SCHANNEL_SOURCE(channel) = (uint32_t)data;
	SCHANNEL_LENGTH(channel) = bytes >> 2 ;
	SCHANNEL_CR(channel)     = SCHANNEL_ENABLE | SOUND_ONE_SHOT | SOUND_VOL(vol) | SOUND_PAN(pan) | (format==1?SOUND_8BIT:SOUND_16BIT);
}

static int getFreeSoundChannel(void) {
	int i;
	for (i=0; i<16; i++) {
		if ( (SCHANNEL_CR(i) & SCHANNEL_ENABLE) == 0 ) return i;
	}
	return -1;
}

static void VcountHandler(void) {
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

static void VblankHandler(void) {
	uint32_t i;

	/* sound code  :) */
	TransferSound *snd = IPC->soundData;
	IPC->soundData = 0;

	if (0 != snd) {
		for (i = 0; i < snd->count; i++) {
			int32_t chan = getFreeSoundChannel();
			if (chan >= 0) {
				startSound(snd->data[i].rate, snd->data[i].data, snd->data[i].len, chan, snd->data[i].vol, snd->data[i].pan, snd->data[i].format);
			}
		}
	}
}

/**************************************************************************/

int main(int argc, char **argv) {
	(void)argc;
	(void)argv;

	/* Reset the clock if needed */
	rtcReset();

	/* enable sound */
	powerON(POWER_SOUND);
	SOUND_CR = SOUND_ENABLE | SOUND_VOL(0x7F);

	/* Stop power LED from blinking (DS-X sets this up) */
	writePowerManagement(0, readPowerManagement(0) & 0xcf);

	irqInit();
	irqSet(IRQ_VBLANK, VblankHandler);
	SetYtrigger(80);
	irqSet(IRQ_VCOUNT, VcountHandler);
	irqEnable(IRQ_VBLANK | IRQ_VCOUNT);

	IPC->mailBusy = 0;

	/* Keep the ARM7 out of main RAM */
	while (1)
		swiWaitForVBlank();
}

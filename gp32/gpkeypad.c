#include <gpstdlib.h>
#include "gp32.h"
#include "gpkeypad.h"

static int oldkey;
static unsigned int repeat_mask;
static int repeat_rate;
static unsigned int repeat_ticks;

int gpkeypad_init(void) {
	oldkey = 0;
	repeat_mask = 0;
	repeat_rate = 225;
	repeat_ticks = GpTickCountGet();
	return 0;
}

void gpkeypad_get_repeat(unsigned int *mask, int *rate) {
	if (mask)
		*mask = repeat_mask;
	if (rate)
		*rate = repeat_rate;
}

void gpkeypad_set_repeat(unsigned int mask, int rate) {
	repeat_mask = mask;
	repeat_rate = rate;
	repeat_ticks = GpTickCountGet();
}

/* Updates:
 * key - currently pressed keys
 * newkey - keys that weren't pressed last time (repeat mask applied)
 * relkey - keys released since last time */
void gpkeypad_poll(unsigned int *key, unsigned int *newkey,
		unsigned int *relkey) {
	unsigned int nkey = ((~rPBDAT & 0xff00) >> 8) | ((~rPEDAT & 0xc0) << 2);
	unsigned int nnewkey = nkey & ~oldkey;
	unsigned int nrelkey = ~nkey & oldkey;
	unsigned int cur = GpTickCountGet();
	oldkey = nkey;
	if (nkey == 0)
		repeat_ticks = cur - repeat_rate;
	if (cur > (repeat_rate + repeat_ticks)) {
		repeat_ticks = cur;
		nnewkey |= (nkey & repeat_mask);
	}
	if (key)
		*key = nkey;
	if (newkey)
		*newkey = nnewkey;
	if (relkey)
		*relkey = nrelkey;
}

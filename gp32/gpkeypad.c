#include <gpstdlib.h>
#include "gp32.h"
#include "gpkeypad.h"

static int oldkey;
static int repeat_rate;
static unsigned int repeat_ticks;

int gpkeypad_init(void) {
	oldkey = ~0;
	repeat_rate = 225;
	repeat_ticks = GpTickCountGet();
	return 0;
}

void gpkeypad_repeat_rate(int rate) {
	repeat_rate = rate;
	repeat_ticks = GpTickCountGet();
}

/* Updates:
 * key - currently pressed keys
 * newkey - keys that weren't pressed last time
 * rkey - key + autorepeat */
void gpkeypad_poll(int *key, int *newkey, int *rkey) {
	int nkey = ((~rPBDAT & 0xff00) >> 8) | ((~rPEDAT & 0xc0) << 2);
	int nnewkey = nkey & oldkey;
	unsigned int cur = GpTickCountGet();
	oldkey = ~nkey;
	if (nkey == 0)
		repeat_ticks = cur - repeat_rate;
	if (cur > (repeat_rate + repeat_ticks)) {
		repeat_ticks = cur;
		if (rkey)
			*rkey = nkey;
	} else {
		if (rkey)
			*rkey = 0;
	}
	if (key)
		*key = nkey;
	if (newkey)
		*newkey = nnewkey;
}

/* usage:
 * #include "gp32/gp32.h"
 * #include "gp32/gpkeypad.h"
 *
 * gpkeypad_init();
 * gpkeypad_repeat_rate(n); - set repeat rate in 'ticks'
 * gpkeypad_poll(&k,&n,&r); - k = currently depressed keys,
 * 	n = newly depressed keys, r = depressed keys (with autorepeat)
 */

int gpkeypad_init(void);
void gpkeypad_repeat_rate(int rate);
void gpkeypad_poll(int *key, int *newkey, int *rkey);

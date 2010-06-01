/* usage:
 * #include "gp32/gp32.h"
 * #include "gp32/gpkeypad.h"
 *
 * gpkeypad_init();
 * gpkeypad_set_repeat(k,n); - set repeat for k to rate in 'ticks'
 * gpkeypad_poll(&k,&n,&r); - k = currently depressed keys,
 * 	n = newly depressed keys, r = released keys
 */

#ifndef XROAR_GP32_GPKEYPAD_H_
#define XROAR_GP32_GPKEYPAD_H_

int gpkeypad_init(void);
void gpkeypad_get_repeat(unsigned int *mask, int *rate);
void gpkeypad_set_repeat(unsigned int mask, int rate);
void gpkeypad_poll(unsigned int *key, unsigned int *newkey,
		unsigned int *relkey);

#endif  /* XROAR_GP32_GPKEYPAD_H_ */

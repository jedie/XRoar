/* Set up IIS and UDA1330A (audio chip) */

#ifndef XROAR_GP32_UDAIIS_H_
#define XROAR_GP32_UDAIIS_H_

#include "../types.h"

#define UDA_NODE (0x80)
#define UDA_DE_32 (0x80 | (1 << 3))
#define UDA_DE_44 (0x80 | (2 << 3))
#define UDA_DE_48 (0x80 | (3 << 3))

/* Sets up the IIS bus and the UDA chip.  The supplied 'rate' is altered
 * to reflect the actual samplerate achievable. */
void udaiis_init(uint32_t pclk, uint32_t *rate);
void uda_volume(unsigned int volume);	/* 0-63, 0 being lowest volume */
void uda_mute(int mute);	/* 1 mutes, 0 unmutes */
void uda_deemph(int deemph);	/* pass UDA_NODE or UDA_DE_?? */

#endif  /* XROAR_GP32_UDAIIS_H_ */

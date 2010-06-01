/* This implements a very simple 2 frame audio buffer with an exported flag
 * indicating which frame is currently playing.  Good for syncing to a known
 * samplerate.  Use like this:
 * 
 * #include <gpsound.h>
 * int rate, writing_frame = 1;
 * uint16_t **frames;
 * gpsound_init(PCLK, &rate);
 * frames = gpsound_buffers(rate / 50);  // 1 audio frame = 1 video frame?
 * while (1) {
 *   do stuff ...
 *   while (writing_frame == playing_frame); // wait for frame to finish
 *   fill frames[writing_frame] ...
 *   writing_frame ^= 1;
 * }    
 * */   

#ifndef XROAR_GP32_GPSOUND_H_
#define XROAR_GP32_GPSOUND_H_

#include "../types.h"

extern volatile unsigned int playing_frame;

void gpsound_init(uint32_t pclk, uint32_t *rate);
uint16_t **gpsound_buffers(int size);
void gpsound_start(void);
void gpsound_stop(void);

#endif  /* XROAR_GP32_GPSOUND_H_ */

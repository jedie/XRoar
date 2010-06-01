#ifndef XROAR_NDS_NDSIPC_H_
#define XROAR_NDS_NDSIPC_H_

#include <nds.h>

struct ndsipc {
	int touchXpx;
	int touchYpx;
	int buttons;
};


#define IPC ((struct ndsipc volatile *)(0x027ff000))

#endif  /* XROAR_NDS_NDSIPC_H_ */

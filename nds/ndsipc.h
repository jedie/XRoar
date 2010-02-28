#ifndef NDSIPC_H
#define NDSIPC_H

#include <nds.h>

struct ndsipc {
	int touchXpx;
	int touchYpx;
	int buttons;
};


#define IPC ((struct ndsipc volatile *)(0x027ff000))

#endif  /* NDSIPC_H */

/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2014  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_DELEGATE_H_
#define XROAR_DELEGATE_H_

typedef struct {
	void (*func)(void *);
	void *sptr;
} delegate_null;

typedef struct {
	void (*func)(void *, _Bool value);
	void *sptr;
} delegate_bool;

typedef struct {
	void (*func)(void *, int value);
	void *sptr;
} delegate_int;

typedef struct {
	void (*func)(void *, float value);
	void *sptr;
} delegate_float;

#define DELEGATE_CALL0(d) ((d).func((d).sptr))
#define DELEGATE_CALL1(d,v) ((d).func((d).sptr, (v)))
#define DELEGATE_SAFE_CALL0(d) do { if ((d).func) { DELEGATE_CALL0((d)); } } while (0)
#define DELEGATE_SAFE_CALL1(d,v) do { if ((d).func) { DELEGATE_CALL1((d),(v)); } } while (0)

#endif  /* XROAR_DELEGATE_H_ */

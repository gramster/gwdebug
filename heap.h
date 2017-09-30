/*
 * heap.h - A portable dynamic memory heap manager
 *
 * Written by Graham Wheeler, January 1995.
 * (c) 1995, All Rights Reserved
 *
 * See heap.c for documentation.
 *
 * Last modified:
 *	29-1-95		Fixed to handle large heaps
 *
 * Contact: gram@aztec.co.za
 */

#ifndef _HEAP_H
#define _HEAP_H

#ifdef LOCAL_HEAP

/* Note that we use macros for the standard names, rather than
   defining actual malloc, etc routines. This is because malloc
   is called by the startup code before main() is executed, and
   we don't want to replace those calls (if we do, then the
   routines should be renamed and any macros below that end up in the
   form "#define X X" should be removed) */

/* Support routines */

void GWsetheap(char far *base, unsigned long extent);
unsigned long GWheap_used();
unsigned long GWheap_avail();
void GWheapstatus(int detailed);

#define setheap(b, e)	GWsetheap((char far *)b, e)
#define heapstatus(d)	GWheapstatus(d)
#define heap_avail()	GWheap_avail()
#define heap_used()	GWheap_used()

/* Standard library replacements */

void far *GWmalloc(unsigned long size);
void far *GWcalloc(unsigned long nitems, unsigned long size);
void far *GWrealloc(void far *p, unsigned long size);
void  GWfree(void far *p);

#define malloc(s)	GWmalloc((unsigned long)s)
#define farmalloc(s)	GWmalloc(s)
#define calloc(n,s)	GWcalloc((unsigned long)n,(unsigned long)s)
#define farcalloc(n,s)	GWcalloc(n, s)
#define realloc(p,s)	GWrealloc((void far *)p,(unsigned long)s)
#define farrealloc(p,s)	GWrealloc(p, s)
#define free(p)		GWfree((void far *)p)
#define farfree(p)	GWfree(p)

#else

#include <malloc.h>

#define setheap(b, e)
#define heapstatus(d)	fprintf(stderr, "Using system heap; no status available\n")
#define heap_avail()
#define heap_used()

#endif

#endif


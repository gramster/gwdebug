/*
 * heap.c - A portable dynamic memory heap manager
 *
 * Written by Graham Wheeler, January 1995.
 * (c) 1995, All Rights Reserved
 *
 * This is ugly, but working code. A lot of the ugliness comes
 * from lots of pointer casting and converting pointers to offsets
 * from the heap base and vice versa. Possibly it could be cleaner
 * if the free list held pointers rather than offsets; I used
 * offsets as they are in principle relocatable without any work.
 *
 * To use this code:
 *   - replace #includes of malloc.h with an #include of "heap.h".
 *	Actually you can leave the malloc.h includes in if you
 *	want, but you must include heap.h in any file that uses
 *	heap-related functions.
 *
 *   - precede the #include with a #define of LOCAL_HEAP. If you
 *	don't, the normal C heap will be used instead. You should
 *	preferably define this in the CFLAGS in the makefile,
 *	especially if you plan to use gwdebug as well.
 *
 *   - before using any heap functions, call setheap to initialise
 *	the heap to a specific block of memory
 *
 *   - this code then replaces the following standard C routines:
 *
 *		void *malloc(size_t n)
 *		void *calloc(size_t n, size_t s)
 *		void *realloc(void *p, size_t n)
 *		void free(void *p)
 *
 *	and adds the following:
 *
 *		void setheap(char *base, unsigned long extent)
 *		void heapstatus(int detailed)
 *		unsigned long heap_avail()
 *		unsigned long heap_used()
 *
 *	NOTE THAT THIS IS A FAR HEAP. ALL POINTERS RETURNED ARE FAR.
 *	This is necessary as we don't know what segment the heap
 *	will be in. It need not be the BSS segment if the heap
 *	is a huge array in the small model, for example.
 *
 *   - wherever you want information about the heap, call heapstatus.
 *	Passing it a nonzero argument of will cause it to dump out
 *	the entire heap. In either case it will produce some information
 *	about the current use and fragmentation of the heap, and the
 *	number of allocations and frees that have occurred.
 *
 * The overhead associated with each allocation is four bytes.
 * Odd valued sizes are rounded up to an even value. Allocations
 * of less than four bytes are rounded up to four. Thus the overhead
 * is:
 *
 *    Requested     Allocated
 *   =========================
 *        1            8
 *        2            8
 *        3            8
 *        4            8
 *        5            10
 *        6            10
 *        7            12
 *        8            12
 *        9            14 etc
 *
 * Other than this, fragmentation over time may prevent successful
 * allocations. Fragmentation under DOS is a fairly insoluble problem;
 * this code could easily be modified to do `best fit' allocations
 * which may help.
 *
 * Implementation notes:
 * ---------------------
 * The implementation is fairly straightforward. Free blocks of
 * memory are held on a singly linked list (the nodes of the list
 * are held at the start of the free blocks; as a node is 8 bytes
 * this is why all allocations are at least 8 bytes - else they
 * couldn't be freed!) The nodes hold the size of the free block
 * (*excluding the 8 bytes for the node itself!*) and the offset
 * from the start of the heap to the next node. At first there is
 * just one node, reprsenting the whole heap.
 *
 * When an allocation request is made, the size is adjusted to
 * be even and be able to hold a free list node. The size is
 * then increased by 4 bytes (a long); this is so that the size
 * of the allocated block can be stored in the block itself.
 *
 * The list of free nodes is then traversed, looking for a block
 * large enough to satisy the request. Note that the requested
 * size is compared against the size stored in the node - this
 * is because after allocating a block a new free list node must be
 * made at the end of the block for the remaining memory (so the
 * number of free list nodes remains unchanged). There is
 * one special case: if the request is for eight bytes and the
 * size stored in the node is zero, we can allocate this node
 * and remove it entirely from the free list. It is worth handling
 * this case as allocations of size eight may be quite common
 * (any malloc of 4 or less bytes).
 *
 * Disregarding the special case, once a block is found, the size
 * of the request is stored in the first four bytes, a new free
 * list node is made at the end of the block and linked into the
 * list at the position of the one it replaces; its size is set
 * to the remaining memory size of the block. Coalescing is then
 * done on this node (see later), and the user is returned the
 * address of the first byte of memory after the saved size.
 * Obviously if no block is found, the request fails; unlike
 * standard malloc, this is reported to stderr, and NULL is returned.
 * The heap is also checked for consistency at some points; if
 * a problem is detected the program will report this and abort.
 *
 * Freeing a block involves converting it into a free list node,
 * linking it into the chain, and coalescing.
 *
 * Coalescing involves merging adjacent free list nodes into single
 * nodes, to reduce fragmentation and speed up allocations.
 *
 * If you compile this code with CHK_USE defined, you will also
 * get shown the effective utilisation of memory - namely the
 * number of bytes that have been allocated from the user's point
 * of view. You can compare this with the actual number of bytes
 * allocated. Note, however, that the CHK_USE option adds another
 * four bytes of overhead to each allocation.
 *
 * Last modified:
 * Contact: gram@aztec.co.za
 */

#ifdef LOCAL_HEAP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "heap.h"

typedef	unsigned long word;
typedef char huge *heap_ptr;
typedef word huge *word_ptr;

typedef struct
{
	word size;
	word next;
} free_list_node_t;

typedef free_list_node_t huge *free_ptr;

static heap_ptr heap_base = NULL;	/* Pointer to heap	*/
static word	heap_size;		/* Size of heap		*/
static word	heap_front;		/* Head of free list	*/
static long	heap_allocs;		/* Count of allocs	*/
static long	heap_deletes;		/* Count of frees	*/
#ifdef CHK_USE
static word	heap_user;		/* Actual user mem	*/
#endif

/* Note - if the heap is freed call this with a NULL arg for base for
   safety. Then any attempts to use the heap will cause an assert. */

void GWsetheap(char far *base, unsigned long extent)
{
	free_ptr f;
	heap_base = (heap_ptr)base;
	heap_size = (word)(extent - sizeof(free_list_node_t));
	heap_front = (word)0;
	heap_allocs = 0l;
	heap_deletes = 0l;
#ifdef CHK_USE
	heap_user = (word)0;
#endif
	f = (free_ptr)base;
	f->next = (word)0;
	f->size = heap_size;
}

unsigned long GWheap_avail(void)
{
	unsigned long t = 0l;
	free_ptr f;
	assert(heap_base);
	f = (free_ptr)&heap_base[heap_front];
	for (;;)
	{
		t += f->size;
		if (f->next == 0) break;
		assert(f->next < heap_size);
		f = (free_ptr)&heap_base[f->next];
	}
	return t;
}

unsigned long GWheap_used(void)
{
	return heap_size - GWheap_avail();
}

static void coalesce_heap(word off)
{
	free_ptr f, nf;
	assert(heap_base);
	f = (free_ptr)(heap_base+off);
	while ((off + sizeof(free_list_node_t) + f->size) == f->next)
	{
		nf = (free_ptr)&heap_base[f->next];
		f->next = nf->next;
		f->size += nf->size + sizeof(free_list_node_t);
	}
}

void far *GWmalloc(unsigned long sz)
{
	free_ptr f, prev = NULL, newf = NULL;
	word size, off;
	/* fprintf(stderr, "In GWmalloc(%lu)\n", sz); */
	assert(heap_base);
	/* We need to store the size of the block as well */
	size = sz + sizeof(word);
#ifdef CHK_USE
	size += sizeof(word);
#endif
	/* Make the size an even number and be sure this is
		big enough to hold a free list node for when
		it is freed */
	if (size < sizeof(free_list_node_t))
		size = sizeof(free_list_node_t);
	else if (size & 1) size++;
	/* Find a block in free list large enough to satisfy request */
	f = (free_ptr)&heap_base[heap_front];
	/*fprintf(stderr, "Checking free list node of size %lu (next %lu) for req of %lu\n",
		f->size, f->next, size); */
	while (size > f->size)
	{
		if (f->next == 0)
		{
			fprintf(stderr,"WARNING - malloc failure (%lu bytes)\n",
				(unsigned long)size);
#ifdef CHK_USE
			GWheapstatus(1);
#endif
			return NULL;
		}
		assert(f->next < heap_size);
		/* Special case - if the request is for eight bytes,
			and the size of this node is zero, the node
			can be allocated and removed from the free
			list. */
		if (size==sizeof(free_list_node_t) && f->size==0)
		{
			/* reuse the old next free list node */
			fprintf(stderr, "Special case!\n");
			newf = (free_ptr)(heap_base + f->next);
			break;
		}
		/* another special case - if the node satisfies the
			request exactly it can be removed from the
			free list */
		if (size == (f->size + sizeof(free_list_node_t)))
		{
			newf = (free_ptr)(heap_base + f->next);
			break;
		}
		prev = f;
		f = (free_ptr)&heap_base[f->next];
		/*fprintf(stderr, "Checking free list node of size %lu (next %lu) for req of %lu\n",
			f->size, f->next, size);*/
	}
	/* Got the block; create a new free list record */
	heap_allocs++;
#ifdef CHK_USE
	heap_user += sz;
#endif
	if (newf == NULL) /* not special case? make new free list node */
	{
		newf = (free_ptr)(((heap_ptr)f) + size);
		newf->next = f->next;		/* Link into chain	*/
		newf->size = f->size - size;	/* Remaining size	*/
	}
	off = (long)(((heap_ptr)newf) - heap_base);
	if (prev) prev->next = off;
	else heap_front = off;
	coalesce_heap(off);		/* Join adjacent list nodes	*/
	f->size = size;			/* Save the size of alloc	*/
#ifdef CHK_USE
	f->next = sz;			/* Save the size of request	*/
	return (void far *)(((heap_ptr)f)+2*sizeof(word));
#else
	return (void far *)(&f->next);	/* Return the address		*/
#endif
}

void GWmemset(heap_ptr dest, int v, unsigned long n)
{
	while (n--)
		dest[n] = (char)v;
}

void far *GWcalloc(unsigned long nitems, unsigned long size)
{
	void far *rtn = GWmalloc(nitems * size);
	if (rtn)
		GWmemset((heap_ptr)rtn, 0, nitems*size);
	return rtn;
}

void GWmemcpy(heap_ptr dest, heap_ptr src, unsigned long n)
{
	while (n--)
		*dest++ = *src++;
}

void far *GWrealloc(void far *p, unsigned long n)
{
	void far *rtn = GWmalloc(n);
	if (p)
	{
		GWmemcpy((heap_ptr )rtn, (heap_ptr )p, n);
		free(p);
	}
	return rtn;
}

void GWfree(void far *p)
{
	word size, off;
	free_ptr newf;
	assert(heap_base);
	/* Move pointer back to real start */
	p = (void far *) ( ((heap_ptr)p) - sizeof(word) );
#ifdef CHK_USE
	heap_user -= *((word_ptr)p);
	p = (void far *) ( ((heap_ptr)p) - sizeof(word) );
#endif
	/* Get size and offset */
	size = *((word_ptr)p);
	off = ((heap_ptr)p)-heap_base;
	/* Make it a free list node */
	newf = (free_ptr)p;
	newf->size = size - sizeof(free_list_node_t);
	heap_deletes++;
	// find the preceding free block, if any
	if (off > heap_front)
	{
		free_ptr f = (free_ptr)&heap_base[heap_front];
		for (;;)
		{
			if (f->next == 0 || f->next > off) break;
			f = (free_ptr)&heap_base[f->next];
		}
		newf->next = f->next;
		f->next = off;
		coalesce_heap(off);
		coalesce_heap(((heap_ptr)f) - heap_base);
	}
	else
	{
		newf->next = heap_front;
		heap_front = off;
		coalesce_heap(off);
	}
}

static void show_allocated(word_ptr now, word_ptr end)
{
	/* show allocated blocks in range, if any */
	while (now < end)
	{
		long sz = (long)(*now - (word)sizeof(word));
		long off = (long)(((heap_ptr)now) - heap_base);
#ifdef CHK_USE
		fprintf(stderr, "\tAllocated block of %ld bytes (req %ld) at offset %ld\n",
			sz, (long)now[1], off);
#else
		fprintf(stderr, "\tAllocated block of %ld bytes at offset %ld\n",
			sz, off);
#endif
		assert(sz>0 && sz <= (heap_size - off));
		now = (word_ptr)(((heap_ptr)now) + (*now));
	}
}

void GWheapstatus(int detailed)
{
	assert(heap_base);
	fprintf(stderr, "HEAP STATISTICS AND INFO\n");
	fprintf(stderr, "Heap size is %ld bytes\n", (long)heap_size);
	if (detailed)		      
	{
		free_ptr f;
		word n = 0;
		long off = heap_front;
		fprintf(stderr, "Heap Dump:\n");
		f = (free_ptr)(&heap_base[heap_front]);
		show_allocated((word_ptr)heap_base,
			       (word_ptr)f);
		for (;;)
		{
			word *ab;
			fprintf(stderr, "Free list node at offset %lu, size %lu, next %lu\n",
				off, (long)f->size, (long)f->next);
			n++;
			show_allocated((word_ptr)
					(((heap_ptr)f)+f->size + sizeof(free_list_node_t)),
				       (word_ptr)(heap_base+f->next));
			if (f->next ==0)
				break;
			assert(f->next > (off + f->size));
			off = f->next;
			f = (free_ptr)&heap_base[f->next];
		}
		fprintf(stderr, "Fragmentation: %d%%\n",
			(int) (n / (heap_size / (100*sizeof(free_list_node_t)))));
	}
	fprintf(stderr, "%ld / %ld bytes of heap used (%ld%%)\n",
		GWheap_used(), (long)heap_size,
		(long)((GWheap_used()*100l)/heap_size));
	fprintf(stderr, "%ld allocations and %ld deletes\n",
		heap_allocs, heap_deletes);
#ifdef CHK_USE
	fprintf(stderr, "Real utilisation: %ld", (long)heap_user);
	if (GWheap_used())
	{
		fprintf(stderr, " (%ld%%)\n", 100l*heap_user/GWheap_used());
		fprintf(stderr, "Corrected for CHK_USE extra overhead: %ld%%",
			100l*heap_user/
			  (GWheap_used() - sizeof(word) * (heap_allocs-heap_deletes)));
	}
	fprintf(stderr, "\n");
#endif
	fprintf(stderr, "\n");
}

#endif


/* run some tests on the heap */

#include <stdlib.h>
#include <stdio.h>
#if __MSDOS__
#include <conio.h>
#endif
#include <assert.h>

#include "heap.h"
#include "gwdebug.h" /* Not really necessary as this should all be
			well-behaved */

#if 0

/* this one does lots of allocs, then frees, then allocs, then
	frees. Good for checking overhead and fragmentation */

#if defined(__TINY__) || defined(__SMALL__) || defined(__MEDIUM__)
#define HEAP_SIZE	30000
#define MAX_PTRS	250
#define MAX_ALLOC_SZ	128
#else
#define HEAP_SIZE	220000
#define MAX_PTRS	10
#define MAX_ALLOC_SZ	70000
#endif

char huge myheap[HEAP_SIZE];
void *ptrs[MAX_PTRS];

int main()
{
	int i;
	srand(0);
	setheap(myheap, HEAP_SIZE);
	heapstatus(1);
	while (getchar() != '\n');
	ptrs[MAX_PTRS-2] = malloc(random(MAX_ALLOC_SZ)+1);
	ptrs[MAX_PTRS-3] = malloc(random(MAX_ALLOC_SZ)+1);
	ptrs[MAX_PTRS-1] = malloc(random(MAX_ALLOC_SZ)+1);
	heapstatus(1);
	while (getchar() != '\n');
	free(ptrs[MAX_PTRS-3]);
	heapstatus(1);
	while (getchar() != '\n');
	for (i = MAX_PTRS-2; i--;)
		ptrs[i] = malloc(random(MAX_ALLOC_SZ)+1);
	heapstatus(1);
	while (getchar() != '\n');
	for (i = MAX_PTRS/2; i;)
	{
		int j = random(MAX_PTRS);
		if (ptrs[j])
		{
			free(ptrs[j]);
			ptrs[j] = NULL;
			i--;
		}
	}
	heapstatus(1);
	while (getchar() != '\n');
	for (i = MAX_PTRS; i--;)
	{
		if (ptrs[i] == NULL)
			ptrs[i] = malloc(random(MAX_ALLOC_SZ)+1);
	}
	heapstatus(1);
	while (getchar() != '\n');
	/* Leave a few memory leaks just for fun */
	for (i = MAX_PTRS-10; i--;)
	{
		if (ptrs[i])
		{
			free(ptrs[i]);
			ptrs[i] = NULL;
		}
	}
	heapstatus(1);
	while (getchar() != '\n');
	return 0;
}

#else

/* this just keeps doing random stuff until a key is pressed */

#define HEAP_SIZE	220000
#define MAX_PTRS	10
#define MAX_ALLOC_SZ	70000

char huge myheap[HEAP_SIZE];
void far *ptrs[MAX_PTRS];

unsigned long sizes[10] =
{
	1l,	2l,	4l,	8l,
	16l,	128l,	1024l,	4096l,
	32768l,	65536l
};

int main()
{
	int i;
	srand(0);
	assert(myheap);
	setheap(myheap, HEAP_SIZE);
	for (i=MAX_PTRS; i--; )
		ptrs[i] = NULL;
	while (!kbhit())
	{
		int n = random(MAX_PTRS);
		heapstatus(1);
		if (ptrs[n])
		{
			farfree(ptrs[n]);
			ptrs[n] = NULL;
		}
		else
		{
			int sz = random(10);
			fprintf(stderr, "Mallocing %lu bytes\n",
				sizes[sz]);
			ptrs[n] = farmalloc(sizes[sz]);
			if (ptrs[n]==NULL) break;
		}
	}
	heapstatus(1);
	return 0;
}

#endif


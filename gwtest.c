/* Sample program illustrating some of the features of the library */

#include <sys/stat.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#ifdef __MSDOS__
#include <io.h>
#endif
#include <fcntl.h>
#include <assert.h>

#include "heap.h"
#include "gwdebug.h"

#ifdef LOCAL_HEAP
typedef void far *heap_ptr;
#define MALLOC	farmalloc
#define FREE	farfree
#else
typedef void *heap_ptr;
#define MALLOC	malloc
#define FREE	free
#endif

void leakTest(void)
{
    (void)MALLOC(10);
}

void badFreeTest(void)
{
    FREE((void *)26304);
}

void doubleFreeTest(void)
{
    heap_ptr p = MALLOC(10);
    FREE(p);
    FREE(p);
}

void overrunTest1(void)
{
    heap_ptr p = MALLOC(10);
#ifdef LOCAL_HEAP
    char far *d = (char far *)p,
    	 *s = "Hello World";
    while ((*d++ = *s++) != 0);
#else
    char *d = (char *)p,
    	 *s = "Hello World";
    while ((*d++ = *s++) != 0);
#endif
    FREE(p);
}

void overrunTest2(void)
{
#if defined(LOCAL_HEAP) && (defined(__SMALL__) || defined(__COMPACT__) || defined(__TINY__))
    printf("This test is skipped with local heap\n");
#else
    heap_ptr *p = MALLOC(10);
    strcpy((char *)p, "Hello world");
    FREE(p);
#endif
}

void overrunTest3(void)
{
    char dest[8];
    strcpy(dest,"Hello world");
    strncpy(dest,"Hello world",8);
    strncpy(dest,"Hello world",9);
}

void fileTest(void)
{
	FILE *fp;
	int h = open("junk", O_CREAT, S_IWRITE), h2;
	close(h);
	h2 = dup(h);
	close(h);
	close(h2);
	fp = fopen("leak", "w");
	(void)fp;
}

void initTest(void)
{
    /* the tests here marked ** will only succeed is junk[] has no
	zero bytes in it after being created on the stack; luck of the
	draw, unfortunately */
    heap_ptr *p = MALLOC(10);
    char junk[10];
/**/(void)strlen(junk);
    (void)strlen((char *)p);
    (void)strcpy(junk, (char *)p);
    (void)memcpy(junk, (char *)p, 10);
/**/(void)strcpy((char *)p, junk);
    (void)memcpy((char *)p, junk, 10); /* this is undetectable */
    FREE(p);
}

#ifdef LOCAL_HEAP
static char heap[24000];
#endif

void main(void)
{
    int ch = 0;
#ifdef LOCAL_HEAP
    setheap(heap, 24000);
#endif
    while (ch != 'q')
    {
    	printf("Enter:\n\tq - to quit\n\t1 - for leak test\n");
    	printf("\t2 - for overrun test 1\n\t3 - for overrun test 2\n");
    	printf("\t4 - for overrun test 3\n");
    	printf("\t5 - for bad free\n\t6 - for double free\n");
    	printf("\t7 - for file test\n\t8 - for uninit test\n");
    	ch=getchar();
    	switch(ch)
    	{
    	case '1': leakTest();		break;
    	case '2': overrunTest1();	break;
    	case '3': overrunTest2();	break;
    	case '4': overrunTest3();	break;
    	case '5': badFreeTest();	break;
    	case '6': doubleFreeTest(); 	break;
    	case '7': fileTest(); 		break;
    	case '8': initTest(); 		break;
    	}
    	while (getchar() != '\n');
    }
#ifdef LOCAL_HEAP
    heapstatus(1);
#endif
}




/*
 * A drop-in debugging library for C v1.1
 *
 * (c) 1995 by Graham Wheeler, All Rights Reserved
 *
 * This library provides drop-in wrappers for the following
 * common C library routines:
 *
 * Memory allocation routines:
 *	malloc(), calloc(), free(), realloc()
 *
 * String and memory routines:
 *	strcpy(), strncpy(), memcpy(), memset(), memmove(),
 *	strcmp(), strncmp(), memcmp(), strlen(), strdup(),
 *	strstr(), strpbrk(), stpcpy(), strcat(), strncat(),
 *	strchr(), strrchr(), strspn(), strcspn()
 *
 * File I/O routines:
 *	open(), close(), dup(), fopen(), fclose(),
 *	read(), fread(), fgets()
 * 
 * The library catches:
 *
 * - failed mallocs/callocs/reallocs and bad frees
 * - under DOS, allocation from the near heap and returning
 *	to the far heap, or vice-versa
 * - memory and file leaks
 * - some bad parameters passed to these routines
 *
 * In addition:
 *
 * - some overruns of dynamic, global and auto buffers are caught,
 *	reported and recovered from (memcpy, memset,
 *	strcpy, strncpy, stpcpy, read, fread, fgets)
 * - some potential overruns are warned about
 * - attempts to memcpy/strcpy/etc uninitialised memory is reported
 * - all overruns of dynamic buffers are reported when the memory
 *	is freed. Overruns of less than 5 bytes will not clobber the
 *	heap.
 *
 * To use the library, link this file with your application.
 * Add a line `#include "gwdebug.h" to each source file of
 * your application, AFTER any other #includes.
 * Then compile with GW_DEBUG defined to use the library.
 *
 * You can define DEBUG_LOG to be a log file name; else
 * standard error is used.
 *
 * TODO:
 * Test all functions not yet done in mytest.c.
 */

#ifdef GW_DEBUG

#include <stdlib.h>
#include <stdio.h>
#ifdef __MSDOS__
#include <io.h>
#include <alloc.h>
#endif
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#ifdef LOCAL_HEAP
#include "heap.h" /* definitions for replacement heap for embedded code */
#endif

static FILE *logfile = NULL;
static char *store_name(char *name);
static void my_initialise(void);
static void my_report(void);

/*******************************/
/* Memory allocation debugging */
/*******************************/

#define MAGIC			(0x24681357L)

#define ISFAR	1

/* Freeing a blk_info header will usually result in the memory
   manager using the first few bytes to store the block on the
   free list. The fields at the start of blk_info have been
   chosen to be the ones we don't mind being clobbered.
*/

typedef struct
{
    long	magic;
    short	flags;	/* only used for far allocs at the moment */
    void far   *next;
    long	nbytes;
    char       *file;
    short	line;
} blk_info;

#define GET_BLKP(p, o)		(( (blk_info huge *)(p) ) + (o))
#define GET_BLK(p)		(blk_info far *)GET_BLKP(p, -1)
#define GET_DATA(p)		((void far *)GET_BLKP(p, 1))
#define BUMPSIZE(n)		((n)+sizeof(blk_info) + sizeof(long))
/*#define GET_BLK(p)		(blk_info far *)(((char far *)p)-sizeof(blk_info))*/
#define SET_ENDMAGIC(p,n)	*(long far *)( ((char huge *)p)+n ) = MAGIC
#define TST_ENDMAGIC(p,n)	(*(long far *)( ((char huge *)p)+n ) == MAGIC)

void my_free(void *p, char *f, int l);

#if __MSDOS__
void my_ffree(void far *p, char *f, int l);
#endif

static void far *heap_list_head = NULL;

void my_memory_report(int is_last)
{
    void far *p;
    /* walk dat list... */
    p = heap_list_head;
    if (p)
    {
    	fprintf(logfile,is_last ? "MEMORY LEAKS:\n" : "Allocated Memory Blocks:\n");
    	while (p)
    	{
    	    blk_info far *bp = GET_BLK(p);
#if __MSDOS__
    	    fprintf(logfile,"\t%s Size %8ld File %16s Line %d\n",
    		    (bp->flags&ISFAR)?"(Far) ":"Near",
		    bp->nbytes, bp->file, bp->line);
#else
    	    fprintf(logfile,"\tSize %8ld File %16s Line %d\n",
    		    bp->nbytes, bp->file, bp->line);
#endif
	    p = bp->next;
	}
    }
}

static void log_alloc(void far *rtn, unsigned long n,
	char *f, int l, unsigned flags)
{
    char *savedname;
    blk_info far *bp = (blk_info far *)rtn;
    assert(rtn);
    if (!logfile) my_initialise();
    /* Get the pointer that is returned to the user */
    rtn = GET_DATA(rtn);
    /* Save the size information */
    bp->nbytes = n;
    /* Prepend to front of heap list */
    bp->next = heap_list_head;
    heap_list_head = rtn;
    /* Save file name and line number, and put in bounding magic markers */
    bp->file = store_name(f);
    bp->line = l;
    bp->magic = MAGIC;
    bp->flags = flags;
    SET_ENDMAGIC(rtn, n);
}

void *my_calloc(unsigned n, char *f, int l)
{
    /* Allocate the memory with space enough for our info */
    void *rtn = calloc(BUMPSIZE(n), 1);
    log_alloc((void far *)rtn, (unsigned long)n, f, l, 0);
#if defined(__TINY__) || defined(__SMALL__) || defined(__MEDIUM__)
    return ((char *)rtn)+sizeof(blk_info);
#else
    return GET_DATA(rtn);
#endif
}

void *my_malloc(unsigned n, char *f, int l)
{
    void *rtn = my_calloc(n,f,l);
    if (n >= sizeof(long))
        *((unsigned long *)rtn) = MAGIC; /* mark as unitialised */
    return rtn;
}

static int log_free(void huge *p, char *f, int l, int isfar)
{
    void far *tmp, far *last = NULL;
    blk_info far *bp;
    if (!logfile) my_initialise();
    /* Search the list for the block */
    tmp = heap_list_head;
    while (tmp)
    {
    	bp = GET_BLK(tmp);
#if __MSDOS__
    	if (((void huge *)tmp) == ((void huge *)p))
	    break;
#else
    	if (tmp == p) break;
#endif
    	if (bp->magic != MAGIC) goto error;
    	else
    	{
    	    last = tmp;
    	    tmp = bp->next;
    	}
    }
    if (tmp)
    {
    	if (!TST_ENDMAGIC(p, bp->nbytes))
    	    fprintf(logfile,"Block of size %ld allocated at %s, line %d, freed at %s, line %d, has been overrun\n",
    			bp->nbytes, bp->file, bp->line, f, l);
	if (((bp->flags&ISFAR)!=0) ^ isfar)
    	    fprintf(logfile,"Wrong version of free called for this block!\n");
    	/* Unlink from chain */
    	if (last==NULL)
    	    heap_list_head = bp->next;
    	else
    	    (GET_BLK(last))->next = bp->next;
    	/* save who freed */
    	bp->file = store_name(f);
    	bp->line = l;
    	/* trash contents */
	if (bp->nbytes >= sizeof(long))
            *((unsigned long far *)GET_DATA(bp)) = MAGIC;
	return (((bp->flags&ISFAR)!=0) ^ isfar);
    }
    else bp = GET_BLK(p);
error:
    fprintf(logfile,"Bad call to free from file %s, line %d\n",f,l);
/*#if __MSDOS__*/
    /* Causes seg violation on UNIX */
    if (bp && bp->magic==MAGIC)
    	fprintf(logfile,"Possibly freed before at %s, line %d, size %ld\n",
    		bp->file, bp->line, bp->nbytes);
/*#endif*/
    return -1;
}

void my_free(void *p, char *f, int l)
{
    if (log_free((void far *)p, f, l, 0) == 0)
#if defined(__TINY__) || defined(__SMALL__) || defined(__MEDIUM__)
	free(((char *)p) - sizeof(blk_info));
#else
        free(GET_BLK(p));
#endif
}

#if __MSDOS__
void far *my_fcalloc(unsigned long n, char *f, int l)
{
    /* Allocate the memory with space enough for our info */
    void far *rtn = farcalloc(BUMPSIZE(n), 1l);
    log_alloc(rtn, n, f, l, ISFAR);
    return GET_DATA(rtn);
}

void far *my_fmalloc(unsigned long n, char *f, int l)
{
    void far *rtn = my_fcalloc(n,f,l);
    if (n >= sizeof(long))
	*((unsigned long far *)rtn) = MAGIC; /* mark as unitialised */
    return rtn;
}

void my_ffree(void far *p, char *f, int l)
{
    if (log_free(p, f, l, 1) == 0)
	farfree(GET_BLK(p));
}
#endif

static blk_info far *my_find_block(void far *p)
{
#ifdef __MSDOS__
    blk_info far *bp = GET_BLK(p);
    return (bp->magic==MAGIC) ? bp : NULL;
#else
    blk_info *bp = GET_BLK(p);
    void *tmp = heap_list_head;
    /* Search the list for the block. We don't just check for the magic
    	number under UNIX as this can cause a segmentation violation */
    while (tmp)
    {
    	if ((void huge *)tmp == (void huge *)p)
	    return GET_BLK(p);
    	tmp = GET_BLK(tmp)->next;
    }
    return NULL;
#endif
}

static int my_sizehint(void far *p, int size)
{
    void *tmp;
    blk_info far *bp;
    if (p == NULL) return -1;
    bp = my_find_block(p);
    if (bp)
    {
    	assert(size == sizeof(char *));
    	return (int)(bp->nbytes);
    }
    else if (size == sizeof(char *))
    	return -1; /* no clues */
    else return size;
}

/*****************************/
/* String function debugging */
/*****************************/

/* flags for additional checks */

#define INIT1	1	/* arg s1 must be initialised */
#define INIT2	2	/* arg s2 must be initialised */
#define NULLT	4	/* must be initialised with NUL-terminated strings */

/* Validate a single pointer arg */

static int my_init_check(char far *p, int space, int nul)
{
    blk_info far *bp = my_find_block(p);
    if (bp)
    {
	if (bp->nbytes>=sizeof(long) && *((unsigned long *)p)==MAGIC)
	    return 0;
	return 1;
    }
    else if (space >= 0 && nul)
    {
	int i;
	for (i = 0; i < space; i++)
	    if (p[i] == 0)
		return 1;
	return 0;
    }
    return 1; /* we don't know, so we assume OK */
}

static int my_validate1(char *name, char *s, int space, char *f, int l,
	unsigned flags)
{
    if (s==NULL)
    {
    	fprintf(logfile,"%s(NULL) at file %s, line %d\n", name, f, l);
        return -1;
    }
    else if (flags & INIT1)
    {
	if (my_init_check(s, space, (flags&NULLT)!=0) == 0)
	{
    	    fprintf(logfile,"%s(uninitialised) at file %s, line %d\n", name, f, l);
            return -1;
	}
    }
    return 0;
}

/* Validate a pair of pointer args */

static int my_validate2(char *name, char *s1, int siz1, char *s2,
	int siz2, char *f, int l, unsigned flags)
{
    /* Just validate arguments */
    if (s1==NULL && s2==NULL)
    	fprintf(logfile,"%s(NULL,NULL) at file %s, line %d\n",
    		name, f, l);
    else if (s1==NULL)
    	fprintf(logfile,"%s(NULL,...) at file %s, line %d\n", name, f, l);
    else if (s2==NULL)
    	fprintf(logfile,"%s(...,NULL) at file %s, line %d\n", name, f, l);
    else if ((flags&INIT1) && my_init_check(s1, siz1, (flags&NULLT)!=0) == 0)
        fprintf(logfile,"%s(invalid,...) at file %s, line %d\n", name, f, l);
    else if ((flags&INIT2) && my_init_check(s2, siz2, (flags&NULLT)!=0) == 0)
        fprintf(logfile,"%s(...,invalid) at file %s, line %d\n", name, f, l);
    else return 0;
    return -1;
}

/* Handler for strcpy, stpcpy, strncpy, strcat, strncat, memcpy,
	and memmove */

char *my_copy(char *d, int space_avail, int limit, char *s,
	int sspace, int flag, char *f, int l, unsigned strflags)
{
    char *rtn = d;
    int space_needed, i, dlen;
    if (!logfile) my_initialise();
    space_avail = my_sizehint(d, space_avail);
    sspace = my_sizehint(s, sspace);
    if (my_validate2(strflags?"String copy":"Mem copy", d, space_avail,
		s, sspace, f,l,INIT2|strflags))
	return d;
    /* how much space do we need? */
    space_needed = limit ? limit : (strflags ? (strlen(s)+1) : space_avail);
    /* Are we cat'ing? If so, how long is the existing stuff? */
    dlen = (flag&4) ? strlen(d) : 0;
    /* space enuf? */
    limit = space_needed;
    if (space_avail >= 0 && space_needed > (space_avail-dlen))
    {
    	fprintf(logfile,"String/memory copy overrun at file %s, line %d - truncating\n\ttarget space %d, source length %d\n",
    		f, l, (space_avail-dlen), space_needed);
    	limit = space_avail-dlen;
    }
    else if (space_avail >= 0 && sspace >= 0 && sspace > (space_avail-dlen))
    	fprintf(logfile,"Potential string/memory copy overrun at file %s, line %d\n\ttarget space %d, source size %d\n",
    		f, l, (space_avail-dlen), sspace);
    d += dlen;
    if (s<d && (s+limit)>=d && (flag&1)==0) /* forward copy would clobber source */
    	fprintf(logfile,"String/memory copy clobber in %s, line %d\n", f, l);
    memmove(d,s,limit);
    return rtn + ( (flag & 2) ? (limit-1) : 0); /* hax for stpcpy */
}

char *my_memcpy(char *d, int space_avail, int limit, char *s,
	int sspace, int flag, char *f, int l)
{
    return my_copy(d, space_avail, limit, s, sspace, flag, f, l, 0);
}

char *my_strcpy(char *d, int space_avail, int limit, char *s,
	int sspace, int flag, char *f, int l)
{
    return my_copy(d, space_avail, limit, s, sspace, flag, f, l, NULLT);
}

char *my_memset(char *d, int space_avail, int limit, char c, char *f, int l)
{
    char *rtn = d;
    int space_needed, i;
    if (!logfile) my_initialise();
    /* how much space do we have? */
    space_avail = my_sizehint(d, space_avail);
    if (my_validate1("memset", d, space_avail, f, l, 0)) goto done;
    /* space enuf? */
    if (space_avail >= 0 && limit > space_avail)
    {
    	fprintf(logfile,"memset overrun at file %s, line %d - truncating\n\ttarget length %d, source length %d\n",
    		f, l, space_avail, limit);
    	limit = space_avail;
    }
    memset(d,limit,c);
done:
    return rtn;
}

static int my_compare(char *s1, int siz1, char *s2, int siz2, int n,
	int flag, char *f, int l, unsigned strflags)
{
    if (!logfile) my_initialise();
    /* Just validate arguments */
    siz1 = my_sizehint(s1, siz1);
    siz2 = my_sizehint(s2, siz2);
    if (my_validate2("memcmp", s1, siz1, s2, siz2, f, l, INIT1|INIT2|strflags))
    	return ((s1?1:0)-(s2?1:0));
    if (n==0) return strcmp(s1,s2);
    else if (flag) return strncmp(s1,s2,n);
    else return memcmp(s1,s2,n);
}

int my_memcmp(char *s1, int siz1, char *s2, int siz2, int n, int flag, char *f, int l)
{
    return my_compare(s1, siz1, s2, siz2, n, flag, f, l, 0);
}

int my_strcmp(char *s1, int siz1, char *s2, int siz2, int n, int flag, char *f, int l)
{
    return my_compare(s1, siz1, s2, siz2, n, flag, f, l, NULLT);
}

int my_strlen(char *s, int siz, char *f, int l)
{
    if (!logfile) my_initialise();
    siz = my_sizehint(s, siz);
    return my_validate1("strlen", s, siz, f, l, INIT1|NULLT) ? 0 : strlen(s);
}

char *my_strdup(char *s, int siz, char *f, int l)
{
    if (!logfile) my_initialise();
    siz = my_sizehint(s, siz);
    if (my_validate1("strdup", s, siz, f, l, INIT1|NULLT)==0)
    {
    	int n = strlen(s)+1;
    	char *rtn = my_calloc(n,f,l);
    	assert(rtn);
    	my_memcpy(rtn, n, n, s, n, 0, f, l);
    	return rtn;
    }
    return NULL;
}

char *my_strstr(char *s1, int siz1, char *s2, int siz2, char *f, int l)
{
    if (!logfile) my_initialise();
    siz1 = my_sizehint(s1, siz1);
    siz2 = my_sizehint(s2, siz2);
    return my_validate2("strstr", s1, siz1, s2, siz2, f, l, INIT1|INIT2|NULLT)
	? NULL : strstr(s1,s2);
}

char *my_strpbrk(char *s1, int siz1, char *s2, int siz2, char *f, int l)
{
    if (!logfile) my_initialise();
    siz1 = my_sizehint(s1, siz1);
    siz2 = my_sizehint(s2, siz2);
    return my_validate2("strpbrk", s1, siz1, s2, siz2, f, l, INIT1|INIT2|NULLT)
	? NULL : strpbrk(s1,s2);
}

char *my_strchr(char *s, int siz, int c, char *f, int l)
{
    if (!logfile) my_initialise();
    siz = my_sizehint(s, siz);
    return my_validate1("strchr", s, siz, f, l, INIT1|NULLT)
	? NULL : strchr(s,c);
}

char *my_strrchr(char *s, int siz, int c, char *f, int l)
{
    if (!logfile) my_initialise();
    siz = my_sizehint(s, siz);
    return my_validate1("strrchr", s, siz, f, l, INIT1|NULLT)
	? NULL : strrchr(s,c);
}

int my_strspn(char *s1, int siz1, char *s2, int siz2, char *f, int l)
{
    if (!logfile) my_initialise();
    siz1 = my_sizehint(s1, siz1);
    siz2 = my_sizehint(s2, siz2);
    return my_validate2("strspn", s1, siz1, s2, siz2, f, l, INIT1|INIT2|NULLT)
	? 0 : strspn(s1,s2);
}

int my_strcspn(char *s1, int siz1, char *s2, int siz2, char *f, int l)
{
    if (!logfile) my_initialise();
    siz1 = my_sizehint(s1, siz1);
    siz2 = my_sizehint(s2, siz2);
    return my_validate2("strcspn", s1, siz1, s2, siz2, f, l, INIT1|INIT2|NULLT)
	? 0 : strcspn(s1,s2);
}

/***************************/
/* File function debugging */
/***************************/

#ifndef MAX_FILES
#define MAX_FILES		64
#endif

typedef struct
{
    char *name;
    char *fname;
    int line;
    FILE *fp;
} file_info_t;

static file_info_t file_info[MAX_FILES];

#include <fcntl.h>

FILE *my_fopen(char *n, char *m, char *f, int l)
{
    FILE *rtn;
    if (!logfile) my_initialise();
    rtn = fopen(n,m);
    if (rtn)
    {
    	int h = fileno(rtn);
    	if (file_info[h].line>0)
    	    /* shouldn't happen unless system fopen is broken */
    	    fprintf(logfile,"%2d (%s) fopened at %s, line %d was already opened at %s, line %d\n",
    	    	h, n, f, l, file_info[h].fname, file_info[h].line);
#ifdef GW_TRACE
    	else
    	    fprintf(logfile,"File %2d (%s) fopened at %s, line %d\n", h, n, f, l);
#endif
    	file_info[h].name = store_name(n);
    	file_info[h].fname = store_name(f);
    	file_info[h].line = l;
    	file_info[h].fp = rtn;
    }
    else fprintf(logfile,"fopen of %s at %s, line %d failed!\n", n, f, l);
    return rtn;
}

int my_fclose(FILE *fp, char *f, int l)
{
    if (!logfile) my_initialise();
    if (fp)
    {
    	int h = fileno(fp);
    	if (file_info[h].line <= 0)
    	    fprintf(logfile,"Bad close(%d) at %s, line %d; already closed at %s, line %d\n",
    	    	    h, f, l, file_info[h].fname, -file_info[h].line);
    	else
    	{
#ifdef GW_TRACE
    	    fprintf(logfile,"File %2d (%s) opened at %s, line %d fclosed at %s, line %d\n",
    	    			h, file_info[h].name, file_info[h].fname,
    	    			file_info[h].line, f, l);
#endif
    	    file_info[h].fname = store_name(f);
    	    file_info[h].line = -l;
    	    return fclose(fp);
    	}
    }
    else fprintf(logfile,"Illegal fclose(NULL) by %s line %d\n",
			 f, l);
    return 0;
}

int my_open(char *n, int m, int a, char *f, int l)
{
    int rtn;
    if (!logfile) my_initialise();
    rtn = open(n,m,a);
    if (rtn >= 0)
    {
    	if (file_info[rtn].line>0)
    	    /* shouldn't happen unless system fopen is broken */
    	    fprintf(logfile,"%2d (%s) fopened at %s, line %d was already open at %s, line %d\n",
    	    	rtn, n, f, l, file_info[rtn].fname, file_info[rtn].line);
#ifdef GW_TRACE
    	else
    	    fprintf(logfile,"File %2d (%s) fopened at %s, line %d\n", rtn, n, f, l);
#endif
    	file_info[rtn].name = store_name(n);
    	file_info[rtn].fname = store_name(f);
    	file_info[rtn].line = l;
    	file_info[rtn].fp = NULL;
    }
    else fprintf(logfile,"open of %s at %s, line %d failed!\n\t(%s)\n",
			n, f, l, strerror(errno));
    return rtn;
}

int my_close(int h, char *f, int l)
{
    if (!logfile) my_initialise();
    if (h>=0)
    {
    	if (file_info[h].line <= 0)
    	    fprintf(logfile,"Bad close(%d) by %s, line %d; already closed at %s, line %d\n",
    	    	h, f, l, file_info[h].fname, -file_info[h].line);
    	else
    	{
#ifdef GW_TRACE
    	    fprintf(logfile,"File %2d (%s) opened at %s, line %d, fclosed at %s, line %d\n",
    	    			h, file_info[h].name, file_info[h].fname,
    	    			file_info[h].line, f, l);
#endif
    	    file_info[h].fname = store_name(f);
    	    file_info[h].line = -l;
    	    return close(h);
    	}
    }
    else fprintf(logfile,"Illegal close(%d) by %s line %d\n", h, f, l);
    return 0;
}

int my_dup(int h, char *f, int l)
{
    int rtn = -1;
    if (!logfile) my_initialise();
    if (h>=0)
    {
    	if (file_info[h].line <= 0)
    	    fprintf(logfile,"Illegal dup(%d) by %s line %d\n", h, f, l);
    	else
    	{
    	    rtn = dup(h);
    	    if (rtn >= 0)
    	    {
#ifdef GW_TRACE
    	    	fprintf(logfile,"File %2d (%s) opened at %s, line %d, dup'ed to %d at %s, line %d\n",
    	    		h,
    	    		file_info[h].name,
    	    		file_info[h].fname,
    	    		file_info[h].line,
    	    		rtn, f, l);
#endif
    	    	file_info[rtn].name = file_info[h].name;
    	    	file_info[rtn].fname = store_name(f);
    	    	file_info[rtn].line = l;
    	    	file_info[rtn].fp = NULL;
    	    }
    	    else fprintf(logfile,"dup(%d) by %s line %d failed!\n\t(%s)\n",
    	    		h, f, l, strerror(errno));
    	}
    }
    else fprintf(logfile,"Illegal dup(%d) by %s line %d!\n", h, f, l);
    return rtn;
}

static int my_readcheck(char *name, void *buf, unsigned len, int space_avail, char *f, int l)
{
    if (buf==NULL)
    {
    	fprintf(logfile,"%s with NULL buffer by %s line %d!\n", name, f, l);
    	return 0;
    }
    space_avail = my_sizehint(buf, space_avail);
    if (space_avail >= 0 && space_avail < len)
    {
    	fprintf(logfile,"%s with count (%d)  > available space (5d) by %s line %d!\n",
    		name, len, space_avail, f, l);
    	return space_avail;
    }
    else return len;
}

int my_read(int h, void *buf, unsigned len, int space_avail, char *f, int l)
{
    if (!logfile) my_initialise();
    len = my_readcheck("read", buf, len, space_avail, f, l);
    return len ? read(h,buf,len) : 0;
}

size_t my_fread(void *buf, size_t size, size_t n, FILE *fp, int space_avail, char *f, int l)
{
    if (!logfile) my_initialise();
    if (size==0)
    {
    	fprintf(logfile,"fread with size zero at %s line %d!\n", f, l);
    	return 0;
    }
    n = my_readcheck("fread", buf, n*size, space_avail, f, l) / size;
    return n ? fread(buf,size,n,fp) : 0;
}

char *my_fgets(void *buf, int n, FILE *fp, int space_avail, char *f, int l)
{
    if (!logfile) my_initialise();
    n = my_readcheck("fgets", buf, n, space_avail, f, l);
    return n ? fgets(buf,n,fp) : buf;
}

static void my_file_report(int is_last)
{
    int i, done_heading=0;
    for (i=0;i<40;i++)
    {
    	if (file_info[i].line>0)
    	{
    	    if (is_last && !done_heading)
    	    {
    		done_heading = 1;
    		fprintf(logfile,"FILE LEAKS:\n");
    	    }
    	    fprintf(logfile,"\tFile `%s' (handle %d) opened at %s, line %d\n",
	    		file_info[i].name,i,file_info[i].fname,	file_info[i].line);
	}
    }
}

/*************************/
/* Building on the past! */
/*************************/

void *my_realloc(void *p, unsigned n, char *f, int l)
{
    void *rtn = my_calloc(n,f,l);
    if (p)
    {
    	my_memcpy(rtn, n, 0, p, 0, 0, f, l);
    	my_free(p,f,l);
    }
    return rtn;
}

void far *my_frealloc(void far *p, unsigned long n, char *f, int l)
{
    void far *rtn = my_fcalloc(n,f,l);
    if (p)
    {
    	/* MUST STILL DO THE COPY!!!!!!!!! */
    	my_ffree(p,f,l);
    }
    return rtn;
}

/**********************/
/* Managed name store */
/**********************/

#ifndef MAX_NAMES
#define MAX_NAMES	32
#endif

#ifndef MAX_NAME_LEN
#define MAX_NAME_LEN	16
#endif

static char file_names[MAX_NAMES][MAX_NAME_LEN];

static int filename_index = 0;

static char *store_name(char *name)
{
    int filename_index;
    for (filename_index=0 ; filename_index<MAX_NAMES ; filename_index++)
    {
    	if (file_names[filename_index][0]=='\0')
    	{
    	    strcpy(file_names[filename_index], name);
    	    return file_names[filename_index];
    	}
    	else if (strcmp(file_names[filename_index],name)==0)
    	    return file_names[filename_index];
    }
    return NULL;
}

/********************************/
/* Generate a report at the end */
/********************************/

void my_report(void)
{
    time_t tm = time(NULL);
    if (!logfile) my_initialise();
    fprintf(logfile,"\n\n================ END-OF-PROGRAM DEBUG LOG ===================\n");
    fprintf(logfile,"Log date: %s\n\n", ctime(&tm));
    my_memory_report(1);
    fprintf(logfile,"\n\n");
    my_file_report(1);
    fprintf(logfile,"\n\n================ END OF LOG ===================\n\n");
#ifdef DEBUG_LOG
    fclose(logfile);
#endif
    logfile=NULL;
}

/*****************************/
/* First-time initialisation */
/*****************************/

void my_initialise(void)
{
    atexit(my_report);
#ifdef DEBUG_LOG
    logfile = fopen(DEBUG_LOG,"w");
    assert(logfile);
#else
    logfile = stderr;
#endif
}

/*===================================================================*/

#endif /* GW_DEBUG */


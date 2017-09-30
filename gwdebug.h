/*
 * A drop-in debugging library for C
 *
 * (c) 1994 by Graham Wheeler, All Rights Reserved
 *
 */

#ifndef __GWDEBUG_H__
#define __GWDEBUG_H__

#ifdef GW_DEBUG

/* Memory debugging */

/* undefine them first in case we included heap.h */

#undef malloc
#undef calloc
#undef realloc
#undef free
#if __MSDOS__
#undef farmalloc
#undef farcalloc
#undef farrealloc
#undef farfree
#else
#define far
#define huge
#endif

extern void *my_malloc(unsigned n, char *f, int l);
extern void *my_calloc(unsigned n, char *f, int l);
extern void *my_realloc(void *p, unsigned n, char *f, int l);
extern void my_free(void *p, char *f, int l);
#if __MSDOS__
extern void far *my_fmalloc(unsigned long n, char *f, int l);
extern void far *my_fcalloc(unsigned long n, char *f, int l);
extern void far *my_frealloc(void far *p, unsigned long n, char *f, int l);
extern void my_ffree(void far *p, char *f, int l);
#endif

#if __MSDOS__
#define farmalloc(n)	my_fmalloc(n, __FILE__, __LINE__)
#define farcalloc(n)	my_fcalloc(n, __FILE__, __LINE__)
#define farrealloc(p,n)	my_frealloc(p, n, __FILE__, __LINE__)
#define farfree(p)	my_ffree(p, __FILE__, __LINE__)
#  if defined(__COMPACT__) || defined(__LARGE__) || defined(__HUGE__)
#    define malloc(n)	my_fmalloc(n, __FILE__, __LINE__)
#    define calloc(n)	my_fcalloc(n, __FILE__, __LINE__)
#    define realloc(p,n) my_frealloc(p, n, __FILE__, __LINE__)
#    define free(p)	my_ffree(p, __FILE__, __LINE__)
#  else
#    define malloc(n)	my_malloc(n, __FILE__, __LINE__)
#    define calloc(n)	my_calloc(n, __FILE__, __LINE__)
#    define realloc(p,n) my_realloc(p, n, __FILE__, __LINE__)
#    define free(p)	my_free(p, __FILE__, __LINE__)
#  endif
#else
#  define malloc(n)	my_malloc(n, __FILE__, __LINE__)
#  define calloc(n)	my_calloc(n, __FILE__, __LINE__)
#  define realloc(p,n)	my_realloc(p, n, __FILE__, __LINE__)
#  define free(p)	my_free(p, __FILE__, __LINE__)
#endif

/* String and memory debugging */

extern char *my_memcpy(char *d, int hint, int limit, char *s, int shint, int flag, char *f, int l);
extern char *my_strcpy(char *d, int hint, int limit, char *s, int shint, int flag, char *f, int l);
extern char *my_memset(char *d, int hint, int limit, char c, char *f, int l);
extern int   my_memcmp(char *s1, int siz1, char *s2, int siz2, int n, int flag, char *f, int l);
extern int   my_strcmp(char *s1, int siz1, char *s2, int siz2, int n, int flag, char *f, int l);
extern int   my_strlen(char *s, int siz, char *f, int l);
extern char *my_strdup(char *s, int siz, int n, char *f, int l);
extern char *my_strstr(char *s1, int siz1, char *s2, int siz2, char *f, int l);
extern char *my_strpbrk(char *s1, int siz1, char *s2, int siz2, char *f, int l);
extern char *my_strchr(char *s, int siz, int c, char *f, int l);
extern char *my_strrchr(char *s, int siz, int c, char *f, int l);
extern int   my_strspn(char *s1, int siz1, char *s2, int siz2, char *f, int l);
extern int   my_strcspn(char *s1, int siz1, char *s2, int siz2, char *f, int l);

#define strcpy(d,s)	my_strcpy(d, sizeof(d), 0, s, sizeof(s), 0, __FILE__, __LINE__)
#define stpcpy(d,s)	my_strcpy(d, sizeof(d), 0, s, sizeof(s), 2, __FILE__, __LINE__)
#define strncpy(d,s,n)	my_strcpy(d, sizeof(d), n, s, sizeof(s), 0, __FILE__, __LINE__)
#define strcat(d,s)	my_strcpy(d, sizeof(d), 0, s, sizeof(s), 4, __FILE__, __LINE__)
#define strncat(d,s,n)	my_strcpy(d, sizeof(d), n, s, sizeof(s), 4, __FILE__, __LINE__)
#define memcpy(d,s,n)	my_memcpy(d, sizeof(d), n, s, sizeof(s), 0, __FILE__, __LINE__)
#define memmove(d,s,n)	my_memcpy(d, sizeof(d), n, s, sizeof(s), 1, __FILE__, __LINE__)
#define memset(d,c,n)	my_memset(d, sizeof(d), n, c, __FILE__, __LINE__)
#define strcmp(s1,s2)	my_strcmp(s1, sizeof(s1), s2, sizeof(s2), 0, 0, __FILE__, __LINE__)
#define strncmp(s1,s2,n) my_strcmp(s1, sizeof(s1), s2, sizeof(s2), n, 0, __FILE__, __LINE__)
#define memcmp(s1,s2,n)	my_memcmp(s1, sizeof(s1), s2, sizeof(s2), n, 1, __FILE__, __LINE__)
#define strlen(s)	my_strlen(s, sizeof(s), __FILE__, __LINE__)
#define strdup(s)	my_strdup(s, sizeof(s), __FILE__, __LINE__)
#define strstr(s1,s2)	my_strstr(s1, sizeof(s1), s2, sizeof(s2), __FILE__, __LINE__)
#define strpbrk(s1,s2)	my_strpbrk(s1, sizeof(s1), s2, sizeof(s2), __FILE__, __LINE__)
#define strchr(s,c)	my_strchr(s, sizeof(s1), c, __FILE__, __LINE__)
#define strrchr(s,c)	my_strrchr(s, sizeof(s1), c, __FILE__, __LINE__)
#define strspn(s1,s2)	my_strspn(s1, sizeof(s1), s2, sizeof(s2), __FILE__, __LINE__)
#define strcspn(s1,s2)	my_strcspn(s1, sizeof(s1), s2, sizeof(s2), __FILE__, __LINE__)

/* File debugging */

extern FILE *my_fopen(char *n, char *m, char *f, int l);
extern int   my_fclose(FILE *fp, char *f, int l);
extern int   my_open(char *n, int m, int a, char *f, int l);
extern int   my_close(int h, char *f, int l);
extern int   my_dup(int h, char *f, int l);
extern int   my_read(int h, void *buf, unsigned len, int hint, char *f, int l);
extern size_t my_fread(void *buf, size_t size, size_t n, FILE *fp, int space_avail, char *f, int l);
extern char *my_fgets(void *buf, int n, FILE *fp, int space_avail, char *f, int l);

#define fopen(n,m)	my_fopen(n,m,__FILE__,__LINE__)
#define fclose(f)	my_fclose(f,__FILE__,__LINE__)
#define open(n,m,a)	my_open(n,m,a,__FILE__,__LINE__)
#define close(h)	my_close(h,__FILE__,__LINE__)
#define dup(h)		my_dup(h,__FILE__,__LINE__)
#define read(h,b,n)	my_read(h, b, n, sizeof(b), __FILE__, __LINE__)
#define fread(b,s,n,f)	my_fread(b, s, n, f, sizeof(b), __FILE__, __LINE__)
#define fgets(b,n,f)	my_fgets(b, n, f, sizeof(b), __FILE__, __LINE__)

#endif /* GW_DEBUG */

#endif /* __GWDEBUG_H__ */

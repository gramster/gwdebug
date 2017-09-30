/* showstack:  Show layout of host machine's stack */

#include <stdio.h>

#define STACKLOW        1

short *stacktop;

void /*near*/ g(short arg_2)
{
    short local = 0x7788, *ip;

    printf("Stacktop: %08lx  &local: %08x\n", (long)stacktop, (long)&local);
#ifdef  STACKLOW        /*  Stack grows towards LOWER addresses  */
    for (ip = stacktop; ip >= &local; ip--)
#else                   /*  Stack grows towards HIGHER addresses  */
    for (ip = stacktop; ip <= &local; ip++)
#endif
         printf("%08lx\t%08lx\n", (long)ip, (long)(*ip));
}

void f(short arg_1, short arg_2)
{
    g(0x5566L);
}

int main(int argc, char **argv)
{
    stacktop = (short *) &argv;
    printf("&argc=%08lx  &argv=%08lx\n", (long)&argc, (long)&argv);
    printf("&main=%08lx  &f=%08lx  &g=%08lx\n", (long)main, (long)f, (long)g);
    f(0x1122, 0x3344);
	  return 0;
}





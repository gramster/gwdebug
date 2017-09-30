/* A stack backtracer for Borland C small model. See the test
   code for how to use it. The program must be compiled with
	 a -ls option for a full map file. This is ugly code, but
	 illustrates the principal. I have no idea how to make it
	 work with the large and mixed memory models, unfortunately,
	 as I can't figure out the relationship between what is on the
	 stack and what is in the map file.

	(c) 1994 by Graham Wheeler
*/

#include <stdio.h>
#include <string.h>
#include <dos.h>

unsigned short *stacktop;

void near Abort(char *mapfile)
{
    unsigned short *p = (unsigned short *)&p;
	  fprintf(stderr, "Aborting! Stack backtrace:\n\n");
	  p++; /* get to first BP */
	  while (p < stacktop)
	  {
		  char buf[128], lastn[40];
		  unsigned short lastaddr = 0xffff;
		  FILE *map = fopen(mapfile, "r");
		  buf[127] = 0;
		  while (!feof(map))
		  {
			fgets(buf, 127, map);
			if (strncmp(buf, "  Address         Publics by Value", 34)==0)
				break;
		  }
		  while (!feof(map))
		  {
			char n1[40], n2[40], *n;
			short addr, cnt;
			fgets(buf, 127, map);
			cnt = sscanf(buf, "%*X:%X %s %s", &addr, n1, n2);
			if (cnt == 3) n = n2;
			else if (cnt == 2) n = n1;
			else continue;
			if (p[1] > lastaddr && p[1] < addr)
			{
				fprintf(stderr, "%04X  %s\n", p[1], lastn);
				if (strcmp(lastn, "_main")==0)
					exit(-1);
				break;
			}
			strcpy(lastn, n);
			lastaddr = addr;
		  }
		  p = (unsigned short *)(*p);
		  fclose(map);
	  }  
}

#ifdef TEST

void near test2(void)
{
	Abort("test.map");
}

void test(void)
{
	test2();
}

void main(int argc, char *argv[])
{
  stacktop = (unsigned short *) &argv;
	(void)argc; /* avoid compiler warning */
	test();
}

#endif /* TEST */


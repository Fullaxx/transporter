#include <stdio.h>
#include <stdlib.h>

unsigned long randomul(void)
{
	FILE *f;
	unsigned long rnum = 0;

	f = fopen("/dev/urandom", "r");
	if(f) {
		(void) fread(&rnum, sizeof(unsigned long), 1, f);
		fclose(f);
	} else {
		rnum = ((long)rand() << 32) | rand();
	}

	return rnum;
}

int rpick(int bound)
{
	return (int)(randomul() % bound);
}

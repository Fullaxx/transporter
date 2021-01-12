#ifndef __TPAD_STATS__
#define __TPAD_STATS__

#include <stdio.h>
#include <string.h>

#include "gcryptfile.h"

// this must be free()'d
static char* get_stats(gcfile_t *gcf)
{
	int n = 0;
	unsigned long bytes;
	unsigned long usec;
	double speed;
	char stats[256];

	bytes = gcfile_get_bytecount(gcf);
	if(bytes > 1000000) {
		n += snprintf(stats+n, sizeof(stats)-n, "(%luMB)", bytes/1000000);
	} else if(bytes > 1000) {
		n += snprintf(stats+n, sizeof(stats)-n, "(%luKB)", bytes/1000);
	} else {
		n += snprintf(stats+n, sizeof(stats)-n, "(%luB)", bytes);
	}

	usec = gcfile_get_duration(gcf);
	if(usec == 0) { return strdup(stats); }
	// If we don't have proper timing information, bail early

	if(usec > 1000000*60) {
		n += snprintf(stats+n, sizeof(stats)-n, " [%lum]", usec/(1000000*60));
	} else if(usec > 1000000) {
		n += snprintf(stats+n, sizeof(stats)-n, " [%lus]", usec/1000000);
	} else {
		n += snprintf(stats+n, sizeof(stats)-n, " [%luu]", usec);
	}

/*
	MegaBytes      (bytes/1000000)      bytes
	_________ == __________________ == ________
	 Seconds     (useconds/1000000)    useconds
*/

	speed = (double)(bytes) / (double)(usec);
	n += snprintf(stats+n, sizeof(stats)-n, " {%3.3fMB/s}", speed);

	return strdup(stats);
}

#endif

// Replacement for SDL_GetTicks

#include "header/fixes_ps3.h"

#include <sys/time.h>

uint32_t getTicks(void)
{
	static qboolean ticks_started = false;
	static struct timeval start;
	if (!ticks_started)
	{
		ticks_started = true;
		gettimeofday(&start, NULL);
	}

	struct timeval now;
	uint32_t ticks;

	gettimeofday(&now, NULL);
	ticks = (now.tv_sec - start.tv_sec) * 1000 + (now.tv_usec - start.tv_usec) / 1000;
	return ticks;
}

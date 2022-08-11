/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * =======================================================================
 *
 * This file implements the low level part of the Hunk_* memory system
 *
 * =======================================================================
 */

/*
 * Code taken from vitaQuakeII
 * As calculated this is takes around 1 megabytes 
 * more RAM (total - after level load) which is negligible
 * It is so until mman.h would be ported to PS3
 */

#include "../../../common/header/common.h"

byte *membase;
int maxhunksize;
int curhunksize;

void *Hunk_Begin (int maxsize)
{
	// reserve a huge chunk of memory, but don't commit any yet
	maxhunksize = maxsize + sizeof(size_t) + 32;
	curhunksize = 0;
	membase = malloc(maxhunksize);

	if (membase == NULL)
    {
		Sys_Error("unable to allocate %d bytes", maxhunksize);
    }
	else
    {
		memset (membase, 0, maxhunksize);
    }

	return membase;
}

void *Hunk_Alloc (int size)
{
	byte *buf;

	// round to cacheline
	size = (size+31)&~31;

	if (curhunksize + size > maxhunksize)
    {
		Sys_Error("Hunk_Alloc overflow");
    }

	buf = membase + curhunksize;
	curhunksize += size;

	return buf;
}

static void loose_counter() 
{
    static int loose = 0;
    loose += (maxhunksize - curhunksize);
    printf("inc: %d bytes loose: %d bytes\n", maxhunksize - curhunksize, loose);
}

int Hunk_End (void)
{
	/* We would prefer to shrink the allocated memory
	 * buffer to 'curhunksize' bytes here, but there exists
	 * no robust cross-platform method for doing this
	 * given that pointers to arbitrary locations in
	 * the buffer are stored and used throughout the
	 * codebase...
	 * (i.e. realloc() would invalidate these pointers,
	 * and break everything)
	 * Attempts were made to allocate hunks dynamically,
	 * storing them in an RBUF array - but the codebase
	 * plays such dirty tricks with the returned pointers
	 * that this turned out to be impractical (it would
	 * have required a major rewrite of the renderers...) */
#if 0
	byte *n;

	n = realloc(membase, curhunksize);

	if (n != membase)
		Sys_Error("Hunk_End:  Could not remap virtual block (%d)", errno);
#endif
    loose_counter();
	return curhunksize;
}

void Hunk_Free (void *base)
{
	if (base == membase)
    {
		membase = NULL;
    }

	if (base)
    {
		free(base);
    }
}


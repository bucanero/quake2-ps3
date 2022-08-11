// system.c for PSL1GHT

#include "../../common/header/common.h"
#include "../../common/header/glob.h"

#include <ppu-lv2.h>
#include <lv2/syscalls.h>

#include <sys/file.h>
#include <sys/stat.h>

#include <sys/cdefs.h>

// replacing it here for compatability with yq2's common.h
#ifdef CFGDIR
#undef CFGDIR
#endif
#define CFGDIR "USRDIR"

// Config dir
char cfgdir[MAX_OSPATH] = CFGDIR;

/* ================================================================ */

void 
Sys_Error(char *error, ...)
{
	va_list argptr;
	char string[1024];

	/* change stdin to non blocking */
	// fcntl(0, F_SETFL, fcntl(0, F_GETFL, 0) & ~FNDELAY);

#ifndef DEDICATED_ONLY
	CL_Shutdown();
#endif
	Qcommon_Shutdown();

	va_start(argptr, error);
	vsnprintf(string, 1024, error, argptr);
	va_end(argptr);
	fprintf(stderr, "Error: %s\n", string);

	exit(1);
}

void Sys_Quit(void)
{
#ifndef DEDICATED_ONLY
 	CL_Shutdown();
#endif

	// if (logfile)
	// {
	// 	fclose(logfile);
	// 	logfile = NULL;
	// }

	Qcommon_Shutdown();
	// fcntl(0, F_SETFL, fcntl(0, F_GETFL, 0) & ~FNDELAY);

	printf("------------------------------------\n");

	exit(0);
}

void 
Sys_Init(void)
{
}


// ---------------------------------------------
//  Input/Output
// ---------------------------------------------

// returns static null terminated string or NULL
char*
Sys_ConsoleInput(void)
{
	/* PS3 had tty input but let's be real
	   we will never use ps3 as dedicated server
	   moreover with tty console input.
	   In future it could be onscreen keyboard
	   call. */
	return NULL;
}

// prints null terminated <string> to stdout
void
Sys_ConsoleOutput(char *string)
{
	fputs(string, stdout);
}


// ---------------------------------------------
//  Time
// ---------------------------------------------

inline void
Sys_getCurrentTime(int64_t* seconds, int64_t* nanoseconds)
{
	lv2syscall2(SYSCALL_TIME_GET_CURRENT_TIME, (uint64_t)seconds, (uint64_t)nanoseconds);
	// printf("Sys_getCurrentTime {s: %ld n: %ld}\n", *seconds, *nanoseconds);
}

#define TIMESPEC_NOW(ts) Sys_getCurrentTime(&(ts.tv_sec), &(ts.tv_nsec))

// returns microseconds since first call
long long
Sys_Microseconds(void)
{
	// Call itself takes around 1-3 us. More on first time.
	// Uses implementation from YQ2's system.c for unix 

	struct timespec now;
	static struct timespec first = {0, 0};
	TIMESPEC_NOW(now);

	if(first.tv_sec == 0)
	{
		long long nsec = now.tv_nsec;
		long long sec = now.tv_sec;
		// set back first by 1ms so neither this function nor Sys_Milliseconds()
		// (which calls this) will ever return 0
		nsec -= 1000000ll;
		if(nsec < 0)
		{
			nsec += 1000000000ll; // 1s in ns => definitely positive now
			--sec;
		}

		first.tv_sec = sec;
		first.tv_nsec = nsec;
	}

	long long sec  = now.tv_sec  - first.tv_sec;
	long long nsec = now.tv_nsec - first.tv_nsec;
	
	return sec*1000000ll + nsec/1000ll;
}

int
Sys_Milliseconds(void)
{
	return (int)(Sys_Microseconds()/1000ll);
}

// sleep for <nanosec> nanoseconds
void 
Sys_Nanosleep(int nanosec)
{
	/* Uses usleep syscall instead of nanosleep
	   due lack of it on PowerPC.
	   It's ain't problem b/c current codebase of YQ2
	   not calling it for precision higher then 1 us. */
	lv2syscall1(SYSCALL_TIMER_USLEEP, (uint64_t)(nanosec/1000ll));
}


// ---------------------------------------------
//  Filesytem
// ---------------------------------------------

// /absolute/path/to/directory[/]
void 
Sys_Mkdir(const char *path)
{
	// code based on one in apollo's savetool
	
	char fullpath[MAX_OSPATH];

	//snprintf(fullpath, sizeof(fullpath), "%s", path);
	strlcpy(fullpath, path, MAX_OSPATH);

	// remove trailing '/'
	char* ptr = fullpath;
	while (*ptr != '\0')
	{
		++ptr;
	}
	--ptr;
	if (*ptr == '/')
	{
		*ptr = '\0';
	}
	ptr = fullpath;

	// creating path to directory
	while (*ptr)
    {
    	ptr++;
        while (*ptr && *ptr != '/')
		{
            ptr++;
		}

        char last = *ptr;
		*ptr = 0;

        if (Sys_IsDir(fullpath) == false)
        {
            if (mkdir(fullpath, 0777) < 0)
			{
                return;
			}
            sysLv2FsChmod(fullpath, S_IFDIR | 0777);
        }
        
        *ptr = last;
    }

    return;
}

qboolean
Sys_IsDir(const char *path)
{
	struct stat sb;

	if (stat(path, &sb) != -1)
	{
		if (S_ISDIR(sb.st_mode))
		{
			return true;
		}
	}

	return false;
}

qboolean
Sys_IsFile(const char *path)
{
	struct stat sb;

	if (stat(path, &sb) != -1)
	{
		if (S_ISREG(sb.st_mode))
		{
			return true;
		}
	}

	return false;
}

// returns null terminated string -- path to
// config directory should end with '<cfgdir>/'
// variable whichi is defined in macro CFGDIR
char*
Sys_GetHomeDir(void)
{
	static char homedir[MAX_OSPATH];
	static const char* home = "/dev_hdd0/game/QUAKE2";

	snprintf(homedir, sizeof(homedir), "%s/%s/", home, cfgdir);
	Sys_Mkdir(homedir);

	return homedir;
}

const char* Sys_GetBinaryDir()
{
	/* We don't load any libraries at runtime so we use this trick
       to not allow filesystem to add this path to search base, but
	   vid.c still needed to be changed for single file build.
	   Also this used in portable run which should not be allowed. */
	return "\0";
}

// See remove(3)
// Used in *download and Load/Save functions
void
Sys_Remove(const char *path)
{
	remove(path);
}

// See rename(2)
// Used in *download functions
int
Sys_Rename(const char *from, const char *to)
{
	return rename(from, to);
}

// removes dir <path> if it is exists
// <path> is absolute
void
Sys_RemoveDir(const char *path)
{
	if (sysLv2FsRmdir(path) != 0) 
	{
		printf("Sys_RemoveDir: can't remove dir: '%s'\n", path);
	}
}

// writes return of realpath(3) (<in>, NULL)
// to buffer <out> with max size <size>
// if realpath returns NULL returns false
// otherwise returns true
qboolean
Sys_Realpath(const char *in, char *out, size_t size)
{
	// Block off relative paths
	if (strstr(in, "..") != NULL)
	{
		Com_Printf("WARNING: Sys_Realpath2: refusing to solve relative path '%s'.\n", in);
		return false;
	}

	// Absolute paths remain the same
	// TODOOOO: /path/././dir_in_path/
	// TODO: Move to separate function
	if (in[0] == '/')
    {
		int len = Q_strlcpy(out, in, size);
		//size_t len = strlcpy(out, in, size);
		// But we remove last forwarslash
		if (out[len-1] == '/')
		{
			out[len-1] = '\0';
		}
		return Sys_IsDir(out);
	}

	size_t path_start = 0;
	if (in[0] == '.' && (in[1] == '/' || in[1] == '\0'))
	{
		++path_start;
		if (in[1] == '/')
		{
			++path_start;
		}
	}

	char* buf = malloc(size);
	Sys_GetWorkDir(buf, size);
	if (buf[0] == '\0')
	{
		Com_Printf("WARNING: Sys_Realpath2: Sys_GetWorkDir returned null string.\n");
		free(buf);
		return false;
	}
	char* bufend = buf;
	size_t freespace = size; 

	while (*bufend)
	{
		++bufend;
		--freespace;
	}
	Q_strlcpy(bufend, &in[path_start], freespace);
	// strlcpy(bufend, &in[path_start], freespace);

	// Threat buf as absolute path by calling
	// recursively itself.
	qboolean ret = Sys_Realpath(buf, out, size);

	free(buf);
	return ret;
}


#ifdef NEED_GET_PROC_ADDRESS
#error "Sys_GetProcAddress could not be implemented"
// void*
// Sys_GetProcAddress(void *handle, const char *sym);
#endif

#ifndef UNICORE
#error "dynamic libs could not be implemented"
// void
// Sys_FreeLibrary(void *handle);
// void*
// Sys_LoadLibrary(const char *path, const char *sym, void **handle);
// void*
// Sys_GetGameAPI(void *parms);
// void 
// Sys_UnloadGame(void);
#endif


// ---------------------------------------------
//  Process location
// ---------------------------------------------

/* This functions are used in server savegame read/write 
   functions. In YQ2 8.02pre game switches it's working
   directory to save path and then saves/read files at ./<name>
   after that switches back to orig WorkDir 
   So both functions could ignored which is leads to
   saving save/read files on default running directory.
   I don't know where is it. */

// writes YQ2's working directory path* 
// to <buffer> up to <len> bytes
// *path should be null terminated
void
Sys_GetWorkDir(char *buffer, size_t len)
{
	if (getcwd(buffer, len) != 0)
	{
		return;
	}

	buffer[0] = '\0';
}

// Sets YQ2's working directory to <path>
// <path> is null terminate string
// returns true on success false on fail
qboolean
Sys_SetWorkDir(char *path)
{
	if (chdir(path) == 0)
	{
		return true;
	}

	return false;
}

/* ================================================================ */

/* The musthave and canhave arguments are unused in YQ2. We
   can't remove them since Sys_FindFirst() and Sys_FindNext()
   are defined in shared.h and may be used in custom game DLLs. */

static char findbase[MAX_OSPATH];
static char findpath[MAX_OSPATH];
static char findpattern[MAX_OSPATH];
static s32 fdir = -1;


// /dev_hdd0/game/QUAKE2/USRDIR/baseq2/*.pak - should open
// /dev_hdd0/game/baseq2/*.pak - should not open (return NULL)
char *Sys_FindFirst(char *path, unsigned musthave, unsigned canthave)
{
	sysFSDirent entry;
	// entry.d_type
	// files -> 0x02
	// dirs  -> 0x01

	char* ptr;

	// Block off relative paths
	if (strstr(path, "..") != NULL)
	{
		Com_Printf("WARNING: Sys_FindFirst: relative paths not allowed '%s'.\n", path);
		return NULL;
	}

	if (fdir >= 0)
	{
		Com_Printf("WARNING: Sys_FindFirst: without closing previous search, closing...\n");
		Sys_FindClose();
	}

	strcpy(findbase, path);

	if ((ptr = strrchr(findbase, '/')) != NULL)
	{
		*ptr = 0;
		strcpy(findpattern, ptr + 1);
		if (strcmp(findpattern, "*.*") == 0)
		{
			strcpy(findpattern, "*");
		}
	}
	else
	{
		strcpy(findpattern, "*");
	}

	// /dev_hdd0/game/QUAKE2/USRDIR/baseq2/ - fdir > 0
	// /dev_hdd0/game/QUAKE2/USRDIR/baseq2/not_existing*msa!%#@ - fdir < 0
	sysLv2FsOpenDir(findbase, &fdir);
	if (fdir < 0)
	{
		// Com_Printf("WARNING: Sys_FindFirst: could not open search directory '%s'\n", findbase);
		return NULL;
	}

	u64 readn = 0; 
	// 0 - '.' -> 0x102 - '..' -> 0x102 - 'file.pak -> 0x102 - 'scrnshot' -> 0x102 - EOF -> 0

	while (true)
	{
		sysLv2FsReadDir(fdir, &entry, &readn);
		if (readn != 0x102)
		{
			break;
		}
		if (!*findpattern || glob_match(findpattern, entry.d_name))
		{
			if ((strcmp(entry.d_name, ".") != 0) || (strcmp(entry.d_name, "..") != 0))
			{
				// Safe way to create path

				int c_cnt = MAX_OSPATH;
				int n_cpd;
				ptr = findpath;

				n_cpd = Q_strlcpy(ptr, findbase, c_cnt);
				ptr += n_cpd;
				c_cnt -= n_cpd;

				n_cpd = Q_strlcpy(ptr, "/", c_cnt);
				ptr += n_cpd;
				c_cnt -= n_cpd;

				Q_strlcpy(ptr, entry.d_name, c_cnt);
				return findpath;
			}
		}
	}

	return NULL;
}

char *Sys_FindNext(unsigned musthave, unsigned canthave)
{
	sysFSDirent entry;
	u64 readn = 0; 

	if (fdir < 0)
	{
		Com_Printf("WARNING: Sys_FindNext: search not started\n");
		return NULL;
	}

	while (true)
	{
		sysLv2FsReadDir(fdir, &entry, &readn);
		if (readn != 0x102)
		{
			break;
		}
		if (!*findpattern || glob_match(findpattern, entry.d_name))
		{
			if ((strcmp(entry.d_name, ".") != 0) || (strcmp(entry.d_name, "..") != 0))
			{
				// Safe way to create path

				int c_cnt = MAX_OSPATH;
				int n_cpd;
				char* ptr = findpath;

				n_cpd = Q_strlcpy(ptr, findbase, c_cnt);
				ptr += n_cpd;
				c_cnt -= n_cpd;

				n_cpd = Q_strlcpy(ptr, "/", c_cnt);
				ptr += n_cpd;
				c_cnt -= n_cpd;

				Q_strlcpy(ptr, entry.d_name, c_cnt);
				return findpath;
			}
		}
	}

	return NULL;
}

void Sys_FindClose(void)
{
	if (fdir >= 0)
	{
		sysLv2FsCloseDir(fdir);
	}
		
	fdir = -1;
}

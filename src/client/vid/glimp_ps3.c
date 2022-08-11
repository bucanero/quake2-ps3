/*
 * Copyright (C) 2010 Yamagi Burmeister
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
 * This is the client side of the render backend, implemented trough
 * PS3's GCM. Screen manipulation implemented here, everything else is
 * in the renderers.
 *
 * =======================================================================
 * 
 * Notes on PS3 changes:
 *     This used do link screen functions to gcm based renderers
 * 
 * -----------------------------------------------------------------------
 */

#include "../../common/header/common.h"
#include "header/ref.h"

#include <stdio.h>

static cvar_t *vid_displayrefreshrate;
static cvar_t *vid_displayindex;
static cvar_t *vid_rate;

static qboolean initSuccessful = false;
static char **displayindices = NULL;
static int num_displays = 0;

// --- for compatibility with gcm
#include <sysutil/video.h>
#include <malloc.h>
#include <rsx/rsx.h>
#include <rsx/gcm_sys.h>
#include <sys/systime.h>

static gcmContextData *context = NULL;;
static void *host_addr = NULL;
static videoDeviceInfo vDeviceInfo;
int n_num_displays;
static char** n_displayindices;

#define CB_SIZE 0x100000
#define HOST_SIZE (32*1024*1024)
#define GCM_LABEL_INDEX 255
// ----

int glimp_refreshRate = -1;
static int last_display = 0;

/*
 * Resets the display index Cvar if out of bounds
 */
static void
ClampDisplayIndexCvar(void)
{
	if (!vid_displayindex)
	{
		// uninitialized render?
		return;
	}

	if (vid_displayindex->value < 0 || vid_displayindex->value >= n_num_displays)
	{
		Cvar_SetValue("vid_displayindex", 0);
	}
}

static void
ClearDisplayIndices(void)
{
	if (displayindices)
	{
		for (int i = 0; i < num_displays; ++i)
		{
			free(displayindices[i]);
		}

		free(displayindices);
		displayindices = NULL;
	}
}

void
GLimp_initDisplayIndices()
{
	n_displayindices = malloc((n_num_displays + 1) * sizeof(char*));

	for (int i = 0; i < n_num_displays; ++i)
	{
		/* There are a maximum of 10 digits in 32 bit int + 1 for the NULL terminator. */
		n_displayindices[i] = malloc(11 * sizeof( char ));
		YQ2_COM_CHECK_OOM(n_displayindices[i], "malloc()", 11 * sizeof(char))

		snprintf(n_displayindices[i], 11, "%d", i);
	}

	/* The last entry is NULL to indicate the list of strings ends. */
	n_displayindices[n_num_displays] = 0;
}

#define TRY_REFRESH(rates, mode) if ((rates & mode) != 0) return mode

u16
maxAvailableRate(u16 refreshRates)
{
	TRY_REFRESH(refreshRates, VIDEO_REFRESH_60HZ);
	TRY_REFRESH(refreshRates, VIDEO_REFRESH_59_94HZ);
	TRY_REFRESH(refreshRates, VIDEO_REFRESH_50HZ);
	TRY_REFRESH(refreshRates, VIDEO_REFRESH_30HZ);

	return VIDEO_REFRESH_AUTO;
}

const char*
refreshRateName(u16 refreshRate)
{
	if (refreshRate == VIDEO_REFRESH_60HZ) return "60";
	if (refreshRate == VIDEO_REFRESH_59_94HZ) return "59.94";
	if (refreshRate == VIDEO_REFRESH_50HZ) return "50";
	if (refreshRate == VIDEO_REFRESH_30HZ) return "30";

	return "0";
}

int
refreshRateToI(u16 refreshRate)
{
	if (refreshRate == VIDEO_REFRESH_60HZ) return 60;
	if (refreshRate == VIDEO_REFRESH_59_94HZ) return 60;
	if (refreshRate == VIDEO_REFRESH_50HZ) return 50;
	if (refreshRate == VIDEO_REFRESH_30HZ) return 30;

	return -1;
}

/*
 * Lists all available display modes.
 */
static void
GLimp_printDisplayModes(void)
{
	videoResolution res;
	u16 rate;
	for (size_t i = 0; i < vDeviceInfo.availableModeCount; ++i)
	{
		videoGetResolution(vDeviceInfo.availableModes[i].resolution, &res);
		rate = maxAvailableRate(vDeviceInfo.availableModes[i].refreshRates);

		Com_Printf(" - Mode %ld: %dx%d@%s\n",
			i,
			res.width,
			res.height,
			refreshRateName(rate)
		);
	}
}

qboolean
updateConfig()
{
	videoDeviceInfo vDevInfo;

	/* PS3 had 2 'videoOut' types: primary and secondary.
	   Don't know in which situations seconday video might
	   be used. videoGetNumberOfDevice returns number of
	   devices on <videoOut> (can't imagine what it could mean)
	   So each [<videoOut>][<videoOutDisplayIndex>] had it's
	   own 'videoDeviceInfo' structure and it's technically
	   posible to switch between all of them at runtime.

	   To keep things simple let's stay with PRIMARY
	   videoOut and use only first device on it. */

	if (videoGetDeviceInfo(VIDEO_PRIMARY, 0, &vDevInfo) != 0)
	{
		return false;
	}

	vDeviceInfo = vDevInfo;

	return true;
}

gcmContextData *
GLimp_initScreen(void *host_addr, u32 size)
{
	gcmContextData *context = NULL; /* Context to keep track of the RSX buffer. */
	videoState state;
	videoConfiguration vconfig;
	videoResolution res; /* Screen Resolution */

	/* Initilise Reality, which sets up the command buffer and shared IO memory */
	rsxInit(&context, CB_SIZE, size, host_addr);
	if (context == NULL)
		goto error;

	/* Get the state of the display */
	if (videoGetState (0, 0, &state) != 0)
		goto error;

	/* Make sure display is enabled */
	if (state.state != 0)
		goto error;

	/* Get the current resolution */
	if (videoGetResolution (state.displayMode.resolution, &res) != 0)
		goto error;

	/* Configure the buffer format to xRGB */
	memset (&vconfig, 0, sizeof(videoConfiguration));
	vconfig.resolution = state.displayMode.resolution;
	vconfig.format = VIDEO_BUFFER_FORMAT_XRGB;
	vconfig.pitch = res.width * sizeof(u32);
	vconfig.aspect = state.displayMode.aspect;

	// waitRSXIdle
	{
		u32 sLabelVal = 1;

		rsxSetWriteBackendLabel(context, GCM_LABEL_INDEX, sLabelVal);
		rsxSetWaitLabel(context, GCM_LABEL_INDEX, sLabelVal);

		sLabelVal++;

		// waitFinish
		{
			rsxSetWriteBackendLabel(context, GCM_LABEL_INDEX, sLabelVal);

			rsxFlushBuffer(context);

			while( *(vu32 *)gcmGetLabelAddress(GCM_LABEL_INDEX) != sLabelVal)
			{
				sysUsleep(30);
			}
		}
	}

	if (videoConfigure (0, &vconfig, NULL, 0) != 0)
		goto error;

	if (videoGetState (0, 0, &state) != 0)
		goto error;

	gcmSetFlipMode(GCM_FLIP_VSYNC); // Wait for VSYNC to flip

	gcmResetFlipStatus();

	return context;

error:
	if (context)
		rsxFinish(context, 0);

	if (host_addr)
		free(host_addr);

	return NULL;
}

/*
 * Initializes the video subsystem. Must
 * be called before anything else.
 */
qboolean
GLimp_Init(void)
{
	vid_displayrefreshrate = Cvar_Get("vid_displayrefreshrate", "-1", CVAR_ARCHIVE);
	vid_displayindex = Cvar_Get("vid_displayindex", "0", CVAR_ARCHIVE);
	vid_rate = Cvar_Get("vid_rate", "-1", CVAR_ARCHIVE);

	if (!context)
	{
		Com_Printf("-------- vid initialization --------\n");

		/* Allocate a 1Mb buffer, alligned to a 1Mb boundary
		* to be our shared IO memory with the RSX. */
		host_addr = memalign(1024*1024, HOST_SIZE);
		context = GLimp_initScreen(host_addr, HOST_SIZE);
		if (context == NULL)
		{
			Com_Printf("%s initScreen returned NULL\n", __func__);
			return false;
		}

		if (updateConfig() == false)
		{
			Com_Printf("Couldn't get display info\n");
			if (context)
			{
				rsxFinish(context, 0);
				context = NULL;
			}

			if (host_addr)
			{
				free(host_addr);
				host_addr = NULL;
			}

			context = NULL;
			return false;
		}
		Com_Printf("Using native gcm...\n");
		n_num_displays = 1;
		GLimp_initDisplayIndices();
		ClampDisplayIndexCvar();
		Com_Printf("Display modes:\n");
		GLimp_printDisplayModes();

		Com_Printf("------------------------------------\n\n");
	}

	return true;
}

/*
 * Shuts the render backend down
 */
static void
ShutdownGraphics(void)
{
	ClampDisplayIndexCvar();

	// make sure that after vid_restart the refreshrate will be queried from S.D.L.2 again.
	glimp_refreshRate = -1;

	initSuccessful = false; // not initialized anymore
}

/*
 * Shuts the video subsystem down. Must
 * be called after evrything's finished and
 * clean up.
 */
void
GLimp_Shutdown(void)
{
	ShutdownGraphics();

	gcmSetWaitFlip(context);

	if (context)
	{
		Com_Printf("rsxFinish(context, 1)\n");
		rsxFinish(context, 1);
		context = NULL;
	}

	if (host_addr)
	{
		Com_Printf("free(host_addr)\n");
		free(host_addr);
		host_addr = NULL;
	}

	ClearDisplayIndices();
}

qboolean
GLimp_InitGraphics(int fullscreen, int *pwidth, int *pheight)
{
	int width = *pwidth;
	int height = *pheight;

	{
		videoResolution res;
		videoState state;
		if (videoGetState(0, 0, &state) != 0)
		{
			Com_Printf("Can't get video state for default display.\n");
			Com_Printf("Are you mad and trying to start game without attached display?\n");
			return false;
		}

		/* Make sure display is enabled */
		if (state.state != 0)
		{
			Com_Printf("Default display is in use or disabled.\n");
			Com_Printf("How did you manage to do so?\n");
			return false;
		}

		/* Get the current resolution */
		if (videoGetResolution(state.displayMode.resolution, &res) != 0)
		{
			Com_Printf("Default display is something that PS3 should not have.\n");
			Com_Printf("Please set your PS3 screen setting to some reasonable value.\n");
			return false;
		}

		if ((width > res.width) || (height > res.height))
		{
			return false;
		}
	}

	static qboolean contextInited = false;

	if (contextInited)
	{
		Com_Printf("re: %p\n", re.ShutdownContext);
		re.ShutdownContext();
		ShutdownGraphics();
		contextInited = false;
	}

	/* We need the window size for the menu, the HUD, etc. */
	viddef.width = width;
	viddef.height = height;

	/* Mkay, now the hard work. Let's create the window. */
	// This one unused so force 0 to not confuse user at video menu
	// cvar_t *gl_msaa_samples = Cvar_Get("r_msaa_samples", "0", CVAR_ARCHIVE);
	Cvar_Set("r_msaa_samples", "0");

	/* Now that we've got a working window print it's mode. */
	int curdisplay = 0;

	if (curdisplay < 0) {
		curdisplay = 0;
	}

	/* Initialize rendering context. */
	if (!re.InitContext(context))
	{
		/* InitContext() should have logged an error. */
		return false;
	}

	initSuccessful = true;
	contextInited = true;

	return true;
}

/*
 * Shuts the graphics down.
 */
void
GLimp_ShutdownGraphics(void)
{
	ShutdownGraphics();
}

/*
 * Returns the current display refresh rate. There're 2 limitations:
 *
 * * The timing code in frame.c only understands full integers, so
 *   values given by vid_displayrefreshrate are always round up. For
 *   example 59.95 become 60. Rounding up is the better choice for
 *   most users because assuming a too high display refresh rate
 *   avoids micro stuttering caused by missed frames if the vsync
 *   is enabled. The price are small and hard to notice timing
 *   problems.
 *
 * * S.D.L. returns only full integers. In most cases they're rounded
 *   up, but in some cases - likely depending on the GPU driver -
 *   they're rounded down. If the value is rounded up, we'll see
 *   some small and nard to notice timing problems. If the value
 *   is rounded down frames will be missed. Both is only relevant
 *   if the vsync is enabled.
 */
int
GLimp_GetRefreshRate(void)
{
	if (vid_displayrefreshrate->value > 0 ||
			vid_displayrefreshrate->modified)
	{
		glimp_refreshRate = ceil(vid_displayrefreshrate->value);
		vid_displayrefreshrate->modified = false;
	}

	if (glimp_refreshRate > 0)
	{
		return glimp_refreshRate;
	}

	// Initialization
	videoState vState;
	if (videoGetState(VIDEO_PRIMARY, 0, &vState) == 0)
	{
		u16 rate = maxAvailableRate(vState.displayMode.refreshRates);
		glimp_refreshRate = refreshRateToI(rate);
	}

	if (glimp_refreshRate <= 0)
	{
		// Something went wrong, use default.
		glimp_refreshRate = 60;
	}

	return glimp_refreshRate;
}

/*
 * Detect current desktop mode
 */
qboolean
GLimp_GetDesktopMode(int *pwidth, int *pheight)
{
	videoState vState;
	if (videoGetState(VIDEO_PRIMARY, 0, &vState) != 0)
	{
		// uncreachable posibly called then screen does not init yet
		return false;
	}
	videoResolution resolution;
	videoGetResolution(vState.displayMode.resolution, &resolution);

	*pwidth = resolution.width;
	*pheight = resolution.height;

	return true;
}

const char**
GLimp_GetDisplayIndices(void)
{
	return (const char**)n_displayindices;
}

int
GLimp_GetNumVideoDisplays(void)
{
	return n_num_displays;
}

int
GLimp_GetWindowDisplayIndex(void)
{
	return last_display;
}

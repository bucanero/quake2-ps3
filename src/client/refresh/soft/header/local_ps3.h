#ifndef _LOCAL_PS3_H_
#define _LOCAL_PS3_H_

#include "local.h"

/* This is header file to link together sw_ps3_main.c
   and it's frontend (sw_ps3_sdl.c or sw_ps3_gcm.c).
   It should not be included in other files.
   Eventualy, Yamagi and it's team made own frontend
   standartization a/k/a (sw_sdl.c) after that this
   will be removed and their aproach will took it's 
   place. */

// -----------------------------------------------------------------------------
//                        main
// -----------------------------------------------------------------------------

typedef struct swstate_s
{
	qboolean	fullscreen;
	int		prev_mode; // last valid SW mode

	unsigned char	gammatable[256];
	unsigned char	currentpalette[1024];
} swstate_t;

extern swstate_t sw_state;
extern pixel_t *swap_frames[2];
extern int swap_current;
extern qboolean	palette_changed;

extern cvar_t* r_vsync;
extern cvar_t* sw_partialrefresh;
extern cvar_t* sw_anisotropic;

void R_InitGraphics(int width, int height);
void SWimp_CreateRender(int width, int height);
void RE_CopyFrame (uint32_t * pixels, int pitch, int vmin, int vmax);
void VID_NoDamageBuffer(void);
void RE_ShutdownRenderer(void);

struct image_s* RE_RegisterSkin(char *name);
void RE_SetSky(char *name, float rotate, vec3_t axis);
void RE_RenderFrame(refdef_t *fd);
qboolean RE_Init(void);
qboolean RE_IsVsyncActive(void);
void RE_Shutdown(void);
void RE_SetPalette(const unsigned char *palette);
void RE_BeginFrame(float camera_separation);
qboolean RE_EndWorldRenderpass(void);
void RE_EndFrame(void);

// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
//                      frontend
// -----------------------------------------------------------------------------

/**
 * @brief Apply frame changed to screen
 * 
 * @param vmin index from which frame changes starts
 * @param vmax where they end
 * 
 * @details Provides pointer render buffer and it's pitch* to RE_CopyFrame.
 * When applies anisotropic to changed space and finally flushes it to the
 * screen. Flips renderer swap buffers and in the end calls to
 * VID_NoDamageBuffer.
 * 
 * *pitch - number of pixels per line of current render (eq screen) area
 * 
 */
void RE_FlushFrame(int vmin, int vmax);

/**
 * @brief Sets all buffers to 0 without flushing them to the screen
 * 
 */
void RE_CleanFrame(void);

/**
 * @brief Shutdown refresher's context
 * 
 * @details Frees all memory allocated by frontend and renderer, returns state
 * of SW to prior of RE_InitContext call 
 * 
 */
void RE_ShutdownContext(void);

/**
 * @brief Translates `sw_state`'s `gammatable` to `currentpalette` 
 * using `palette`
 * 
 * @param palette array of lenght 1024 with structure [R,G,B,_,R,G,B,_,...] of
 * indeces to gammatable
 * 
 */
void R_GammaCorrectAndSetPalette(const unsigned char *palette);
// -----------------------------------------------------------------------------
#endif

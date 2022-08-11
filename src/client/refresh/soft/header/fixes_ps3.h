#ifndef _FIXES_PS3_H_
#define _FIXES_PS3_H_

#include "local.h"

/**
 * \brief Get the number of milliseconds since the first call.
 * \details Replcamenet for SDL_GetTicks()
 *
 * \note This value wraps if the program runs for more than ~49 days.
 */
uint32_t getTicks(void);

#endif

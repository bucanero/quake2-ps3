This repository contains some work on porting parts of Yamagi's Quake II to PS3

Notable parts:
 - `backends/ps3/system.c`
 - `backends/ps3/network_loopback.c` - platform independent loopback placeholder
 - `client/input/input_ps3.c` - SDL-less PS3's pad handling
 - `common/shared/shared_ps3.c` - line 1184: precooking 'in folder' paths for fopen
 - `client/sound/sound_backend_ps3.c` - based on sdl.c sound backend for ps3
 - `client/sound/sound_ps3.c` - replacement for sound.c setted to use only ps3's sound backend
 - `client/vid/glimp_ps3.c` - replacement for glimp\_sdl.c for video output
 - `client/refresh/sw_ps3_fixes` - replacement for SDL\_GetTicks to sync frames
 - `client/refresh/sw_ps3_main.c` and `client/refresh/sw_ps3_gcm.c` - replaces fixed to SDL sw\_main.c to be (front/back)end realisation with sw\_ps3\_gcm.c as frontend
 - `client/refresh/sw_ps3_misc.c` - replacement for sw\_misc.c just to use own getTicks

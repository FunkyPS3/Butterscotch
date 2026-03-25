#pragma once

#if defined(__PPU__) || defined(__CELLOS_LV2__) || defined(PSL1GHT)
    #define PLATFORM_PS3 1
#else
    #define PLATFORM_PS3 0
#endif
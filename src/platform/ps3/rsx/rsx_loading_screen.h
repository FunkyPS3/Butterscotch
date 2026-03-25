#pragma once

#include <stdint.h>
#include <rsx/gcm_sys.h>
#include "rsxutil.h"

typedef struct LoadingScreen {
    const char* game_name;
    const char* title_text;
    const char* status_text;
    float progress;
    int ready;
} LoadingScreen;

int LoadingScreen_init(LoadingScreen* loading, const char* game_name);

void LoadingScreen_setStatus(LoadingScreen* loading,
                                const char* status_text,
                                float progress);

void LoadingScreen_render(LoadingScreen* loading,
                             gcmContextData* context,
                             rsxBuffer* buffer,
                             uint16_t width,
                             uint16_t height);

void LoadingScreen_shutdown(LoadingScreen* loading);
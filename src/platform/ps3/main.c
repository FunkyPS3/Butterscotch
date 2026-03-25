#include <ppu-lv2.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/process.h>
#include <sysutil/video.h>
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>
#include <io/pad.h>
#include <sysutil/sysutil.h>

#include "rsx/rsxutil.h"
#include "data_win.h"
#include "vm.h"
#include "runner.h"
#include "renderer/soft_renderer.h"
#include "sys/ps3_time.h"
#include "runner_keyboard.h"
#include "noop_file_system.h"
#include "input/pad_mapping.h"
#include "core/log.h"
#include "rsx/rsx_loading_screen.h"

#ifndef PS3_DATA_WIN_PATH
#define PS3_DATA_WIN_PATH "/dev_hdd0/game/DEFAULT/USRDIR/data.win"
#endif

#define MAX_BUFFERS 2

static const int PAD_MAPPING_COUNT = sizeof(PAD_MAPPINGS) / sizeof(PAD_MAPPINGS[0]);
static bool prevState[sizeof(PAD_MAPPINGS) / sizeof(PAD_MAPPINGS[0])] = {0};

static void loadingCallback(const char *chunkName, int chunkIndex, int totalChunks,
                            DataWin *dataWin, void *userData)
{
    (void)dataWin;
    (void)userData;
    if (chunkName && chunkIndex >= 0 && totalChunks > 0)
    {
        fprintf(stderr, "Loading chunk %d/%d: %s\n", chunkIndex + 1, totalChunks, chunkName);
    }
}

static double getTimeSeconds(void)
{
    struct timespec ts;
    ps3_clock_gettime(&ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

DataWin *loadDataWin()
{
    DataWin *dataWin = DataWin_parse(
        PS3_DATA_WIN_PATH,
        (DataWinParserOptions){
            .parseGen8 = true,
            .parseOptn = true,
            .parseLang = true,
            .parseExtn = true,
            .parseSond = true,
            .parseAgrp = true,
            .parseSprt = true,
            .parseBgnd = true,
            .parsePath = true,
            .parseScpt = true,
            .parseGlob = true,
            .parseShdr = true,
            .parseFont = true,
            .parseTmln = true,
            .parseObjt = true,
            .parseRoom = true,
            .parseTpag = true,
            .parseCode = true,
            .parseVari = true,
            .parseFunc = true,
            .parseStrg = true,
            .parseTxtr = true,
            .loadTxtrBlobData = false,
            .parseAudo = false, // We don't need to parse audio data yet, because our implementation doesn't support audio yet :3
            .skipLoadingPreciseMasksForNonPreciseSprites = true,
            .progressCallback = loadingCallback,
            .progressCallbackUserData = NULL,
        });

    return dataWin;
}

static void sysutil_callback(u64 status, u64 param, void *usrdata)
{
    (void)param;

    switch (status)
    {
    // If we didn't handle this callback, the GameOS will restart forcefully
    case SYSUTIL_EXIT_GAME:
    {
        Runner *runner = (Runner *)usrdata;
        if (runner != NULL)
        {
            runner->shouldExit = true;
        }
        // Bye bye :3
        sysProcessExit(0);

        break;
    }

    default:
    {
        break;
    }
    }
}

static void loading_step(LoadingScreen *loading,
                         gcmContextData *context,
                         rsxBuffer *buffers,
                         int *currentBuffer,
                         u16 width,
                         u16 height,
                         const char *status,
                         float progress)
{
    LoadingScreen_setStatus(loading, status, progress);
    LoadingScreen_render(loading, context, &buffers[*currentBuffer], width, height);
    waitFlip();
    flip(context, buffers[*currentBuffer].id);
    *currentBuffer ^= 1;
}

static Renderer *createPlatformRenderer(DataWin *dataWin, gcmContextData *context)
{
    (void)context;
    fprintf(stderr, "PS3: using SoftRenderer backend\n");
    return SoftRenderer_create(dataWin);
}

static void preloadRendererRoom(Renderer *renderer, Room *room)
{
    SoftRenderer_preloadRoom(renderer, room);
}

static void bindRendererBuffer(Renderer *renderer,
                               rsxBuffer *buffer,
                               int width,
                               int height,
                               int gameWidth,
                               int gameHeight)
{
    SoftRenderer_setBuffer(renderer, buffer->ptr, width, height, gameWidth, gameHeight);
}

static void destroyRenderer(Renderer *renderer)
{
    if (renderer == NULL)
    {
        return;
    }

    if (renderer->vtable != NULL && renderer->vtable->destroy != NULL)
    {
        renderer->vtable->destroy(renderer);
    }
}

int main(void)
{
    sysUtilRegisterCallback(0, sysutil_callback, NULL);

    void *host_addr = memalign(1024 * 1024, HOST_SIZE);

    if (!host_addr)
    {
        logger("Butterscotch", "FATAL: failed to allocate RSX host memory");
        return 1;
    }
    gcmContextData *context = initScreen(host_addr, HOST_SIZE);

    if (!context)
    {
        logger("Butterscotch", "FATAL: failed to initialize screen");
        return 1;
    }

    u16 width, height;
    getResolution(&width, &height);

    rsxBuffer buffers[MAX_BUFFERS];
    for (int i = 0; i < MAX_BUFFERS; i++)
    {
        makeBuffer(&buffers[i], width, height, i);
        clearBuffer(&buffers[i], 0xFF000000u);
    }

    int currentBuffer = 0;
    flip(context, MAX_BUFFERS - 1);
    LoadingScreen loading;
    LoadingScreen_init(&loading, "Butterscotch4PS3");
    ioPadInit(7);

    loading_step(&loading, context, buffers, &currentBuffer, width, height,
                 "Loading data.win...", 0.20f);

    DataWin *dataWin = loadDataWin();

    if (!dataWin)
    {
        loading_step(&loading, context, buffers, &currentBuffer, width, height,
                     "Failed to load data.win", 0.10f);
        logger("Butterscotch", "FATAL: failed to load data.win");
        return 1;
    }

    // Create a VM and a runner
    logger(dataWin->gen8.displayName, "Creating VM and runner...");
    loading_step(&loading, context, buffers, &currentBuffer, width, height,
                 "Creating VM and runner...", 0.50f);
    FileSystem *fs = NoopFileSystem_create();
    VMContext *vm = VM_create(dataWin);
    Runner *runner = Runner_create(dataWin, vm, fs);
    loading_step(&loading, context, buffers, &currentBuffer, width, height,
                 "Launching game...", 0.90f);

    // Attach a renderer
    Renderer *renderer = createPlatformRenderer(dataWin, context);
    runner->renderer = renderer;

    // Initialize the first room before entering the main loop
    Runner_initFirstRoom(runner);
    preloadRendererRoom(renderer, runner->currentRoom);

    int32_t lastPreloadedRoomIndex = runner->currentRoomIndex;

    double lastFrameTime = getTimeSeconds();

    // Main loop
    while (!runner->shouldExit)
    {
        sysUtilCheckCallback();

        RunnerKeyboard_beginFrame(runner->keyboard);

        padInfo padinfo;
        ioPadGetInfo(&padinfo);

        if (padinfo.status[0])
        {
            padData paddata;
            ioPadGetData(0, &paddata);

            for (int i = 0; i < PAD_MAPPING_COUNT; i++)
            {
                uint8_t byte = (uint8_t)paddata.button[PAD_MAPPINGS[i].digital];
                uint8_t mask = PAD_MAPPINGS[i].mask;
                int32_t gmlKey = PAD_MAPPINGS[i].gmlKey;

                bool isPressed = (byte & mask) != 0;
                bool wasPressed = prevState[i];

                if (isPressed && !wasPressed)
                {
                    RunnerKeyboard_onKeyDown(runner->keyboard, gmlKey);
                }
                else if (!isPressed && wasPressed)
                {
                    RunnerKeyboard_onKeyUp(runner->keyboard, gmlKey);
                }

                prevState[i] = isPressed;
            }
        }

        if (RunnerKeyboard_checkPressed(runner->keyboard, VK_F10))
        {
            int32_t interactVarId = shget(runner->vmContext->globalVarNameMap, "interact");

            runner->vmContext->globalVars[interactVarId] = RValue_makeInt32(0);
        }

        Runner_step(runner);

        if (runner->currentRoomIndex != lastPreloadedRoomIndex)
        {
            preloadRendererRoom(renderer, runner->currentRoom);
            lastPreloadedRoomIndex = runner->currentRoomIndex;
        }

        // Render the frame
        bindRendererBuffer(renderer,
                           &buffers[currentBuffer],
                           width,
                           height,
                           dataWin->gen8.defaultWindowWidth,
                           dataWin->gen8.defaultWindowHeight);

        renderer->vtable->beginFrame(renderer,
                                     dataWin->gen8.defaultWindowWidth,
                                     dataWin->gen8.defaultWindowHeight,
                                     width, height);

        if (runner->drawBackgroundColor)
        {
            uint32_t bg = runner->backgroundColor;
            uint8_t r = (bg) & 0xFF;
            uint8_t g = (bg >> 8) & 0xFF;
            uint8_t b = (bg >> 16) & 0xFF;
            uint32_t px = 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
            clearBuffer(&buffers[currentBuffer], px);
        }
        else
        {
            clearBuffer(&buffers[currentBuffer], 0xFF000000u);
        }

        Room *room = runner->currentRoom;
        int32_t gameW = dataWin->gen8.defaultWindowWidth;
        int32_t gameH = dataWin->gen8.defaultWindowHeight;
        bool viewsEnabled = (room->flags & 1) != 0;
        bool anyViewRendered = false;

        if (viewsEnabled)
        {
            for (int vi = 0; vi < 8; vi++)
            {
                if (!room->views[vi].enabled)
                    continue;

                RoomView *v = &room->views[vi];
                runner->viewCurrent = vi;
                renderer->vtable->beginView(renderer,
                                            v->viewX, v->viewY, v->viewWidth, v->viewHeight,
                                            v->portX, v->portY, v->portWidth, v->portHeight,
                                            runner->viewAngles[vi]);
                Runner_draw(runner);
                renderer->vtable->endView(renderer);
                anyViewRendered = true;
            }
        }

        if (!anyViewRendered)
        {
            runner->viewCurrent = 0;
            renderer->vtable->beginView(renderer,
                                        0, 0, gameW, gameH,
                                        0, 0, gameW, gameH,
                                        0.0f);
            Runner_draw(runner);
            renderer->vtable->endView(renderer);
        }

        runner->viewCurrent = 0;

        renderer->vtable->endFrame(renderer);

        waitFlip();
        flip(context, buffers[currentBuffer].id);
        currentBuffer ^= 1;

        uint32_t roomSpeed = runner->currentRoom->speed;
        double targetFrameTime = (roomSpeed > 0) ? (1.0 / (double)roomSpeed) : (1.0 / 60.0);
        double nextFrameTime = lastFrameTime + targetFrameTime;
        double now = getTimeSeconds();
        double remaining = nextFrameTime - now;

        if (remaining > 0.002)
        {
            usleep((useconds_t)((remaining - 0.001) * 1000000.0));
        }

        while ((now = getTimeSeconds()) < nextFrameTime)
        {
        }

        if (now > nextFrameTime)
        {
            lastFrameTime = now;
        }
        else
        {
            lastFrameTime = nextFrameTime;
        }
    }

    logger("Butterscotch", "Exiting main loop, cleaning up...");
    destroyRenderer(renderer);
    DataWin_free(dataWin);
    NoopFileSystem_destroy(fs);
    ioPadEnd();

    return 0;
}

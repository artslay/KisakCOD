#include <cstdio>
#include <switch.h>

#include <qcommon/qcommon.h>
#include <qcommon/threads.h>
#include <universal/com_files.h>
#include <universal/profile.h>
#include <universal/timing.h>
#include <win32/win_local.h>

extern void Com_InitParse();
extern void Dvar_Init();

static void SwitchBootLog(const char *message)
{
    std::printf("[KisakCOD][BOOT] %s\n", message);
    std::fflush(stdout);
}

int main()
{
    SwitchBootLog("========================================");
    SwitchBootLog("KisakCOD Switch SP starting");
    SwitchBootLog("NRO entrypoint reached");
    SwitchBootLog("Game data: sdmc:/switch/KisakCOD/game");
    SwitchBootLog("Stage 1/7: initializing main thread");

    Sys_InitMainThread();
    SwitchBootLog("Stage 2/7: initializing parser");
    Com_InitParse();

    SwitchBootLog("Stage 3/7: initializing dvars");
    Dvar_Init();

    SwitchBootLog("Stage 4/7: initializing timing");
    InitTiming();

    SwitchBootLog("Stage 5/7: initializing profile");
    Profile_Init();

    SwitchBootLog("Stage 6/7: initializing game engine");
    Com_Init((char*)"");

    SwitchBootLog("Stage 7/7: engine initialized");
    SwitchBootLog("Entering applet/frame loop");

    while (appletMainLoop())
        Com_Frame();

    SwitchBootLog("Applet loop stopped, shutting down");
    Sys_Quit();
    return 0;
}

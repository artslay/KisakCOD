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

int main()
{
    std::printf("KisakCOD Switch SP\n");
    std::printf("Code: NRO | Game data: sdmc:/switch/KisakCOD/game\n");

    // PC WinMain-equivalent bootstrap. Every implementation below is linked
    // into the NRO; the filesystem only supplies the original game data.
    Sys_InitMainThread();
    Com_InitParse();
    Dvar_Init();
    InitTiming();
    Profile_Init();

    Com_Init((char*)"");


    while (appletMainLoop())
        Com_Frame();

    Sys_Quit();
    return 0;
}

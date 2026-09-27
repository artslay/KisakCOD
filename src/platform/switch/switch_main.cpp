#include <cstdio>

#ifdef __SWITCH__
#include <switch.h>
#endif

int main(int argc, char** argv)
{
#ifdef __SWITCH__
    consoleInit(NULL);
    printf("KisakCOD Switch bootstrap\\n");
    printf("Graphics backend bootstrap: OpenGL/Mesa\\n");
    printf("argc: %d\\n", argc);
    printf("Press + to exit.\\n");

    while (appletMainLoop())
    {
        hidScanInput();

        const u64 kDown = hidKeysDown(CONTROLLER_P1_AUTO);
        if (kDown & KEY_PLUS)
            break;

        consoleUpdate(NULL);
    }

    consoleExit(NULL);
#else
    (void)argc;
    (void)argv;
    std::puts("KisakCOD Switch bootstrap requires a Switch toolchain.");
#endif

    return 0;
}

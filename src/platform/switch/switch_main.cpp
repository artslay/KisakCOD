#include <cstdio>
#include <memory>

#include <switch.h>

#include <gfx/gfx_backend.h>\n#include <universal/com_files.h>\n\nextern void __cdecl FS_Startup(char *gameName);

int main()
{
    std::printf("KisakCOD Switch bootstrap\n");
    std::printf("Initializing OpenGL through Mesa/EGL...\n");

    auto backend = CreateOpenGLBackend();
    if (!backend)
    {
        std::printf("CreateOpenGLBackend failed\n");
        return 1;
    }

    if (!backend->Init(nullptr))
    {
        std::printf("OpenGL initialization failed: %s\n", backend->GetLastError());
        return 1;
    }

    // Mount the original game data from the SD card. The executable stays in the NRO;\n    // only original game assets are read from sdmc:/switch/KisakCOD/game.\n    FS_Startup((char*)"main");\n    const int commonFfSize = FS_ReadFile("common.ff", nullptr);\n    std::printf("common.ff: %d bytes\\n", commonFfSize);\n    if (commonFfSize < 0)\n        std::printf("Game data not found under sdmc:/switch/KisakCOD/game/main\\n");\n\n    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    PadState pad;
    padInitializeDefault(&pad);

    while (appletMainLoop())
    {
        padUpdate(&pad);

        if (padGetButtonsDown(&pad) & HidNpadButton_Plus)
            break;

        backend->Clear(0.08f, 0.12f, 0.20f, 1.0f);
        backend->Present();
    }

    backend->Shutdown();
    return 0;
}

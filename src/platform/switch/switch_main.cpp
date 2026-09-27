#include <cstdio>
#include <memory>

#include <switch.h>

#include <gfx/gfx_backend.h>

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

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

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

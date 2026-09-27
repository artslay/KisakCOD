#include "gfx_backend.h"
#include "opengl/gl_backend.h"

std::unique_ptr<IGfxBackend> g_gfxBackend;

std::unique_ptr<IGfxBackend> CreateOpenGLBackend()
{
    return std::make_unique<OpenGLBackend>();
}

std::unique_ptr<IGfxBackend> CreateDirectX9Backend()
{
    return nullptr;
}

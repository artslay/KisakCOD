#include "gl_backend.h"

#ifdef __SWITCH__
#include <switch.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <glad/glad.h>
#endif

#ifdef __SWITCH__
namespace
{
EGLDisplay s_display = EGL_NO_DISPLAY;
EGLContext s_context = EGL_NO_CONTEXT;
EGLSurface s_surface = EGL_NO_SURFACE;
}
#endif

OpenGLBackend::OpenGLBackend() = default;

OpenGLBackend::~OpenGLBackend()
{
    Shutdown();
}

bool OpenGLBackend::Init(const GfxWindowParms* wndParms)
{
    m_lastError.clear();

    if (!CreateWindow(const_cast<GfxWindowParms*>(wndParms)))
        return false;

    if (!InitContext(wndParms))
        return false;

    if (!InitCapabilities())
    {
        Shutdown();
        return false;
    }

    return true;
}

void OpenGLBackend::Shutdown()
{
#ifdef __SWITCH__
    if (s_display != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(s_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

        if (s_context != EGL_NO_CONTEXT)
            eglDestroyContext(s_display, s_context);

        if (s_surface != EGL_NO_SURFACE)
            eglDestroySurface(s_display, s_surface);

        eglTerminate(s_display);
    }

    s_display = EGL_NO_DISPLAY;
    s_context = EGL_NO_CONTEXT;
    s_surface = EGL_NO_SURFACE;
#endif

    m_window = nullptr;
    m_vertexArrayObject = 0;
    m_currentProgram = 0;
    m_deviceLost = false;
}

bool OpenGLBackend::RecoverLostDevice()
{
    m_deviceLost = false;
    return true;
}

bool OpenGLBackend::CreateWindow(GfxWindowParms* wndParms)
{
    (void)wndParms;

#ifdef __SWITCH__
    m_window = nwindowGetDefault();
    return m_window != nullptr;
#else
    return false;
#endif
}

void OpenGLBackend::DestroyWindow()
{
    m_window = nullptr;
}

void OpenGLBackend::Present()
{
#ifdef __SWITCH__
    if (s_display != EGL_NO_DISPLAY && s_surface != EGL_NO_SURFACE)
        eglSwapBuffers(s_display, s_surface);
#endif
}

void OpenGLBackend::BeginScene()
{
}

void OpenGLBackend::EndScene()
{
}

void OpenGLBackend::Clear(float r, float g, float b, float a)
{
#ifdef __SWITCH__
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
#else
    (void)r;
    (void)g;
    (void)b;
    (void)a;
#endif
}

void OpenGLBackend::DrawPrimitive(uint32_t primitiveType, uint32_t startVertex, uint32_t primitiveCount)
{
    (void)primitiveType;
    (void)startVertex;
    (void)primitiveCount;
}

void OpenGLBackend::DrawIndexedPrimitive(uint32_t primitiveType, uint32_t minIndex, uint32_t numVertices,
                                         uint32_t startIndex, uint32_t primitiveCount)
{
    (void)primitiveType;
    (void)minIndex;
    (void)numVertices;
    (void)startIndex;
    (void)primitiveCount;
}

void* OpenGLBackend::CreateVertexBuffer(uint32_t size, uint32_t usage)
{
    (void)size;
    (void)usage;
    return nullptr;
}

void* OpenGLBackend::CreateIndexBuffer(uint32_t size, uint32_t usage)
{
    (void)size;
    (void)usage;
    return nullptr;
}

void OpenGLBackend::ReleaseVertexBuffer(void* buffer) { (void)buffer; }
void OpenGLBackend::ReleaseIndexBuffer(void* buffer) { (void)buffer; }
void* OpenGLBackend::LockVertexBuffer(void* buffer, uint32_t flags) { (void)buffer; (void)flags; return nullptr; }
void OpenGLBackend::UnlockVertexBuffer(void* buffer) { (void)buffer; }
void* OpenGLBackend::LockIndexBuffer(void* buffer, uint32_t flags) { (void)buffer; (void)flags; return nullptr; }
void OpenGLBackend::UnlockIndexBuffer(void* buffer) { (void)buffer; }

void* OpenGLBackend::CreateTexture(uint32_t width, uint32_t height, uint32_t format)
{
    (void)width; (void)height; (void)format;
    return nullptr;
}

void OpenGLBackend::ReleaseTexture(void* texture) { (void)texture; }
void OpenGLBackend::SetTexture(uint32_t stage, void* texture) { (void)stage; (void)texture; }

void* OpenGLBackend::CreateRenderTarget(uint32_t width, uint32_t height, uint32_t format)
{
    (void)width; (void)height; (void)format;
    return nullptr;
}

void OpenGLBackend::ReleaseRenderTarget(void* rt) { (void)rt; }
void OpenGLBackend::SetRenderTarget(uint32_t rtIndex, void* rt) { (void)rtIndex; (void)rt; }
void* OpenGLBackend::GetRenderTarget(uint32_t rtIndex) { (void)rtIndex; return nullptr; }

void* OpenGLBackend::CreateVertexShader(const void* bytecode, uint32_t size)
{
    (void)bytecode; (void)size;
    return nullptr;
}

void* OpenGLBackend::CreatePixelShader(const void* bytecode, uint32_t size)
{
    (void)bytecode; (void)size;
    return nullptr;
}

void OpenGLBackend::ReleaseShader(void* shader) { (void)shader; }
void OpenGLBackend::SetVertexShader(void* shader) { (void)shader; }
void OpenGLBackend::SetPixelShader(void* shader) { (void)shader; }

void OpenGLBackend::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
#ifdef __SWITCH__
    glViewport(static_cast<GLint>(x), static_cast<GLint>(y),
               static_cast<GLsizei>(width), static_cast<GLsizei>(height));
#else
    (void)x; (void)y; (void)width; (void)height;
#endif
}

void OpenGLBackend::SetScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
#ifdef __SWITCH__
    glScissor(static_cast<GLint>(x), static_cast<GLint>(y),
              static_cast<GLsizei>(width), static_cast<GLsizei>(height));
#else
    (void)x; (void)y; (void)width; (void)height;
#endif
}

void OpenGLBackend::SetBlendState(uint32_t srcBlend, uint32_t destBlend)
{
    (void)srcBlend;
    (void)destBlend;
}

void OpenGLBackend::SetDepthState(bool depthEnable, bool depthWrite)
{
#ifdef __SWITCH__
    if (depthEnable)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    glDepthMask(depthWrite ? GL_TRUE : GL_FALSE);
#else
    (void)depthEnable;
    (void)depthWrite;
#endif
}

void OpenGLBackend::SetCullMode(uint32_t cullMode)
{
    (void)cullMode;
}

void* OpenGLBackend::CreateQuery(uint32_t queryType)
{
    (void)queryType;
    return nullptr;
}

void OpenGLBackend::ReleaseQuery(void* query) { (void)query; }
void OpenGLBackend::BeginQuery(void* query) { (void)query; }
void OpenGLBackend::EndQuery(void* query) { (void)query; }

bool OpenGLBackend::GetQueryResult(void* query, uint64_t* result)
{
    (void)query;
    if (result)
        *result = 0;
    return false;
}

void OpenGLBackend::WaitForGpu()
{
#ifdef __SWITCH__
    glFinish();
#endif
}

void OpenGLBackend::Flush()
{
#ifdef __SWITCH__
    glFlush();
#endif
}

bool OpenGLBackend::GetBackBufferDesc(uint32_t* width, uint32_t* height, uint32_t* format) const
{
    if (width) *width = m_backBufferWidth;
    if (height) *height = m_backBufferHeight;
    if (format) *format = m_backBufferFormat;
    return m_backBufferWidth != 0 && m_backBufferHeight != 0;
}

bool OpenGLBackend::InitContext(const GfxWindowParms* wndParms)
{
    (void)wndParms;

#ifdef __SWITCH__
    s_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (s_display == EGL_NO_DISPLAY)
    {
        m_lastError = "eglGetDisplay failed";
        return false;
    }

    EGLint major = 0;
    EGLint minor = 0;
    if (eglInitialize(s_display, &major, &minor) == EGL_FALSE)
    {
        m_lastError = "eglInitialize failed";
        return false;
    }

    if (eglBindAPI(EGL_OPENGL_API) == EGL_FALSE)
    {
        m_lastError = "eglBindAPI(EGL_OPENGL_API) failed";
        return false;
    }

    static const EGLint configAttributes[] =
    {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };

    EGLConfig config = nullptr;
    EGLint numConfigs = 0;
    if (eglChooseConfig(s_display, configAttributes, &config, 1, &numConfigs) == EGL_FALSE ||
        numConfigs == 0)
    {
        m_lastError = "eglChooseConfig failed";
        return false;
    }

    s_surface = eglCreateWindowSurface(
        s_display, config, static_cast<EGLNativeWindowType>(m_window), nullptr);

    if (s_surface == EGL_NO_SURFACE)
    {
        m_lastError = "eglCreateWindowSurface failed";
        return false;
    }

    static const EGLint contextAttributes[] =
    {
        EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
        EGL_CONTEXT_MAJOR_VERSION_KHR, 4,
        EGL_CONTEXT_MINOR_VERSION_KHR, 3,
        EGL_NONE
    };

    s_context = eglCreateContext(
        s_display, config, EGL_NO_CONTEXT, contextAttributes);

    if (s_context == EGL_NO_CONTEXT)
    {
        m_lastError = "eglCreateContext failed";
        return false;
    }

    if (eglMakeCurrent(s_display, s_surface, s_surface, s_context) == EGL_FALSE)
    {
        m_lastError = "eglMakeCurrent failed";
        return false;
    }

    return true;
#else
    m_lastError = "OpenGL backend is only initialized on Switch";
    return false;
#endif
}

bool OpenGLBackend::InitCapabilities()
{
#ifdef __SWITCH__
    if (!gladLoadGL())
    {
        m_lastError = "gladLoadGL failed";
        return false;
    }

    m_backBufferWidth = 1280;
    m_backBufferHeight = 720;
    m_backBufferFormat = 0;

    return true;
#else
    return false;
#endif
}

void OpenGLBackend::LogGLError(const char* context)
{
#ifdef __SWITCH__
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR)
        m_lastError = context ? context : "OpenGL error";
#else
    (void)context;
#endif
}

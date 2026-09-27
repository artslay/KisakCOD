#include "gl_backend.h"

OpenGLBackend::OpenGLBackend() = default;
OpenGLBackend::~OpenGLBackend()
{
    Shutdown();
}

bool OpenGLBackend::Init(const GfxWindowParms* wndParms)
{
    (void)wndParms;
    m_lastError.clear();
    return InitContext(wndParms) && InitCapabilities();
}

void OpenGLBackend::Shutdown()
{
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
    return true;
}

void OpenGLBackend::DestroyWindow()
{
    m_window = nullptr;
}

void OpenGLBackend::Present()
{
}

void OpenGLBackend::BeginScene()
{
}

void OpenGLBackend::EndScene()
{
}

void OpenGLBackend::Clear(float r, float g, float b, float a)
{
    (void)r;
    (void)g;
    (void)b;
    (void)a;
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
    (void)width; (void)height; (void)format; return nullptr;
}

void OpenGLBackend::ReleaseTexture(void* texture) { (void)texture; }
void OpenGLBackend::SetTexture(uint32_t stage, void* texture) { (void)stage; (void)texture; }

void* OpenGLBackend::CreateRenderTarget(uint32_t width, uint32_t height, uint32_t format)
{
    (void)width; (void)height; (void)format; return nullptr;
}

void OpenGLBackend::ReleaseRenderTarget(void* rt) { (void)rt; }
void OpenGLBackend::SetRenderTarget(uint32_t rtIndex, void* rt) { (void)rtIndex; (void)rt; }
void* OpenGLBackend::GetRenderTarget(uint32_t rtIndex) { (void)rtIndex; return nullptr; }

void* OpenGLBackend::CreateVertexShader(const void* bytecode, uint32_t size)
{
    (void)bytecode; (void)size; return nullptr;
}

void* OpenGLBackend::CreatePixelShader(const void* bytecode, uint32_t size)
{
    (void)bytecode; (void)size; return nullptr;
}

void OpenGLBackend::ReleaseShader(void* shader) { (void)shader; }
void OpenGLBackend::SetVertexShader(void* shader) { (void)shader; }
void OpenGLBackend::SetPixelShader(void* shader) { (void)shader; }

void OpenGLBackend::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    (void)x; (void)y; (void)width; (void)height;
}

void OpenGLBackend::SetScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    (void)x; (void)y; (void)width; (void)height;
}

void OpenGLBackend::SetBlendState(uint32_t srcBlend, uint32_t destBlend)
{
    (void)srcBlend; (void)destBlend;
}

void OpenGLBackend::SetDepthState(bool depthEnable, bool depthWrite)
{
    (void)depthEnable; (void)depthWrite;
}

void OpenGLBackend::SetCullMode(uint32_t cullMode)
{
    (void)cullMode;
}

void* OpenGLBackend::CreateQuery(uint32_t queryType)
{
    (void)queryType; return nullptr;
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
}

void OpenGLBackend::Flush()
{
}

bool OpenGLBackend::GetBackBufferDesc(uint32_t* width, uint32_t* height, uint32_t* format) const
{
    if (width) *width = m_backBufferWidth;
    if (height) *height = m_backBufferHeight;
    if (format) *format = m_backBufferFormat;
    return false;
}

bool OpenGLBackend::InitContext(const GfxWindowParms* wndParms)
{
    (void)wndParms;
    // Real EGL/OpenGL initialization will be wired to Mesa Switch here.
    return true;
}

bool OpenGLBackend::InitCapabilities()
{
    return true;
}

void OpenGLBackend::LogGLError(const char* context)
{
    (void)context;
}

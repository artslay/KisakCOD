#pragma once

#ifdef __SWITCH__

#include <cstdint>
#include <vector>
#include <cstring>
#include <algorithm>
#include <glad/glad.h>

using HRESULT = int32_t;
using _D3DFORMAT = uint32_t;

constexpr HRESULT S_OK = 0;
constexpr HRESULT E_FAIL = -1;

// D3D9 format values are kept for asset compatibility; Switch maps them to GL.
constexpr uint32_t D3DFMT_A8 = 1;
constexpr uint32_t D3DFMT_A8R8G8B8 = 21;
constexpr uint32_t D3DFMT_X8R8G8B8 = 22;
constexpr uint32_t D3DFMT_A8L8 = 51;
constexpr uint32_t D3DFMT_L8 = 50;
constexpr uint32_t D3DFMT_D16 = 80;
constexpr uint32_t D3DFMT_D24S8 = 75;
constexpr uint32_t D3DFMT_D24X8 = 77;
constexpr uint32_t D3DFMT_G16R16F = 112;
constexpr uint32_t D3DFMT_R32F = 114;
constexpr uint32_t D3DFMT_DXT1 = 0x31545844u;
constexpr uint32_t D3DFMT_DXT3 = 0x33545844u;
constexpr uint32_t D3DFMT_DXT5 = 0x35545844u;

enum _D3DCUBEMAP_FACES : uint32_t {
    D3DCUBEMAP_FACE_POSITIVE_X = 0,
    D3DCUBEMAP_FACE_NEGATIVE_X = 1,
    D3DCUBEMAP_FACE_POSITIVE_Y = 2,
    D3DCUBEMAP_FACE_NEGATIVE_Y = 3,
    D3DCUBEMAP_FACE_POSITIVE_Z = 4,
    D3DCUBEMAP_FACE_NEGATIVE_Z = 5,
};

struct D3DVIEWPORT9
{
    uint32_t X, Y, Width, Height;
    float MinZ, MaxZ;
};

enum : uint32_t
{
    D3DPT_TRIANGLELIST = 4,
    D3DFILL_SOLID = 3,
    D3DFILL_WIREFRAME = 2,
    D3DFMT_UNKNOWN = 0,
    D3DFMT_INDEX16 = 101,
    D3DPOOL_DEFAULT = 0,
    D3DZB_FALSE = 0,
    D3DZB_TRUE = 1,
    D3DTEXF_NONE = 0,
    D3DTEXF_POINT = 1,
    D3DTEXF_LINEAR = 2,
};

#define KISAK_D3D_STATE(name) constexpr uint32_t name = __LINE__

KISAK_D3D_STATE(D3DRS_ZENABLE);
KISAK_D3D_STATE(D3DRS_FILLMODE);
KISAK_D3D_STATE(D3DRS_ZWRITEENABLE);
KISAK_D3D_STATE(D3DRS_ALPHATESTENABLE);
KISAK_D3D_STATE(D3DRS_SRCBLEND);
KISAK_D3D_STATE(D3DRS_DESTBLEND);
KISAK_D3D_STATE(D3DRS_CULLMODE);
KISAK_D3D_STATE(D3DRS_ZFUNC);
KISAK_D3D_STATE(D3DRS_ALPHAREF);
KISAK_D3D_STATE(D3DRS_ALPHAFUNC);
KISAK_D3D_STATE(D3DRS_ALPHABLENDENABLE);
KISAK_D3D_STATE(D3DRS_STENCILENABLE);
KISAK_D3D_STATE(D3DRS_STENCILFAIL);
KISAK_D3D_STATE(D3DRS_STENCILZFAIL);
KISAK_D3D_STATE(D3DRS_STENCILPASS);
KISAK_D3D_STATE(D3DRS_STENCILFUNC);
KISAK_D3D_STATE(D3DRS_STENCILREF);
KISAK_D3D_STATE(D3DRS_STENCILMASK);
KISAK_D3D_STATE(D3DRS_STENCILWRITEMASK);
KISAK_D3D_STATE(D3DRS_COLORWRITEENABLE);
KISAK_D3D_STATE(D3DRS_BLENDOP);
KISAK_D3D_STATE(D3DRS_SEPARATEALPHABLENDENABLE);
KISAK_D3D_STATE(D3DRS_SRCBLENDALPHA);
KISAK_D3D_STATE(D3DRS_DESTBLENDALPHA);
KISAK_D3D_STATE(D3DRS_BLENDOPALPHA);
KISAK_D3D_STATE(D3DRS_TWOSIDEDSTENCILMODE);
KISAK_D3D_STATE(D3DRS_CCW_STENCILFAIL);
KISAK_D3D_STATE(D3DRS_CCW_STENCILZFAIL);
KISAK_D3D_STATE(D3DRS_CCW_STENCILPASS);
KISAK_D3D_STATE(D3DRS_CCW_STENCILFUNC);
KISAK_D3D_STATE(D3DRS_DEPTHBIAS);
KISAK_D3D_STATE(D3DRS_SLOPESCALEDEPTHBIAS);
KISAK_D3D_STATE(D3DRS_ADAPTIVETESS_Y);

KISAK_D3D_STATE(D3DSAMP_ADDRESSU);
KISAK_D3D_STATE(D3DSAMP_ADDRESSV);
KISAK_D3D_STATE(D3DSAMP_ADDRESSW);
KISAK_D3D_STATE(D3DSAMP_MAGFILTER);
KISAK_D3D_STATE(D3DSAMP_MINFILTER);
KISAK_D3D_STATE(D3DSAMP_MIPFILTER);
KISAK_D3D_STATE(D3DSAMP_MAXANISOTROPY);

#undef KISAK_D3D_STATE

struct KisakGLBuffer
{
    GLuint object = 0;
    GLenum target = GL_ARRAY_BUFFER;
    std::vector<uint8_t> shadow;
    bool mapped = false;

    KisakGLBuffer(GLenum t, size_t size) : target(t), shadow(size)
    {
        glGenBuffers(1, &object);
        glBindBuffer(target, object);
        glBufferData(target, (GLsizeiptr)size, nullptr, GL_DYNAMIC_DRAW);
    }

    ~KisakGLBuffer()
    {
        if (object)
            glDeleteBuffers(1, &object);
    }

    HRESULT Lock(uint32_t offset, uint32_t size, void** out, uint32_t)
    {
        if (!out || offset > shadow.size())
            return E_FAIL;
        const size_t requested = size ? size : shadow.size() - offset;
        if (offset + requested > shadow.size())
            return E_FAIL;
        *out = shadow.data() + offset;
        mapped = true;
        return S_OK;
    }

    HRESULT Unlock()
    {
        if (mapped)
        {
            glBindBuffer(target, object);
            glBufferSubData(target, 0, (GLsizeiptr)shadow.size(), shadow.data());
            mapped = false;
        }
        return S_OK;
    }

    void Release() { delete this; }
};

using IDirect3DVertexBuffer9 = KisakGLBuffer;
using IDirect3DIndexBuffer9 = KisakGLBuffer;

struct KisakGLTexture
{
    GLuint object = 0;
    GLenum target = GL_TEXTURE_2D;
    GLenum internalFormat = GL_RGBA8;
    GLenum uploadFormat = GL_BGRA;
    GLenum uploadType = GL_UNSIGNED_BYTE;
    uint32_t width = 0, height = 0, depth = 1;
    uint32_t mipLevels = 1;
    _D3DFORMAT sourceFormat = D3DFMT_UNKNOWN;
    void Release()
    {
        if (object)
            glDeleteTextures(1, &object);
        delete this;
    }
};

using IDirect3DBaseTexture9 = KisakGLTexture;
using IDirect3DTexture9 = KisakGLTexture;
using IDirect3DVolumeTexture9 = KisakGLTexture;
using IDirect3DCubeTexture9 = KisakGLTexture;

struct IDirect3DSurface9
{
    KisakGLTexture *texture = nullptr;
    uint32_t level = 0;
    void Release() { delete this; }
};
struct IDirect3DQuery9 { void Release() { delete this; } };

class IDirect3DDevice9
{
public:
    HRESULT CreateVertexBuffer(uint32_t size, uint32_t, uint32_t, uint32_t, IDirect3DVertexBuffer9** out, void*)
    {
        if (!out) return E_FAIL;
        *out = new KisakGLBuffer(GL_ARRAY_BUFFER, size);
        return S_OK;
    }

    HRESULT CreateIndexBuffer(uint32_t size, uint32_t, _D3DFORMAT, uint32_t, IDirect3DIndexBuffer9** out, void*)
    {
        if (!out) return E_FAIL;
        *out = new KisakGLBuffer(GL_ELEMENT_ARRAY_BUFFER, size);
        return S_OK;
    }

    HRESULT SetIndices(IDirect3DIndexBuffer9* ib)
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib ? ib->object : 0);
        return S_OK;
    }

    HRESULT SetStreamSource(uint32_t, IDirect3DVertexBuffer9* vb, uint32_t offset, uint32_t)
    {
        glBindBuffer(GL_ARRAY_BUFFER, vb ? vb->object : 0);
        (void)offset;
        return S_OK;
    }

    HRESULT SetTexture(uint32_t stage, IDirect3DBaseTexture9* tex)
    {
        glActiveTexture(GL_TEXTURE0 + stage);
        glBindTexture(GL_TEXTURE_2D, tex ? tex->object : 0);
        return S_OK;
    }

    HRESULT SetViewport(const D3DVIEWPORT9* vp)
    {
        if (!vp) return E_FAIL;
        glViewport((GLint)vp->X, (GLint)vp->Y, (GLsizei)vp->Width, (GLsizei)vp->Height);
        glDepthRangef(vp->MinZ, vp->MaxZ);
        return S_OK;
    }

    HRESULT SetRenderTarget(uint32_t, void*) { return S_OK; }
    HRESULT SetDepthStencilSurface(void*) { return S_OK; }

    HRESULT SetSamplerState(uint32_t stage, uint32_t state, uint32_t value)
    {
        glActiveTexture(GL_TEXTURE0 + stage);
        switch (state)
        {
        case D3DSAMP_MINFILTER:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, value == D3DTEXF_POINT ? GL_NEAREST : GL_LINEAR);
            break;
        case D3DSAMP_MAGFILTER:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, value == D3DTEXF_POINT ? GL_NEAREST : GL_LINEAR);
            break;
        default:
            break;
        }
        return S_OK;
    }

    HRESULT SetRenderState(uint32_t state, uint32_t value)
    {
        switch (state)
        {
        case D3DRS_ZENABLE:
            if (value) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
            break;
        case D3DRS_ZWRITEENABLE:
            glDepthMask(value ? GL_TRUE : GL_FALSE);
            break;
        case D3DRS_ALPHABLENDENABLE:
            if (value) glEnable(GL_BLEND); else glDisable(GL_BLEND);
            break;
        case D3DRS_ALPHATESTENABLE:
            break;
        case D3DRS_CULLMODE:
            if (value == 1) { glEnable(GL_CULL_FACE); glCullFace(GL_FRONT); }
            else if (value == 2) { glEnable(GL_CULL_FACE); glCullFace(GL_BACK); }
            else glDisable(GL_CULL_FACE);
            break;
        case D3DRS_ZFUNC:
            glDepthFunc(value == 1 ? GL_NEVER : value == 2 ? GL_LESS : value == 3 ? GL_EQUAL : GL_LEQUAL);
            break;
        case D3DRS_FILLMODE:
            glPolygonMode(GL_FRONT_AND_BACK, value == D3DFILL_WIREFRAME ? GL_LINE : GL_FILL);
            break;
        default:
            break;
        }
        return S_OK;
    }

    HRESULT DrawIndexedPrimitive(uint32_t, uint32_t, uint32_t, uint32_t startIndex, uint32_t primitiveCount)
    {
        glDrawElements(GL_TRIANGLES, (GLsizei)(primitiveCount * 3), GL_UNSIGNED_SHORT,
                       reinterpret_cast<const void*>(uintptr_t(startIndex * sizeof(uint16_t))));
        return S_OK;
    }

    HRESULT Clear(uint32_t, uint32_t, uint32_t flags, uint32_t color, float depth, uint32_t stencil)
    {
        GLbitfield mask = 0;
        if (flags & 1) mask |= GL_DEPTH_BUFFER_BIT;
        if (flags & 2) mask |= GL_STENCIL_BUFFER_BIT;
        if (flags & 4) mask |= GL_COLOR_BUFFER_BIT;
        glClearColor(((color >> 16) & 0xff) / 255.0f, ((color >> 8) & 0xff) / 255.0f,
                     (color & 0xff) / 255.0f, ((color >> 24) & 0xff) / 255.0f);
        glClearDepthf(depth);
        glClearStencil((GLint)stencil);
        glClear(mask);
        return S_OK;
    }
};

#endif

#ifdef __SWITCH__

#include <algorithm>
#include <cstdint>
#include <GL/gl.h>
#include "binklib/binktextures.h"
#include "gfx/opengl/d3d9_compat.h"

static void UploadBinkPlane(IDirect3DTexture9 *texture, const BINKPLANE &plane, uint32_t width, uint32_t height)
{
    if (!texture || !plane.Buffer || !width || !height) return;
    glBindTexture(GL_TEXTURE_2D, texture->object);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)width, (GLsizei)height,
        texture->uploadFormat, texture->uploadType, plane.Buffer);
}

static void LockPlane(IDirect3DTexture9 *texture, BINKPLANE &plane, uint32_t width, uint32_t height)
{
    if (!texture || !plane.Allocate || !width || !height) {
        plane.Buffer = nullptr; plane.BufferPitch = 0; return;
    }
    texture->lockShadow.resize((size_t)width * height);
    texture->lockShadowActive = true;
    plane.Buffer = texture->lockShadow.data();
    plane.BufferPitch = width;
}

void Lock_Bink_textures(BINKTEXTURESET *set_textures)
{
    if (!set_textures) return;
    BINKFRAMEBUFFERS &buffers = set_textures->bink_buffers;
    for (int i = 0; i < buffers.TotalFrames; ++i) {
        BINKFRAMETEXTURES &textures = set_textures->textures[i];
        BINKFRAMEPLANESET &planes = buffers.Frames[i];
        LockPlane(textures.Ytexture, planes.YPlane, buffers.YABufferWidth, buffers.YABufferHeight);
        LockPlane(textures.cRtexture, planes.cRPlane, buffers.cRcBBufferWidth, buffers.cRcBBufferHeight);
        LockPlane(textures.cBtexture, planes.cBPlane, buffers.cRcBBufferWidth, buffers.cRcBBufferHeight);
        LockPlane(textures.Atexture, planes.APlane, buffers.YABufferWidth, buffers.YABufferHeight);
    }
}

void Unlock_Bink_textures(LPDIRECT3DDEVICE9, BINKTEXTURESET *set_textures, HBINK)
{
    if (!set_textures) return;
    BINKFRAMEBUFFERS &buffers = set_textures->bink_buffers;
    const int activeFrame = std::clamp<int>(buffers.FrameNum, 0, std::max(0, buffers.TotalFrames - 1));

    if (buffers.TotalFrames > 0) {
        BINKFRAMEPLANESET &activePlanes = buffers.Frames[activeFrame];
        BINKFRAMETEXTURES &draw = set_textures->tex_draw;
        UploadBinkPlane(draw.Ytexture, activePlanes.YPlane, buffers.YABufferWidth, buffers.YABufferHeight);
        UploadBinkPlane(draw.cRtexture, activePlanes.cRPlane, buffers.cRcBBufferWidth, buffers.cRcBBufferHeight);
        UploadBinkPlane(draw.cBtexture, activePlanes.cBPlane, buffers.cRcBBufferWidth, buffers.cRcBBufferHeight);
        UploadBinkPlane(draw.Atexture, activePlanes.APlane, buffers.YABufferWidth, buffers.YABufferHeight);
    }

    for (int i = 0; i < buffers.TotalFrames; ++i) {
        BINKFRAMETEXTURES &textures = set_textures->textures[i];
        BINKFRAMEPLANESET &planes = buffers.Frames[i];
        UploadBinkPlane(textures.Ytexture, planes.YPlane, buffers.YABufferWidth, buffers.YABufferHeight);
        UploadBinkPlane(textures.cRtexture, planes.cRPlane, buffers.cRcBBufferWidth, buffers.cRcBBufferHeight);
        UploadBinkPlane(textures.cBtexture, planes.cBPlane, buffers.cRcBBufferWidth, buffers.cRcBBufferHeight);
        UploadBinkPlane(textures.Atexture, planes.APlane, buffers.YABufferWidth, buffers.YABufferHeight);
        planes.YPlane.Buffer = planes.cRPlane.Buffer = planes.cBPlane.Buffer = planes.APlane.Buffer = nullptr;
        planes.YPlane.BufferPitch = planes.cRPlane.BufferPitch = planes.cBPlane.BufferPitch = planes.APlane.BufferPitch = 0;
        if (textures.Ytexture) textures.Ytexture->lockShadowActive = false;
        if (textures.cRtexture) textures.cRtexture->lockShadowActive = false;
        if (textures.cBtexture) textures.cBtexture->lockShadowActive = false;
        if (textures.Atexture) textures.Atexture->lockShadowActive = false;
    }
}

#endif

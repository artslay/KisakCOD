#pragma once

#ifdef __SWITCH__

#include <cstdint>
#include <vector>
#include <cstring>
#include <array>
#include <algorithm>

#ifndef __cdecl
#define __cdecl
#endif
#ifndef __stdcall
#define __stdcall
#endif
#ifndef __declspec
#define __declspec(x)
#endif

struct HINSTANCE__ {};
struct IDirect3DSwapChain9 { void Release() { delete this; } };
using _D3DMULTISAMPLE_TYPE = uint32_t;
#include <GL/gl.h>

using HRESULT = int32_t;
using _D3DFORMAT = uint32_t;
constexpr _D3DFORMAT D3DFMT_X8R8G8B8 = 22;
struct _D3DDISPLAYMODE { uint32_t Width=0, Height=0; uint32_t RefreshRate=60; _D3DFORMAT Format=D3DFMT_X8R8G8B8; };

constexpr HRESULT S_OK = 0;
constexpr HRESULT E_FAIL = -1;

// D3D9 format values are kept for asset compatibility; Switch maps them to GL.
constexpr uint32_t D3DFMT_A8 = 1;
constexpr uint32_t D3DFMT_A1R5G5B5 = 25;
constexpr uint32_t D3DFMT_R5G6B5 = 23;
constexpr uint32_t D3DFMT_A8B8G8R8 = 32;
constexpr uint32_t D3DFMT_D15S1 = 73;
constexpr uint32_t D3DFMT_D16_LOCKABLE = 70;
constexpr uint32_t D3DFMT_D24FS8 = 83;
constexpr uint32_t D3DMULTISAMPLE_NONE = 0;
constexpr uint32_t D3DDEVTYPE_HAL = 1;
constexpr uint32_t D3DRTYPE_SURFACE = 8;
constexpr uint32_t D3DBACKBUFFER_TYPE_MONO = 1;

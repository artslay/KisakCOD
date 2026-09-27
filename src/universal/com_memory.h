#pragma once

#include <script/scr_stringlist.h>
#include "q_shared.h"
#include <qcommon/mem_track.h>

#define HUNK_MAX_ALIGNEMT 4096

struct TempMemInfo // sizeof=0x28
{                                       // ...
    int permanent;
    int high;
    int highExtra;
    int hunkSize;
    int low;
    mem_track_t data;                   // ...
};

union XAssetHeader;

void __cdecl Hunk_AddAsset(XAssetHeader header, _DWORD *data);

void Com_TouchMemory();

uint8_t* __cdecl Hunk_AllocXAnimPrecache(uint32_t size);
uint8_t* __cdecl Hunk_AllocPhysPresetPrecache(uint32_t size);
void* __cdecl Hunk_AllocXAnimClient(int size);
uint8_t* __cdecl Hunk_AllocXAnimServer(uint32_t size);

uint8_t* __cdecl Hunk_AllocLow(uint32_t size, const char* name, int32_t type);
uint8_t* __cdecl Hunk_AllocLowAlign(uint32_t size, int32_t alignment, const char* name, int32_t type);

//void __cdecl TRACK_com_memory();

// LWSS: Note that the Z_ prefix comes from the fact that it uses the "Zone" memory pool.
// There are a few memory pools of fixed size that allocations come from.
void* __cdecl Z_VirtualReserve(int size);
void __cdecl Z_VirtualDecommitInternal(void* ptr, int size);
void* __cdecl Z_VirtualFreeInternal(void* ptr);
void* __cdecl Z_TryVirtualAllocInternal(int size);
bool __cdecl Z_TryVirtualCommitInternal(void* ptr, int size);
void __cdecl Z_VirtualCommitInternal(void* ptr, int size);
void __cdecl Z_VirtualFree(void* ptr);

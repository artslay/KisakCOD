#include <universal/q_shared.h>
#include "database.h"

#include <qcommon/files.h>
#ifdef __SWITCH__
#include <cstdio>
extern FILE *FS_SwitchOpenFile(const char *path);
extern FILE *FS_SwitchOpenRootFile(const char *path);
#endif
#include <qcommon/mem_track.h>

#include <xanim/xmodel.h>
#ifndef __SWITCH__
#include <win32/win_net.h>
#endif
#include <qcommon/threads.h>
#include <qcommon/com_bsp.h>
#include <gfx_d3d/r_init.h>
#ifndef __SWITCH__
#include <win32/win_local.h>
#endif
#include <gfx_d3d/rb_uploadshaders.h>
#include <gfx_d3d/r_image.h>
#include <universal/com_files.h>
#include <game/game_public.h>
#include <gfx_d3d/r_bsp.h>
#include <stringed/stringed_hooks.h>
#include <qcommon/cmd.h>
#include <universal/physicalmemory.h>
#include <gfx_d3d/rb_shade.h>
#include <gfx_d3d/r_staticmodelcache.h>
#ifndef __SWITCH__
#include <win32/win_localize.h>
#endif
#include <universal/profile.h>

#include <algorithm>

#include <setjmp.h>
#include <game/g_bsp.h>
#include <cgame/cg_local.h>

GfxWorld s_world;
MaterialGlobals materialGlobals;
ImgGlobals imageGlobals;
r_globals_t rg{ 0 };

struct DBReorderAssetEntry // sizeof=0x10
{                                       // ...
    uint32_t sequence;
    int32_t type;
    const char *typeString;
    const char *assetName;
};

#define POOLSIZE_XMODELPIECES   64
#define POOLSIZE_PHYSPRESET     64
#define POOLSIZE_XANIMPARTS     4096
#define POOLSIZE_XMODEL         1000
#define POOLSIZE_MATERIAL       2048
#define POOLSIZE_TECHNIQUE_SET  1024 // 512 on SP (XBox?)
#define POOLSIZE_IMAGE          2400
#define POOLSIZE_SOUND          16'000
#define POOLSIZE_SOUND_CURVE    64
#define POOLSIZE_LOADED_SOUND   1200
#define POOLSIZE_CLIPMAP        1
#define POOLSIZE_CLIPMAP_PVS    1
#define POOLSIZE_COMWORLD       1
#define POOLSIZE_GAMEWORLD_SP   1
#define POOLSIZE_GAMEWORLD_MP   1
#define POOLSIZE_MAP_ENTS       2
#define POOLSIZE_GFXWORLD       1
#define POOLSIZE_LIGHT_DEF      32
#define POOLSIZE_UI_MAP         0
#define POOLSIZE_FONT           16
#define POOLSIZE_MENULIST       128
#define POOLSIZE_MENU           640 // 512 on SP
#define POOLSIZE_LOCALIZE_ENTRY 6144
#define POOLSIZE_WEAPON         128
#define POOLSIZE_SNDDRIVER_GLOBALS 1
#define POOLSIZE_FX             400
#define POOLSIZE_IMPACT_FX      4
#define POOLSIZE_AITYPE         0
#define POOLSIZE_MPTYPE         0
#define POOLSIZE_CHARACTER      0
#define POOLSIZE_XMODELALIAS    0
#define POOLSIZE_RAWFILE        1024
#define POOLSIZE_STRINGTABLE    50

int32_t g_poolSize[ASSET_TYPE_COUNT] =
{
    POOLSIZE_XMODELPIECES,
    POOLSIZE_PHYSPRESET,
    POOLSIZE_XANIMPARTS,
    POOLSIZE_XMODEL,
    POOLSIZE_MATERIAL,
    POOLSIZE_TECHNIQUE_SET,
    POOLSIZE_IMAGE,
    POOLSIZE_SOUND,
    POOLSIZE_SOUND_CURVE,
    POOLSIZE_LOADED_SOUND,
    POOLSIZE_CLIPMAP,
    POOLSIZE_CLIPMAP_PVS,
    POOLSIZE_COMWORLD,
    POOLSIZE_GAMEWORLD_SP,
    POOLSIZE_GAMEWORLD_MP,
    POOLSIZE_MAP_ENTS,
    POOLSIZE_GFXWORLD,
    POOLSIZE_LIGHT_DEF,
    POOLSIZE_UI_MAP,
    POOLSIZE_FONT,
    POOLSIZE_MENULIST,
    POOLSIZE_MENU,
    POOLSIZE_LOCALIZE_ENTRY,
    POOLSIZE_WEAPON,
    POOLSIZE_SNDDRIVER_GLOBALS,
    POOLSIZE_FX,
    POOLSIZE_IMPACT_FX,
    POOLSIZE_AITYPE,
    POOLSIZE_MPTYPE,
    POOLSIZE_CHARACTER,
    POOLSIZE_XMODELALIAS,
    POOLSIZE_RAWFILE,
    POOLSIZE_STRINGTABLE,
}; // idb

bool g_archiveBuf;

// --- file-local forward declarations (moved out of database.h) ---
static void __cdecl DB_InitSingleton(void *pool, int32_t size);
static void __cdecl DB_RemoveClipMap(XAssetHeader ass);
static void __cdecl DB_RemoveComWorld(XAssetHeader ass);
static void __cdecl DB_RemoveGfxWorld(XAssetHeader ass);
static void __cdecl DB_DynamicCloneMenu(XAssetHeader from, XAssetHeader to, int32_t swag = 0);
static void __cdecl DB_RemoveWindowFocus(windowDef_t *window);
static XAssetHeader __cdecl DB_AllocMaterial(void *arg);
static void __cdecl DB_FreeMaterial(void *pool, XAssetHeader header);
static void __cdecl DB_Sleep(uint32_t msec);
static void __cdecl DB_LogMissingAsset(XAssetType type, const char *name);
static void __cdecl DB_RegisteredReorderAsset(int32_t type, const char *assetName, XAssetEntry *assetEntry);
static XAssetEntryPoolEntry *__cdecl DB_FindXAssetEntry(XAssetType type, const char *name);
static uint32_t __cdecl DB_HashForName(const char *name, XAssetType type);
static XAssetEntry *__cdecl DB_CreateDefaultEntry(XAssetType type, char *name);
static XAssetEntryPoolEntry *__cdecl DB_AllocXAssetEntry(XAssetType type, uint8_t zoneIndex);
static XAssetHeader __cdecl DB_AllocXAssetHeader(XAssetType type);
static void __cdecl DB_PrintAssetName(XAssetHeader header, int32_t *data);
static void __cdecl DB_CloneXAssetInternal(const XAsset *from, XAsset *to);
static XAssetHeader __cdecl DB_FindXAssetDefaultHeaderInternal(XAssetType type);
static void __cdecl PrintWaitedError(XAssetType type, const char *name, int32_t waitedMsec);
static bool __cdecl DB_GetInitializing();
static XAssetHeader __cdecl DB_AddXAsset(XAssetType type, XAssetHeader header);
static XAssetEntryPoolEntry *__cdecl DB_LinkXAssetEntry(XAssetEntryPoolEntry *newEntry, int32_t allowOverride);
static void __cdecl DB_FreeXAssetEntry(XAssetEntryPoolEntry *assetEntry);
static void __cdecl DB_FreeXAssetHeader(XAssetType type, XAssetHeader header);
static void __cdecl DB_CloneXAssetEntry(const XAssetEntry *from, XAssetEntry *to);
static void __cdecl DB_DynamicCloneXAsset(XAssetHeader from, XAssetHeader to, XAssetType type, int32_t fromDefault);
static void __cdecl DB_DelayedCloneXAsset(XAssetEntry *newEntry);
static bool __cdecl DB_OverrideAsset(uint32_t newZoneIndex, uint32_t existingZoneIndex);
static void __cdecl DB_GetXAsset(XAssetType type, XAssetHeader header);
static void DB_PostLoadXZone();
static void DB_Init();
static void __cdecl DB_InitPoolHeader(XAssetType type);
static void __cdecl DB_LoadXZone(XZoneInfo *zoneInfo, uint32_t zoneCount);
static void __cdecl DB_LoadZone_f();
static void __cdecl  DB_Thread(uint32_t threadContext);
static void DB_TryLoadXFile();
static int32_t __cdecl DB_TryLoadXFileInternal(char *zoneName, int32_t zoneFlags);
static void __cdecl DB_BuildOSPath(const char *zoneName, uint32_t size, char *filename)
{
#ifdef __SWITCH__
    Com_sprintf(filename, size, "zone/english/%s.ff", zoneName);
#else
    char *v3;
    char *Language;
    Language = Win_GetLanguage();
    v3 = Sys_DefaultInstallPath();
    Com_sprintf(filename, size, "%s\\zone\\%s\\%s.ff", v3, Language, zoneName);
#endif
}

int32_t __cdecl DB_GetZoneAllocType(int32_t zoneFlags)
{
    int32_t result; // eax

    switch (zoneFlags)
    {
    case DB_ZONE_COMMON_LOC:
    case DB_ZONE_COMMON:
    case DB_ZONE_MOD:
    case DB_ZONE_LOAD:
    case DB_ZONE_DEV:
        result = 1;
        break;
    default:
        result = 0;
        break;
    }
    return result;
}

void __cdecl DB_UnloadXZone(uint32_t zoneIndex, bool createDefault)
{
    uint32_t hash; // [esp+4h] [ebp-28h]
    uint16_t *pAssetEntryIndex; // [esp+8h] [ebp-24h]
    XAssetEntryPoolEntry *overrideAssetEntry; // [esp+Ch] [ebp-20h]
    XAsset asset; // [esp+14h] [ebp-18h] BYREF
    const char *name; // [esp+1Ch] [ebp-10h]
    XAssetEntry *assetEntry; // [esp+20h] [ebp-Ch]
    uint16_t *pOverrideAssetEntryIndex; // [esp+24h] [ebp-8h]
    uint32_t overrideAssetEntryIndex; // [esp+28h] [ebp-4h]

    iassert(zoneIndex);
    hash = 0;

    // KISAKTODO: would be nice
#if 0
    //Com_Printf(CON_CHANNEL_SYSTEM, "Unloading assets from fastfile '%s' ", g_zoneNames[zoneIndex]) // KISAKTODO: would be nice
    Com_Printf(CON_CHANNEL_SYSTEM, "Unloading assets from fastfile '%i' ", zoneIndex);
    
    if (createDefault)
    {
        Com_Printf(CON_CHANNEL_SYSTEM, "and creating default assets stubs\n");
    }
    else
    {
        Com_Printf(CON_CHANNEL_SYSTEM, "and deleting all assets\n");
    }
#endif

LABEL_4:
    if (hash < 0x8000)
    {
        pAssetEntryIndex = &db_hashTable[hash];
        while (1)
        {
            while (1)
            {
                if (!*pAssetEntryIndex)
                {
                    ++hash;
                    goto LABEL_4;
                }
                assetEntry = &g_assetEntryPool[*pAssetEntryIndex].entry;
                if (assetEntry->zoneIndex == zoneIndex)
                    break;
            LABEL_24:
                pOverrideAssetEntryIndex = &assetEntry->nextOverride;
                while (*pOverrideAssetEntryIndex)
                {
                    overrideAssetEntry = &g_assetEntryPool[*pOverrideAssetEntryIndex];
                    iassert(!overrideAssetEntry->entry.inuse);
                    if (overrideAssetEntry->entry.zoneIndex == zoneIndex)
                    {
                        DB_RemoveXAsset(&overrideAssetEntry->entry.asset);
                        *pOverrideAssetEntryIndex = overrideAssetEntry->entry.nextOverride;
                        DB_FreeXAssetEntry(overrideAssetEntry);
                    }
                    else
                    {
                        pOverrideAssetEntryIndex = &overrideAssetEntry->entry.nextOverride;
                    }
                }
                pAssetEntryIndex = &assetEntry->nextHash;
            }
            if (assetEntry->inuse && createDefault)
            {
                varXAsset = &assetEntry->asset;
                Mark_XAsset();
            }
            DB_RemoveXAsset(&assetEntry->asset);
            overrideAssetEntryIndex = assetEntry->nextOverride;
            if (overrideAssetEntryIndex)
            {
                overrideAssetEntry = &g_assetEntryPool[overrideAssetEntryIndex];
                DB_CloneXAssetEntry(&overrideAssetEntry->entry, assetEntry);
                assetEntry->nextOverride = overrideAssetEntry->entry.nextOverride;
                DB_FreeXAssetEntry(overrideAssetEntry);
                goto LABEL_24;
            }
            if (createDefault)
            {
                asset.type = assetEntry->asset.type;
                asset.header = DB_FindXAssetDefaultHeaderInternal(asset.type);
                if (asset.header.xmodelPieces)
                {
                    ++g_defaultAssetCount;
                    assetEntry->zoneIndex = 0;
                    name = DB_GetXAssetName(&assetEntry->asset);
                    DB_CloneXAssetInternal(&asset, &assetEntry->asset);
                    DB_SetXAssetName(&assetEntry->asset, name);
                    goto LABEL_24;
                }
                
                iassert(!assetEntry->nextOverride);
                *pAssetEntryIndex = assetEntry->nextHash;
                DB_FreeXAssetEntry((XAssetEntryPoolEntry *)assetEntry);
                if (*g_defaultAssetName[asset.type])
                {
                    Sys_UnlockWrite(&db_hashCritSect);
                    asset.header = DB_FindXAssetDefaultHeaderInternal(asset.type);
                    Sys_Error("Could not load default asset for asset type '%s'", g_assetNames[asset.type]);
                }
            }
            else
            {
                iassert(!assetEntry->nextOverride);
                *pAssetEntryIndex = assetEntry->nextHash;
                DB_FreeXAssetEntry((XAssetEntryPoolEntry *)assetEntry);
            }
        }
    }
}

void(__cdecl *DB_RemoveXAssetHandler[ASSET_TYPE_COUNT])(XAssetHeader) =
{
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  (void(*)(XAssetHeader)) & Material_ReleaseTechniqueSet,
  (void(*)(XAssetHeader)) & Image_Free,
  NULL,
  NULL,
  &DB_RemoveLoadedSound,
  &DB_RemoveClipMap,
  &DB_RemoveClipMap,
  &DB_RemoveComWorld,
  NULL,
  NULL,
  NULL,
  &DB_RemoveGfxWorld,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
}; // idb

void __cdecl DB_RemoveXAsset(XAsset *asset)
{
    if (DB_RemoveXAssetHandler[asset->type])
        DB_RemoveXAssetHandler[asset->type](asset->header);
}

void __cdecl DB_ReleaseXAssets()
{
    uint32_t hash; // [esp+0h] [ebp-Ch]
    uint32_t assetEntryIndex; // [esp+4h] [ebp-8h]

    if (!Sys_IsMainThread())
        MyAssertHandler(".\\database\\db_registry.cpp", 3998, 0, "%s", "Sys_IsMainThread()");
    Sys_SyncDatabase();
    for (hash = 0; hash < 0x8000; ++hash)
    {
        for (assetEntryIndex = db_hashTable[hash];
            assetEntryIndex;
            assetEntryIndex = g_assetEntryPool[assetEntryIndex].entry.nextHash)
        {
            g_assetEntryPool[assetEntryIndex].entry.inuse = 0;
        }
    }
}

void __cdecl DB_ShutdownXAssets()
{
    int32_t i; // [esp+0h] [ebp-4h]
    int32_t ia; // [esp+0h] [ebp-4h]

    DB_SyncXAssets();
    DB_SyncExternalAssets();
    iassert(!db_hashCritSect.writeCount);
    Sys_LockWrite(&db_hashCritSect);
    for (i = g_zoneCount - 1; i >= 0; --i)
        DB_UnloadXZone(g_zoneHandles[i], 0);
    DB_FreeDefaultEntries();
    DB_FreeUnusedResources();
    for (ia = g_zoneCount - 1; ia >= 0; --ia)
        DB_UnloadXZoneMemory(&g_zones[g_zoneHandles[ia]]);
    g_zoneCount = 0;
    Sys_UnlockWrite(&db_hashCritSect);
}

void __cdecl DB_FreeXZoneMemory(XZoneMemory *zoneMem)
{
    uint32_t blockIndex; // [esp+0h] [ebp-4h]

    DB_ReleaseGeometryBuffers(zoneMem);
    for (blockIndex = 0; blockIndex < 9; ++blockIndex)
    {
        zoneMem->blocks[blockIndex].data = 0;
        zoneMem->blocks[blockIndex].size = 0;
    }
}

void __cdecl DB_UnloadXZoneMemory(XZone *zone)
{
    DB_FreeXZoneMemory(&zone->mem);
    Com_Printf(CON_CHANNEL_SYSTEM, "Unloaded fastfile %s\n", zone->name);
    PMem_Free(zone->name, zone->allocType);
    zone->name[0] = 0;
}

void DB_FreeDefaultEntries()
{
    uint32_t nextAssetEntryIndex; // [esp+0h] [ebp-10h]
    uint32_t hash; // [esp+4h] [ebp-Ch]
    uint32_t assetEntryIndex; // [esp+8h] [ebp-8h]
    XAssetEntryPoolEntry *assetEntry; // [esp+Ch] [ebp-4h]

    for (hash = 0; hash < 0x8000; ++hash)
    {
        for (assetEntryIndex = db_hashTable[hash]; assetEntryIndex; assetEntryIndex = nextAssetEntryIndex)
        {
            assetEntry = &g_assetEntryPool[assetEntryIndex];
            nextAssetEntryIndex = assetEntry->entry.nextHash;
            if (assetEntry->entry.zoneIndex)
                MyAssertHandler(".\\database\\db_registry.cpp", 3950, 0, "%s", "!assetEntry->zoneIndex");
            if (assetEntry->entry.nextOverride)
                MyAssertHandler(".\\database\\db_registry.cpp", 3951, 0, "%s", "!assetEntry->nextOverride");
            if (!g_defaultAssetCount)
                MyAssertHandler(".\\database\\db_registry.cpp", 3952, 0, "%s", "g_defaultAssetCount");
            --g_defaultAssetCount;
            DB_FreeXAssetEntry(assetEntry);
        }
        db_hashTable[hash] = 0;
    }
    if (g_defaultAssetCount)
        MyAssertHandler(".\\database\\db_registry.cpp", 3959, 0, "%s", "!g_defaultAssetCount");
}

void __cdecl DB_UnloadXAssetsMemoryForZone(int32_t zoneFreeFlags, int32_t zoneFreeBit)
{
    int32_t sortedIndex; // [esp+0h] [ebp-8h]
    XZone *zone; // [esp+4h] [ebp-4h]

    if ((zoneFreeBit & zoneFreeFlags) != 0)
    {
        for (sortedIndex = g_zoneCount - 1; sortedIndex >= 0; --sortedIndex)
        {
            zone = &g_zones[g_zoneHandles[sortedIndex]];
            if ((zoneFreeBit & zone->flags) != 0)
                DB_UnloadXAssetsMemory(zone, sortedIndex);
        }
    }
}

void __cdecl DB_UnloadXAssetsMemory(XZone *zone, int32_t sortedIndex)
{
    DB_UnloadXZoneMemory(zone);
    --g_zoneCount;
    while (sortedIndex < g_zoneCount)
    {
        //g_zoneHandles[sortedIndex] = *(_BYTE *)(sortedIndex + 19939261);
        g_zoneHandles[sortedIndex] = g_zoneHandles[sortedIndex + 1];
        ++sortedIndex;
    }
}

void __cdecl DB_ReplaceModel(const char *original, const char *replacement)
{
    DB_ReplaceXAsset(ASSET_TYPE_XMODEL, original, replacement);
}

void __cdecl DB_ReplaceXAsset(XAssetType type, const char *original, const char *replacement)
{
    const char *originalName; // [esp+8h] [ebp-14h]
    XAsset replacementAsset; // [esp+Ch] [ebp-10h] BYREF
    XAsset originalAsset; // [esp+14h] [ebp-8h] BYREF

    originalAsset.type = type;
    originalAsset.header = DB_FindXAssetHeader(type, original);
    originalName = DB_GetXAssetName(&originalAsset);
    replacementAsset.type = type;
    replacementAsset.header = DB_FindXAssetHeader(type, replacement);
    DB_CloneXAsset(&replacementAsset, &originalAsset);
    DB_SetXAssetName(&originalAsset, originalName);
}

void __cdecl DB_CloneXAsset(const XAsset *from, XAsset *to)
{
    if (from->type != to->type)
        MyAssertHandler(".\\database\\db_registry.cpp", 2504, 0, "%s", "from->type == to->type");
    DB_DynamicCloneXAsset(to->header, from->header, to->type, 0);
    DB_CloneXAssetInternal(from, to);
}

void DB_SyncExternalAssets()
{
#ifndef DEDICATED
    R_SyncRenderThread();
    RB_UnbindAllImages();
    R_ShutdownStreams();
    RB_ClearPixelShader();
    RB_ClearVertexShader();
    RB_ClearVertexDecl();
#endif
}

void DB_ArchiveAssets()
{
    if (!g_archiveBuf)
    {
        g_archiveBuf = 1;
        R_SyncRenderThread();
        R_ClearAllStaticModelCacheRefs();
        DB_SaveSounds();
        DB_SaveDObjs();
    }
}

void DB_FreeUnusedResources()
{
    uint32_t hash; // [esp+0h] [ebp-18h]
    uint32_t hasha; // [esp+0h] [ebp-18h]
    uint16_t *pAssetEntryIndex; // [esp+4h] [ebp-14h]
    uint32_t assetEntryIndex; // [esp+8h] [ebp-10h]
    const char *newName; // [esp+Ch] [ebp-Ch]
    char *name; // [esp+10h] [ebp-8h]
    XAssetEntryPoolEntry *assetEntry; // [esp+14h] [ebp-4h]

    SL_TransferSystem(4u, 8u);
    for (hash = 0; hash < 0x8000; ++hash)
    {
        for (assetEntryIndex = db_hashTable[hash];
            assetEntryIndex;
            assetEntryIndex = g_assetEntryPool[assetEntryIndex].entry.nextHash)
        {
            if (g_assetEntryPool[assetEntryIndex].entry.zoneIndex)
            {
                varXAsset = &g_assetEntryPool[assetEntryIndex].entry.asset;
                Mark_XAsset();
            }
        }
    }
    for (hasha = 0; hasha < 0x8000; ++hasha)
    {
        //pAssetEntryIndex = (uint16_t *)(2 * hasha + 17442712);
        pAssetEntryIndex = &db_hashTable[hasha];
        while (*pAssetEntryIndex)
        {
            assetEntry = &g_assetEntryPool[*pAssetEntryIndex];
            if (assetEntry->entry.zoneIndex)
            {
                pAssetEntryIndex = &assetEntry->entry.nextHash;
            }
            else if (assetEntry->entry.inuse)
            {
                name = (char *)DB_GetXAssetName(&assetEntry->entry.asset);
                newName = SL_ConvertToString(SL_GetString(name, 4));
                DB_SetXAssetName(&assetEntry->entry.asset, newName);
                pAssetEntryIndex = &assetEntry->entry.nextHash;
            }
            else
            {
                if (assetEntry->entry.nextOverride)
                    MyAssertHandler(".\\database\\db_registry.cpp", 4200, 0, "%s", "!assetEntry->nextOverride");
                *pAssetEntryIndex = assetEntry->entry.nextHash;
                if (!g_defaultAssetCount)
                    MyAssertHandler(".\\database\\db_registry.cpp", 4202, 0, "%s", "g_defaultAssetCount");
                --g_defaultAssetCount;
                DB_FreeXAssetEntry(assetEntry);
            }
        }
    }
    SL_ShutdownSystem(8);
}

void DB_ExternalInitAssets()
{
    Material_DirtyTechniqueSetOverrides();
    BG_FillInAllWeaponItems();
}

void DB_UnarchiveAssets()
{
    iassert(g_archiveBuf);
    g_archiveBuf = 0;
    DB_LoadSounds();
    DB_LoadDObjs();
    DB_ExternalInitAssets();
    iassert(Sys_IsMainThread() || Sys_IsRenderThread());

    if (Sys_IsMainThread())
        R_ReleaseThreadOwnership();
}

void __cdecl DB_Cleanup()
{
    Sys_SyncDatabase();
    iassert(!g_archiveBuf);
}

int32_t __cdecl DB_FileSize(const char *zoneName, int32_t isMod)
{
    char filename[260]; // [esp+0h] [ebp-110h] BYREF
    int32_t size; // [esp+108h] [ebp-8h]
    void *zoneFile; // [esp+10Ch] [ebp-4h]

    if (isMod)
        DB_BuildOSPath_Mod(zoneName, 0x100u, filename);
    else
        DB_BuildOSPath(zoneName, 0x100u, filename);
    zoneFile = CreateFileA(filename, 0x80000000, 1u, 0, 3u, 0x60000000u, 0);
    if (zoneFile == (void *)-1)
        return 0;
    size = GetFileSize(zoneFile, 0);
    CloseHandle(zoneFile);
    return size;
}

void __cdecl Load_GetCurrentZoneHandle(uint8_t *handle)
{
    //uint8_t v1; // [esp+0h] [ebp-4h]

    iassert(g_loadingZone);
    //v1 = g_zoneIndex;
    //if (g_zoneIndex != g_zoneIndex)
    //    MyAssertHandler(
    //        "c:\\trees\\cod3\\src\\qcommon\\../universal/assertive.h",
    //        281,
    //        0,
    //        "i == static_cast< Type >( i )\n\t%i, %i",
    //        g_zoneIndex,
    //        g_zoneIndex);
    *handle = g_zoneIndex;
}

#ifdef __SWITCH__
void __cdecl DB_LoadXAssets(XZoneInfo *zoneInfo, uint32_t zoneCount, int32_t sync)
{
    uint32_t j; // [esp+4h] [ebp-14h]
    uint32_t ja; // [esp+4h] [ebp-14h]
    bool unloadedZone; // [esp+Bh] [ebp-Dh]
    uint32_t zoneIndex; // [esp+Ch] [ebp-Ch]
    int32_t i; // [esp+10h] [ebp-8h]
    int32_t zoneFreeFlags; // [esp+14h] [ebp-4h]

    iassert(Sys_IsMainThread());
    iassert(zoneCount);

    if (!g_zoneInited)
    {
        g_zoneInited = 1;
        DB_Init();
        Cmd_AddCommandInternal("loadzone", DB_LoadZone_f, &DB_LoadZone_f_VAR);
    }

    unloadedZone = 0;
    Material_ClearShaderUploadList();
#ifdef __SWITCH__
    if (g_zoneCount)
        DB_SyncXAssets();
#else
    DB_SyncXAssets();
#endif
    
    iassert(!g_archiveBuf);

    for (j = 0; j < zoneCount; ++j)
    {
        zoneFreeFlags = zoneInfo[j].freeFlags;
        for (i = g_zoneCount - 1; i >= 0; --i)
        {
            zoneIndex = g_zoneHandles[i];
            if ((zoneFreeFlags & g_zones[zoneIndex].flags) != 0)
            {
                if (!unloadedZone)
                {
                    unloadedZone = 1;
                    DB_SyncExternalAssets();
                    DB_ArchiveAssets();
                    Sys_LockWrite(&db_hashCritSect);
                }
                DB_UnloadXZone(zoneIndex, 1);
            }
        }
    }
    if (unloadedZone)
    {
        DB_FreeUnusedResources();
        for (ja = 0; ja < zoneCount; ++ja)
        {
            DB_UnloadXAssetsMemoryForZone(zoneInfo[ja].freeFlags, DB_ZONE_DEV);
            DB_UnloadXAssetsMemoryForZone(zoneInfo[ja].freeFlags, DB_ZONE_LOAD);
            DB_UnloadXAssetsMemoryForZone(zoneInfo[ja].freeFlags, DB_ZONE_MOD);
            DB_UnloadXAssetsMemoryForZone(zoneInfo[ja].freeFlags, DB_ZONE_GAME);
            DB_UnloadXAssetsMemoryForZone(zoneInfo[ja].freeFlags, DB_ZONE_COMMON);
            DB_UnloadXAssetsMemoryForZone(zoneInfo[ja].freeFlags, DB_ZONE_COMMON_LOC);
        }
        Sys_UnlockWrite(&db_hashCritSect);
        DB_UnarchiveAssets();
    }
    if (sync)
        DB_ArchiveAssets();
    g_sync = sync;
    DB_LoadXZone(zoneInfo, zoneCount);
    if (sync)
    {
        iassert(!g_copyInfoCount);
        Sys_SyncDatabase();
        DB_UnarchiveAssets();
    }
}

void DB_Init()
{
    for (XAssetType type = (XAssetType)0; type < ASSET_TYPE_COUNT; ++type)
        DB_InitPoolHeader(type);

    g_freeAssetEntryHead = g_assetEntryPool + 16;

    for (int32_t i = 1; i < 0x7FFF; ++i)
        g_assetEntryPool[i].next = &g_assetEntryPool[i + 1];

    g_assetEntryPool[0x7FFF].next = NULL;
}


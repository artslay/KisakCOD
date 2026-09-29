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
#include <win32/win_local.h>
#ifdef __SWITCH__
extern void __cdecl NET_Sleep(int msec);
#endif
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
#include <gfx_d3d/r_dvars.h>
#include <gfx_d3d/r_staticmodelcache.h>
#ifndef __SWITCH__
#include <win32/win_localize.h>
#endif
#include <universal/profile.h>

#include <algorithm>
#ifdef __SWITCH__
#include <thread>
#endif

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

struct XZoneInfoInternal
{
    char name[64];
    int32_t flags;
};

char g_zoneNameList[2080];
XAssetPool<XModelPieces, POOLSIZE_XMODELPIECES> g_XModelPiecesPool;
XAssetPool<PhysPreset, POOLSIZE_PHYSPRESET> g_PhysPresetPool;
XAssetPool<XAnimParts, POOLSIZE_XANIMPARTS> g_XAnimPartsPool;
XAssetPool<XModel, POOLSIZE_XMODEL> g_XModelPool;
XAssetPool<Material, POOLSIZE_MATERIAL> g_MaterialPool;
XAssetPool<MaterialTechniqueSet, POOLSIZE_TECHNIQUE_SET> g_MaterialTechniqueSetPool;
XAssetPool<GfxImage, POOLSIZE_IMAGE> g_GfxImagePool;
XAssetPool<snd_alias_list_t, POOLSIZE_SOUND> g_SoundPool;
XAssetPool<SndCurve, POOLSIZE_SOUND_CURVE> g_SndCurvePool;
XAssetPool<LoadedSound, POOLSIZE_LOADED_SOUND> g_LoadedSoundPool;
XAssetPool<MapEnts, POOLSIZE_MAP_ENTS> g_MapEntsPool;
XAssetPool<GfxLightDef, POOLSIZE_LIGHT_DEF> g_GfxLightDefPool;
XAssetPool<Font_s, POOLSIZE_FONT> g_FontPool;
XAssetPool<MenuList, POOLSIZE_MENULIST> g_MenuListPool;
XAssetPool<menuDef_t, POOLSIZE_MENU> g_MenuPool;
XAssetPool<LocalizeEntry, POOLSIZE_LOCALIZE_ENTRY> g_LocalizeEntryPool;
XAssetPool<WeaponDef, POOLSIZE_WEAPON> g_WeaponDefPool;
XAssetPool<FxEffectDef, POOLSIZE_FX> g_FxEffectDefPool;
XAssetPool<FxImpactTable, POOLSIZE_IMPACT_FX> g_FxImpactTablePool;
XAssetPool<RawFile, POOLSIZE_RAWFILE> g_RawFilePool;
XAssetPool<StringTable, POOLSIZE_STRINGTABLE> g_StringTablePool;

fileData_s *com_fileDataHashTable[1024];

template <typename T>
static void __cdecl DB_InitPool(void *arg, int32_t size)
{
    T *pool = static_cast<T *>(arg);
    if (size <= 0)
        return;
    pool->freeHead = &pool->entries[0];
    for (int32_t i = 0; i < size - 1; ++i)
        pool->entries[i].next = &pool->entries[i + 1];
    pool->entries[size - 1].next = NULL;
}

static void __cdecl DB_InitSingleton(void *pool, int32_t size);

void(__cdecl *DB_InitPoolHeaderHandler[ASSET_TYPE_COUNT])(void *, int) =
{
  DB_InitPool<XAssetPool<XModelPieces, POOLSIZE_XMODELPIECES>>,
  DB_InitPool<XAssetPool<PhysPreset, POOLSIZE_PHYSPRESET>>,
  DB_InitPool<XAssetPool<XAnimParts, POOLSIZE_XANIMPARTS>>,
  DB_InitPool<XAssetPool<XModel, POOLSIZE_XMODEL>>,
  DB_InitPool<XAssetPool<Material, POOLSIZE_MATERIAL>>,
  DB_InitPool<XAssetPool<MaterialTechniqueSet, POOLSIZE_TECHNIQUE_SET>>,
  DB_InitPool<XAssetPool<GfxImage, POOLSIZE_IMAGE>>,
  DB_InitPool<XAssetPool<snd_alias_list_t, POOLSIZE_SOUND>>,
  DB_InitPool<XAssetPool<SndCurve, POOLSIZE_SOUND_CURVE>>,
  DB_InitPool<XAssetPool<LoadedSound, POOLSIZE_LOADED_SOUND>>,
  &DB_InitSingleton,
  &DB_InitSingleton,
  &DB_InitSingleton,
  &DB_InitSingleton,
  &DB_InitSingleton,
  DB_InitPool<XAssetPool<MapEnts, POOLSIZE_MAP_ENTS>>,
  &DB_InitSingleton,
  DB_InitPool<XAssetPool<GfxLightDef, POOLSIZE_LIGHT_DEF>>,
  NULL,
  DB_InitPool<XAssetPool<Font_s, POOLSIZE_FONT>>,
  DB_InitPool<XAssetPool<MenuList, POOLSIZE_MENULIST>>,
  DB_InitPool<XAssetPool<menuDef_t, POOLSIZE_MENU>>,
  DB_InitPool<XAssetPool<LocalizeEntry, POOLSIZE_LOCALIZE_ENTRY>>,
  DB_InitPool<XAssetPool<WeaponDef, POOLSIZE_WEAPON>>,
  NULL,
  DB_InitPool<XAssetPool<FxEffectDef, POOLSIZE_FX>>,
  DB_InitPool<XAssetPool<FxImpactTable, POOLSIZE_IMPACT_FX>>,
  NULL,
  NULL,
  NULL,
  NULL,
  DB_InitPool<XAssetPool<RawFile, POOLSIZE_RAWFILE>>,
  DB_InitPool<XAssetPool<StringTable, POOLSIZE_STRINGTABLE>>,
};

void *DB_XAssetPool[ASSET_TYPE_COUNT] =
{
  &g_XModelPiecesPool,
  &g_PhysPresetPool,
  &g_XAnimPartsPool,
  &g_XModelPool,
  &g_MaterialPool,
  &g_MaterialTechniqueSetPool,
  &g_GfxImagePool,
  &g_SoundPool,
  &g_SndCurvePool,
  &g_LoadedSoundPool,
  &cm,
  &cm,
  &comWorld,
#ifdef KISAK_MP
  NULL,
  &gameWorldMp,
#elif KISAK_SP
  &gameWorldSp,
  NULL,
#else
  NULL,
  NULL,
#endif
  &g_MapEntsPool,
  &s_world,
  &g_GfxLightDefPool,
  NULL,
  &g_FontPool,
  &g_MenuListPool,
  &g_MenuPool,
  &g_LocalizeEntryPool,
  &g_WeaponDefPool,
  NULL,
  &g_FxEffectDefPool,
  &g_FxImpactTablePool,
  NULL,
  NULL,
  NULL,
  NULL,
  &g_RawFilePool,
  &g_StringTablePool
};

uint32_t volatile g_mainThreadBlocked;
XAssetEntryPoolEntry *g_freeAssetEntryHead;
uint16_t db_hashTable[32768];
XAssetEntry *g_copyInfo[0x800];
uint32_t g_copyInfoCount;
XZone g_zones[ASSET_TYPE_COUNT]{ 0 };

void __cdecl DB_GetIndexBufferAndBase(uint8_t zoneHandle, void *indices, void **ib, int32_t *baseIndex)
{
    *ib = g_zones[zoneHandle].mem.indexBuffer;
    *baseIndex = ((uintptr_t)indices - (uintptr_t)g_zones[zoneHandle].mem.blocks[8].data) >> 1;
}

void __cdecl DB_GetVertexBufferAndOffset(uint8_t zoneHandle, uint8_t *verts, void **vb, int32_t *vertexOffset)
{
    *vertexOffset = (int32_t)(verts - g_zones[zoneHandle].mem.blocks[7].data);
    *vb = g_zones[zoneHandle].mem.vertexBuffer;
}

uint8_t g_zoneHandles[32];
XAssetEntryPoolEntry g_assetEntryPool[32768];
uint8_t g_fileBuf[524288];
FastCriticalSection db_hashCritSect;

bool g_zoneInited;
int32_t g_zoneCount;
bool g_isRecoveringLostDevice;
bool g_mayRecoverLostAssets;
volatile bool g_loadingZone;
volatile uint32_t g_zoneInfoCount;
bool g_initializing;
char g_debugZoneName[64];
uint32_t g_zoneAllocType;
uint32_t g_zoneIndex;
uint32_t _S1;
const dvar_t *zone_reorder;
volatile uint32_t g_loadingAssets;
XZoneInfoInternal g_zoneInfo[8];

int32_t g_defaultAssetCount;
const char *g_defaultAssetName[ASSET_TYPE_COUNT] =
{
    "",
    "default",
    "void",
    "void",
    "$default",
    "default",
    "$white",
    "null",
    "default",
    "null.wav",
    "",
    "",
    "",
    "",
    "",
    "",
    "light_dynamic",
    "",
    "fonts/consolefont",
    "ui/default.menu",
    "default_menu",
    "CGAME_UNKNOWN",
#ifdef KISAK_MP
    "defaultweapon_mp",
#elif KISAK_SP
    "defaultweapon",
#else
    "defaultweapon",
#endif
    "",
    "misc/missing_fx",
    "default",
    "",
    "",
    "",
    "",
    "",
    "mp/defaultStringTable.csv"
};

int32_t g_sync;
cmd_function_s DB_LoadZone_f_VAR;

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
XAssetHeader __cdecl DB_AddXAsset(XAssetType type, XAssetHeader header);
XAssetEntryPoolEntry *__cdecl DB_LinkXAssetEntry(XAssetEntryPoolEntry *newEntry, int32_t allowOverride);
static void __cdecl DB_FreeXAssetEntry(XAssetEntryPoolEntry *assetEntry);
static void __cdecl DB_FreeXAssetHeader(XAssetType type, XAssetHeader header);
static void __cdecl DB_CloneXAssetEntry(const XAssetEntry *from, XAssetEntry *to);
static void(__cdecl *DB_DynamicCloneXAssetHandler[ASSET_TYPE_COUNT])(XAssetHeader, XAssetHeader, int) =
{
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
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    DB_DynamicCloneMenu,
    NULL,
    (void(*)(XAssetHeader, XAssetHeader, int))KISAK_NULLSUB,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

void __cdecl DB_DynamicCloneXAsset(XAssetHeader from, XAssetHeader to, XAssetType type, int32_t fromDefault);
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
static int32_t __cdecl DB_GetAllXAssetOfType_LoadObj(XAssetType type, XAssetHeader *assets, int32_t maxCount);
static void __cdecl DB_EnumXAssets_LoadObj(XAssetType type, void(*func)(void*, void*), void *inData);
static void __cdecl DB_RemoveLoadedSound(XAssetHeader header);
void __cdecl DB_RemoveXAsset(XAsset *asset);
void __cdecl DB_SyncExternalAssets();
void DB_FreeDefaultEntries();
void DB_FreeUnusedResources();
void __cdecl DB_UnloadXZoneMemory(XZone *zone);
void __cdecl DB_UnloadXAssetsMemory(XZone *zone, int32_t sortedIndex);
void __cdecl DB_ReplaceXAsset(XAssetType type, const char *original, const char *replacement);
void __cdecl DB_CloneXAsset(const XAsset *from, XAsset *to);
void __cdecl Material_DirtyTechniqueSetOverrides();
void __cdecl Material_ClearShaderUploadList();
static void __cdecl DB_BuildOSPath(const char *zoneName, uint32_t size, char *filename);

static void __cdecl DB_RemoveLoadedSound(XAssetHeader header)
{
    if (header.loadSnd && header.loadSnd->sound.data)
        Z_Free(header.loadSnd->sound.data, 15);
}

static void __cdecl DB_BuildOSPath_Mod(const char *zoneName, uint32_t size, char *filename)
{
#ifdef __SWITCH__
    if (fs_gameDirVar && fs_gameDirVar->current.string[0])
        Com_sprintf(filename, size, "%s/%s.ff", fs_gameDirVar->current.string, zoneName);
    else
        DB_BuildOSPath(zoneName, size, filename);
#else
    char *v3;
    const char *string = fs_gameDirVar->current.string;
    v3 = Sys_DefaultInstallPath();
    Com_sprintf(filename, size, "%s\\%s\\%s.ff", v3, string, zoneName);
#endif
}

static void __cdecl DB_BuildOSPath(const char *zoneName, uint32_t size, char *filename)
{
#ifdef __SWITCH__
    const int languageIndex = SEH_GetCurrentLanguage();
    const char *languageName = SEH_GetLanguageName(languageIndex);
    if (!languageName || !*languageName)
        languageName = "english";
    Com_sprintf(filename, size, "zone/%s/%s.ff", languageName, zoneName);
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

XAssetHeader __cdecl DB_FindXAssetHeader(XAssetType type, const char *name)
{
    XAssetEntryPoolEntry *assetEntry = DB_FindXAssetEntry(type, name);
    if (assetEntry)
    {
        assetEntry->entry.inuse = 1;
        return assetEntry->entry.asset.header;
    }

    if (type == ASSET_TYPE_LOCALIZE_ENTRY || type == ASSET_TYPE_RAWFILE)
        return {};

    XAssetEntry *newEntry = DB_CreateDefaultEntry(type, (char *)name);
    return newEntry ? newEntry->asset.header : XAssetHeader{};
}

bool __cdecl DB_IsXAssetDefault(XAssetType type, const char *name)
{
    const uint32_t hash = DB_HashForName(name, type);
    InterlockedIncrement(&db_hashCritSect.readCount);
    while (db_hashCritSect.writeCount)
        std::this_thread::yield();

    for (uint32_t assetEntryIndex = db_hashTable[hash];
         assetEntryIndex;
         assetEntryIndex = g_assetEntryPool[assetEntryIndex].entry.nextHash)
    {
        XAssetEntryPoolEntry *assetEntry = &g_assetEntryPool[assetEntryIndex];
        if (assetEntry->entry.asset.type == type)
        {
            const char *assetName = DB_GetXAssetName(&assetEntry->entry.asset);
            if (assetName && !I_stricmp(assetName, name))
            {
                InterlockedDecrement(&db_hashCritSect.readCount);
                return assetEntry->entry.zoneIndex == 0;
            }
        }
    }

    InterlockedDecrement(&db_hashCritSect.readCount);
    return true;
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
    char filename[260];
#ifdef __SWITCH__
    if (isMod)
        DB_BuildOSPath_Mod(zoneName, sizeof(filename), filename);
    else
        DB_BuildOSPath(zoneName, sizeof(filename), filename);

    FILE *zoneFile = FS_SwitchOpenRootFile(filename);
    if (!zoneFile)
        return 0;

    long saved = std::ftell(zoneFile);
    std::fseek(zoneFile, 0, SEEK_END);
    long size = std::ftell(zoneFile);
    std::fseek(zoneFile, saved, SEEK_SET);
    std::fclose(zoneFile);
    return size > 0 ? static_cast<int32_t>(size) : 0;
#else
    int32_t size;
    void *zoneFile;
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
#endif
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


#ifdef __SWITCH__
void __cdecl DB_LoadXZone(XZoneInfo *zoneInfo, uint32_t zoneCount)
{
    uint32_t j; // [esp+0h] [ebp-Ch]
    char *zoneName; // [esp+4h] [ebp-8h]
    uint32_t zoneInfoCount; // [esp+8h] [ebp-4h]

    if (g_zoneCount == 32)
        Com_Error(ERR_DROP, "Max zone count exceeded");
    if (g_zoneInfoCount)
        MyAssertHandler(".\\database\\db_registry.cpp", 3240, 0, "%s", "!g_zoneInfoCount");
    if (g_loadingAssets)
        MyAssertHandler(".\\database\\db_registry.cpp", 3241, 0, "%s", "!g_loadingAssets");
    zoneInfoCount = 0;
    for (j = 0; j < zoneCount; ++j)
    {
        zoneName = (char *)zoneInfo[j].name;
        if (zoneName)
        {
            if (zoneInfoCount >= 8)
                MyAssertHandler(".\\database\\db_registry.cpp", 3249, 0, "%s", "zoneInfoCount < ARRAY_COUNT( g_zoneInfo )");
            I_strncpyz(g_zoneInfo[zoneInfoCount].name, zoneName, 64);
            Com_Printf(CON_CHANNEL_SYSTEM, "Loading fastfile %s\n", g_zoneInfo[zoneInfoCount].name);
            g_zoneInfo[zoneInfoCount++].flags = zoneInfo[j].allocFlags;
        }
    }
    if (zoneInfoCount)
    {
        g_loadingAssets = zoneInfoCount;
        Sys_WakeDatabase2();
        Sys_WakeDatabase();
        g_zoneInfoCount = zoneInfoCount;
        Sys_NotifyDatabase();
    }
}

void __cdecl DB_InitThread()
{
    if (!Sys_SpawnDatabaseThread((void(__cdecl *)(uint32_t))DB_Thread))
        Sys_Error("Failed to create database thread");
}

void __cdecl  DB_Thread(uint32_t threadContext)
{
    jmp_buf *Value; // eax

    iassert(threadContext == THREAD_CONTEXT_DATABASE);
    Value = (jmp_buf *)Sys_GetValue(2);
    
    if (setjmp(*Value))
    {
        Profile_Recover(1);
#ifdef __llvm__ 
        __builtin_debugtrap();
#else
#ifdef __SWITCH__
        __builtin_trap();
#else
        __debugbreak();
#endif
#endif
        Com_ErrorAbort();
    }
    Profile_Guard(1);
    while (1)
    {
        Sys_WaitStartDatabase();
        DB_TryLoadXFile();
    }
}

void DB_TryLoadXFile()
{
    uint32_t j; // [esp+0h] [ebp-8h]
    uint32_t zoneInfoCount; // [esp+4h] [ebp-4h]

    if (g_zoneInfoCount)
    {
        zoneInfoCount = g_zoneInfoCount;
        g_zoneInfoCount = 0;
        if (g_loadingZone)
            MyAssertHandler(".\\database\\db_registry.cpp", 3764, 0, "%s", "!g_loadingZone");
        for (j = 0; j < zoneInfoCount; ++j)
        {
            if (!DB_TryLoadXFileInternal(g_zoneInfo[j].name, g_zoneInfo[j].flags))
                --g_loadingAssets;
        }
        if (g_loadingZone)
            MyAssertHandler(".\\database\\db_registry.cpp", 3772, 0, "%s", "!g_loadingZone");
        if (g_loadingAssets)
            MyAssertHandler(".\\database\\db_registry.cpp", 3773, 0, "%s", "!g_loadingAssets");
#ifdef __SWITCH__
        Sys_DatabaseCompleted();
#else
        Sys_LockWrite(&s_dbReorder.critSect);
        DB_EndReorderZone();
        Sys_UnlockWrite(&s_dbReorder.critSect);
        Sys_DatabaseCompleted();
#endif
    }
    else if (g_loadingAssets)
    {
        MyAssertHandler(".\\database\\db_registry.cpp", 3759, 0, "%s", "!g_loadingAssets");
    }
}

int32_t __cdecl DB_TryLoadXFileInternal(char *zoneName, int32_t zoneFlags)
{
    char filename[256];
    XZone *zone;
    uint32_t i;
    FILE *zoneFile;

    Com_Printf(CON_CHANNEL_DONT_FILTER, "Trying to load file %s with flags %x\n", zoneName, zoneFlags);
    iassert(!g_zoneInfoCount);

    DB_BuildOSPath(zoneName, sizeof(filename), filename);
    zoneFile = FS_SwitchOpenRootFile(filename);
    if (!zoneFile)
    {
        Com_PrintWarning(CON_CHANNEL_FILES, "WARNING: Could not find zone '%s'\n", filename);
        return 0;
    }

    g_zoneIndex = 0;
    for (i = 1; i < 0x21; ++i)
    {
        if (!g_zones[i].name[0])
        {
            g_zoneIndex = i;
            break;
        }
    }
    if (!g_zoneIndex)
    {
        fclose(zoneFile);
        Com_Error(ERR_DROP, "ERROR: Max zone count exceeded");
        return 0;
    }
    if (!*zoneName)
    {
        fclose(zoneFile);
        Com_Error(ERR_DROP, "ERROR: Empty fastfile name");
        return 0;
    }

    zone = &g_zones[g_zoneIndex];
    memset(zone, 0, sizeof(XZone));
    g_zoneHandles[g_zoneCount] = g_zoneIndex;
    I_strncpyz(zone->name, zoneName, sizeof(zone->name));
    zone->flags = zoneFlags;
    long saved = ftell(zoneFile);
    fseek(zoneFile, 0, SEEK_END);
    zone->fileSize = static_cast<uint32_t>(ftell(zoneFile));
    fseek(zoneFile, saved, SEEK_SET);
    zone->modZone = false;

    ++g_zoneCount;
    g_loadingZone = 1;
    g_mayRecoverLostAssets = 0;
    g_zoneAllocType = DB_GetZoneAllocType(zoneFlags);

    PMem_BeginAlloc(zone->name, g_zoneAllocType);
    zone->allocType = g_zoneAllocType;
    DB_ResetZoneSize((zoneFlags & DB_ZONE_GAME) != 0);
    DB_LoadXFile(filename, zoneFile, zone->name, &zone->mem, 0, g_fileBuf, g_zoneAllocType);
    DB_LoadXFileInternal();
    PMem_EndAlloc(zone->name, g_zoneAllocType);

    fclose(zoneFile);
    g_loadingZone = 0;
    g_mayRecoverLostAssets = 1;
    Com_Printf(CON_CHANNEL_SYSTEM, "Loaded fastfile %s (%u bytes)\n", zone->name, zone->fileSize);
    return 1;
}

#endif

#endif

// Restored upstream asset type names required by DB registry.
const char *g_assetNames[ASSET_TYPE_COUNT] = // SP/MP same
{
  "xmodelpieces",
  "physpreset",
  "xanim",
  "xmodel",
  "material",
  "techset",
  "image",
  "sound",
  "sndcurve",
  "loaded_sound",
  "col_map_sp",
  "col_map_mp",
  "com_map",
  "game_map_sp",
  "game_map_mp",
  "map_ents",
  "gfx_map",
  "lightdef",
  "ui_map",
  "font",
  "menufile",
  "menu",
  "localize",
  "weapon",
  "snddriverglobals",
  "fx",
  "impactfx",
  "aitype",
  "mptype",
  "character",
  "xmodelalias",
  "rawfile",
  "stringtable"
};

// Restored upstream DB registry implementations required by the Switch build.
void __cdecl DB_RemoveClipMap(XAssetHeader ass)
{
    CM_Unload();
}



void __cdecl DB_RemoveComWorld(XAssetHeader ass)
{
    Com_UnloadWorld();
}



void __cdecl DB_RemoveGfxWorld(XAssetHeader ass)
{
    R_UnloadWorld();
}



XAssetEntryPoolEntry *__cdecl DB_FindXAssetEntry(XAssetType type, const char *name)
{
    const char *XAssetName; // eax
    uint32_t assetEntryIndex; // [esp+4h] [ebp-8h]
    XAssetEntryPoolEntry *assetEntry; // [esp+8h] [ebp-4h]

    for (assetEntryIndex = db_hashTable[DB_HashForName(name, type)];
        assetEntryIndex;
        assetEntryIndex = assetEntry->entry.nextHash)
    {
        assetEntry = &g_assetEntryPool[assetEntryIndex];
        if (assetEntry->entry.asset.type == type)
        {
            XAssetName = DB_GetXAssetName(&assetEntry->entry.asset);
            if (!I_stricmp(XAssetName, name))
                return &g_assetEntryPool[assetEntryIndex];
        }
    }
    return 0;
}



uint32_t __cdecl DB_HashForName(const char *name, XAssetType type)
{
    int32_t c; // [esp+8h] [ebp-4h]
    int32_t out_val = (int)type;

    while (1)
    {
        c = tolower(*name);
        if (c == '\\')
            c = '/';
        if (!c)
            break;
        out_val = c + 31 * out_val;
        ++name;
    }
    return out_val % 0x8000u;
}



XAssetEntry *__cdecl DB_CreateDefaultEntry(XAssetType type, char *name)
{
    XAsset asset; // [esp+Ch] [ebp-Ch] BYREF
    XAssetEntry *newEntry; // [esp+14h] [ebp-4h]

    asset.header = DB_FindXAssetDefaultHeaderInternal(type);
    if (!asset.header.data)
    {
        Sys_UnlockWrite(&db_hashCritSect);
        if (type == ASSET_TYPE_CLIPMAP || type == ASSET_TYPE_CLIPMAP_PVS)
            Com_Error(
                ERR_DROP,
                "Couldn't find the bsp for this map.  Please build the fast file associated with %s and try again.",
                name);
        else
            Com_Error(
                ERR_DROP,
                "Could not load default asset '%s' for asset type '%s'.\nTried to load asset '%s'.",
                g_defaultAssetName[type],
                g_assetNames[type],
                name);
    }
    asset.type = type;
    ++g_defaultAssetCount;
    newEntry = (XAssetEntry *)DB_AllocXAssetEntry(type, 0);
    DB_CloneXAssetInternal(&asset, &newEntry->asset);
    if (type == ASSET_TYPE_SOUND)
    {
        newEntry->asset.header.sound->count = 0;
        newEntry->asset.header.sound->head = NULL;
    }
    newEntry->nextHash = db_hashTable[DB_HashForName(name, type)];
    db_hashTable[DB_HashForName(name, type)] = ((char *)newEntry - (char *)g_assetEntryPool) >> 4;
    DB_SetXAssetName(&newEntry->asset, SL_ConvertToString(SL_GetString(name, 4)));
    newEntry->inuse = 1;
    return newEntry;
}



void __cdecl DB_CloneXAssetInternal(const XAsset *from, XAsset *to)
{
    uint32_t size; // [esp+0h] [ebp-4h]

    iassert(from->type == to->type);
    size = DB_GetXAssetTypeSize(from->type);
    iassert(size <= sizeof(XAssetSize));
    memcpy(to->header.data, from->header.data, size);
}



XAssetHeader __cdecl DB_FindXAssetDefaultHeaderInternal(XAssetType type)
{
    const char *XAssetName; // eax
    uint32_t assetEntryIndex; // [esp+8h] [ebp-Ch]
    const char *name; // [esp+Ch] [ebp-8h]
    XAssetEntryPoolEntry *assetEntry; // [esp+10h] [ebp-4h]

    name = g_defaultAssetName[type];
    for (assetEntryIndex = db_hashTable[DB_HashForName(name, type)]; ; assetEntryIndex = assetEntry->entry.nextHash)
    {
        if (!assetEntryIndex)
            return 0;
        assetEntry = &g_assetEntryPool[assetEntryIndex];
        if (assetEntry->entry.asset.type == type)
        {
            XAssetName = DB_GetXAssetName(&assetEntry->entry.asset);
            if (!I_stricmp(XAssetName, name))
                break;
        }
    }
    while (assetEntry->entry.nextOverride)
        assetEntry = &g_assetEntryPool[assetEntry->entry.nextOverride];
    return assetEntry->entry.asset.header;
}



void __cdecl DB_FreeXAssetEntry(XAssetEntryPoolEntry *assetEntry)
{
    XAssetEntryPoolEntry *oldFreeHead; // [esp+4h] [ebp-4h]

    DB_FreeXAssetHeader(assetEntry->entry.asset.type, assetEntry->entry.asset.header);
    oldFreeHead = g_freeAssetEntryHead;
    g_freeAssetEntryHead = assetEntry;
    assetEntry->next = oldFreeHead;
}



void __cdecl DB_CloneXAssetEntry(const XAssetEntry *from, XAssetEntry *to)
{
    iassert(from->asset.type == to->asset.type);
    DB_DynamicCloneXAsset(to->asset.header, from->asset.header, to->asset.type, to->zoneIndex == 0);
    DB_CloneXAssetInternal(&from->asset, &to->asset);
    to->zoneIndex = from->zoneIndex;
}



void __cdecl DB_DynamicCloneXAsset(XAssetHeader from, XAssetHeader to, XAssetType type, int32_t fromDefault)
{
    if (DB_DynamicCloneXAssetHandler[type])
        DB_DynamicCloneXAssetHandler[type](from, to, fromDefault);
}



void __cdecl DB_LoadZone_f()
{
    char *v0; // eax

    v0 = (char *)Cmd_Argv(1);
    I_strncpyz(g_debugZoneName, v0, 64);
    DB_UpdateDebugZone();
}



int32_t __cdecl DB_GetAllXAssetOfType(XAssetType type, XAssetHeader* assets, int32_t maxCount)
{
    if (IsFastFileLoad())
        return DB_GetAllXAssetOfType_FastFile(type, assets, maxCount);
    else
        return DB_GetAllXAssetOfType_LoadObj(type, assets, maxCount);
}



int32_t __cdecl DB_GetAllXAssetOfType_FastFile(XAssetType type, XAssetHeader *assets, int32_t maxCount)
{
    uint32_t hash; // [esp+4h] [ebp-10h]
    uint32_t assetEntryIndex; // [esp+8h] [ebp-Ch]
    int32_t assetCount; // [esp+Ch] [ebp-8h]
    XAssetEntryPoolEntry *assetEntry; // [esp+10h] [ebp-4h]

    assetCount = 0;
    InterlockedIncrement(&db_hashCritSect.readCount);
    while (db_hashCritSect.writeCount)
        NET_Sleep(0);
    for (hash = 0; hash < 0x8000; ++hash)
    {
        for (assetEntryIndex = db_hashTable[hash]; assetEntryIndex; assetEntryIndex = assetEntry->entry.nextHash)
        {
            assetEntry = &g_assetEntryPool[assetEntryIndex];
            if (assetEntry->entry.asset.type == type)
            {
                if (assets)
                {
                    if (assetCount >= maxCount)
                        MyAssertHandler(".\\database\\db_registry.cpp", 2877, 0, "%s", "assetCount < maxCount");
                    assets[assetCount] = assetEntry->entry.asset.header;
                }
                ++assetCount;
            }
        }
    }
    if (db_hashCritSect.readCount <= 0)
        MyAssertHandler(
            "c:\\trees\\cod3\\src\\gfx_d3d\\../qcommon/threads_interlock.h",
            76,
            0,
            "%s",
            "critSect->readCount > 0");
    InterlockedDecrement(&db_hashCritSect.readCount);
    return assetCount;
}



void __cdecl DB_EnumXAssets(
    XAssetType type,
    void(__cdecl* func)(XAssetHeader, void*),
    void* inData,
    bool includeOverride)
{
    if (IsFastFileLoad())
        DB_EnumXAssets_FastFile(type, func, inData, includeOverride);
    else
        DB_EnumXAssets_LoadObj(type, (void(*)(void *, void *))func, inData);
}



void __cdecl DB_SyncXAssets()
{
    if (!Sys_IsMainThread())
        MyAssertHandler(".\\database\\db_registry.cpp", 3386, 0, "%s", "Sys_IsMainThread()");
    R_BeginRemoteScreenUpdate();
    Sys_SyncDatabase();
    R_EndRemoteScreenUpdate();
    DB_PostLoadXZone();
}



void __cdecl DB_InitPoolHeader(XAssetType type)
{
    if (DB_XAssetPool[type])
        DB_InitPoolHeaderHandler[type](DB_XAssetPool[type], g_poolSize[type]);
}




// Restored DB registry allocation, enumeration and post-load helpers.
static XAssetHeader __cdecl node1_(void *pool)
{
    return (XAssetHeader)pool;
}

static void __cdecl DB_FreeXAssetHeader_StringTable_(void *arg, XAssetHeader header);
void __cdecl R_EnumMaterials(void(__cdecl *func)(Material *, void *), void *data);
void __cdecl R_EnumTechniqueSets(void(__cdecl *func)(MaterialTechniqueSet *, void *), void *data);
void __cdecl R_EnumImages(void(__cdecl *func)(GfxImage *, void *), void *data);
void __cdecl Material_OverrideTechniqueSets();

static XAssetHeader __cdecl DB_AllocXAsset_StringTable_(void *arg)
{
    XAssetHeader *pool = (XAssetHeader*)arg;
    XAssetHeader header;

    if (pool->xmodelPieces)
    {
        header.xmodelPieces = pool->xmodelPieces;
        pool->xmodelPieces = (XModelPieces *)pool->xmodelPieces->name;
    }
    else
    {
        header.xmodelPieces = 0;
    }
    return header;
}

static XAssetHeader __cdecl DB_AllocMaterial(void *arg)
{
    XAssetHeader *pool = (XAssetHeader*)arg;
    Material_DirtySort();
    return DB_AllocXAsset_StringTable_(pool);
}

static void __cdecl DB_FreeMaterial(void *arg, XAssetHeader header)
{
    XAssetPoolEntry<StringTable> **pool = (XAssetPoolEntry<StringTable> **)arg;
    Material_DirtySort();
    DB_FreeXAssetHeader_StringTable_(pool, header);
}

XAssetHeader(__cdecl *DB_AllocXAssetHeaderHandler[ASSET_TYPE_COUNT])(void *) =
{
  &DB_AllocXAsset_StringTable_,
  &DB_AllocXAsset_StringTable_,
  &DB_AllocXAsset_StringTable_,
  &DB_AllocXAsset_StringTable_,
  &DB_AllocMaterial,
  &DB_AllocXAsset_StringTable_,
  &DB_AllocXAsset_StringTable_,
  &DB_AllocXAsset_StringTable_,
  &DB_AllocXAsset_StringTable_,
  &DB_AllocXAsset_StringTable_,
  &node1_,
  &node1_,
  &node1_,
  &node1_,
  &node1_,
  &DB_AllocXAsset_StringTable_,
  &node1_,
  &DB_AllocXAsset_StringTable_,
  NULL,
  &DB_AllocXAsset_StringTable_,
  &DB_AllocXAsset_StringTable_,
  &DB_AllocXAsset_StringTable_,
  &DB_AllocXAsset_StringTable_,
  &DB_AllocXAsset_StringTable_,
  NULL,
  &DB_AllocXAsset_StringTable_,
  &DB_AllocXAsset_StringTable_,
  NULL,
  NULL,
  NULL,
  NULL,
  &DB_AllocXAsset_StringTable_,
  &DB_AllocXAsset_StringTable_
};

void __cdecl DB_FreeXAssetHeader_StringTable_(void *arg, XAssetHeader header)
{
    XAssetPoolEntry<StringTable> **pool = (XAssetPoolEntry<StringTable> **)arg;
    XAssetPoolEntry<StringTable> *oldFreeHead = *pool;
    *pool = (XAssetPoolEntry<StringTable> *)header.xmodelPieces;
    header.xmodelPieces->name = (const char *)oldFreeHead;
}

void __cdecl NULLSUB(void *crap, XAssetHeader head)
{
    (void)crap;
    (void)head;
}

void(__cdecl *DB_FreeXAssetHeaderHandler[ASSET_TYPE_COUNT])(void *, XAssetHeader) =
{
  DB_FreeXAssetHeader_StringTable_,
  DB_FreeXAssetHeader_StringTable_,
  DB_FreeXAssetHeader_StringTable_,
  DB_FreeXAssetHeader_StringTable_,
  DB_FreeMaterial,
  DB_FreeXAssetHeader_StringTable_,
  DB_FreeXAssetHeader_StringTable_,
  DB_FreeXAssetHeader_StringTable_,
  DB_FreeXAssetHeader_StringTable_,
  DB_FreeXAssetHeader_StringTable_,
  NULLSUB,
  NULLSUB,
  NULLSUB,
  NULLSUB,
  NULLSUB,
  DB_FreeXAssetHeader_StringTable_,
  NULLSUB,
  DB_FreeXAssetHeader_StringTable_,
  NULL,
  DB_FreeXAssetHeader_StringTable_,
  DB_FreeXAssetHeader_StringTable_,
  DB_FreeXAssetHeader_StringTable_,
  DB_FreeXAssetHeader_StringTable_,
  DB_FreeXAssetHeader_StringTable_,
  NULL,
  DB_FreeXAssetHeader_StringTable_,
  DB_FreeXAssetHeader_StringTable_,
  NULL,
  NULL,
  NULL,
  NULL,
  DB_FreeXAssetHeader_StringTable_,
  DB_FreeXAssetHeader_StringTable_
};

static void __cdecl DB_InitSingleton(void *pool, int32_t size)
{
    (void)pool;
    if (size != 1)
        MyAssertHandler(".\\database\\db_registry.cpp", 528, 0, "%s\\n\\t(size) = %i", "(size == 1)", size);
}

static XAssetHeader __cdecl DB_AllocXAssetHeader(XAssetType type)
{
    XAssetHeader header;
    header.data = DB_AllocXAssetHeaderHandler[type](DB_XAssetPool[type]).data;
    if (!header.data)
    {
        Sys_UnlockWrite(&db_hashCritSect);
        Com_PrintError(CON_CHANNEL_ERROR, "Exceeded limit of %d '%s' assets.\\n", g_poolSize[type], g_assetNames[type]);
        DB_EnumXAssets(type, (void(__cdecl *)(XAssetHeader, void *))DB_PrintAssetName, &type, 1);
        Com_Error(ERR_DROP, "Exceeded limit of %d '%s' assets.\\n", g_poolSize[type], g_assetNames[type]);
    }
    return header;
}

static void __cdecl DB_FreeXAssetHeader(XAssetType type, XAssetHeader header)
{
    if (DB_FreeXAssetHeaderHandler[type])
        DB_FreeXAssetHeaderHandler[type](DB_XAssetPool[type], header);
}

static XAssetEntryPoolEntry *__cdecl DB_AllocXAssetEntry(XAssetType type, uint8_t zoneIndex)
{
    XAssetEntryPoolEntry *freeHead = g_freeAssetEntryHead;
    if (!freeHead)
    {
        Sys_UnlockWrite(&db_hashCritSect);
        Com_Error(ERR_DROP, "Could not allocate asset - increase XASSET_ENTRY_POOL_SIZE");
    }
    g_freeAssetEntryHead = freeHead->next;
    freeHead->entry.asset.type = type;
    freeHead->entry.asset.header = DB_AllocXAssetHeader(type);
    freeHead->entry.zoneIndex = zoneIndex;
    freeHead->entry.inuse = 0;
    freeHead->entry.nextHash = 0;
    freeHead->entry.nextOverride = 0;
    return freeHead;
}

static void __cdecl DB_PrintAssetName(XAssetHeader header, int32_t *data)
{
    const char *XAssetHeaderName = DB_GetXAssetHeaderName(*data, &header);
    Com_Printf(CON_CHANNEL_DONT_FILTER, "%s\\n", XAssetHeaderName);
}

static void __cdecl DB_RemoveWindowFocus(windowDef_t *window)
{
    for (uint32_t i = 0; i < 1; i++)
        window->dynamicFlags[0] &= ~2u;
}

static void __cdecl DB_DynamicCloneMenu(XAssetHeader from, XAssetHeader to, int32_t swag)
{
    (void)swag;
    windowDef_t *toWindow;
    windowDef_t *fromWindow;

    to.xmodelPieces[6].pieces = from.xmodelPieces[6].pieces;
    for (int32_t toIndex = 0; toIndex < (int)(uintptr_t)to.xmodelPieces[13].pieces; ++toIndex)
    {
        toWindow = *reinterpret_cast<windowDef_t **>(static_cast<uintptr_t>(static_cast<uint32_t>(to.xmodelPieces[23].numpieces)) + sizeof(windowDef_t *) * static_cast<uintptr_t>(toIndex));
        if (toWindow->name)
        {
            for (int32_t fromIndex = 0; fromIndex < (int)(uintptr_t)from.xmodelPieces[13].pieces; ++fromIndex)
            {
                fromWindow = *reinterpret_cast<windowDef_t **>(static_cast<uintptr_t>(static_cast<uint32_t>(from.xmodelPieces[23].numpieces)) + sizeof(windowDef_t *) * static_cast<uintptr_t>(fromIndex));
                if (fromWindow->name && !strcmp(fromWindow->name, toWindow->name))
                {
                    toWindow->dynamicFlags[0] = fromWindow->dynamicFlags[0];
                    break;
                }
            }
        }
        DB_RemoveWindowFocus(toWindow);
    }
}

void __cdecl DB_EnumXAssetsFor(
    fileData_s *fileData,
    int32_t fileDataType,
    void(__cdecl *func)(void*, void*),
    void *inData)
{
    while (fileData)
    {
        if (fileData->type == fileDataType && fileData->type == 5)
            func(fileData->data, inData);
        fileData = fileData->next;
    }
}

static void __cdecl DB_EnumXAssets_LoadObj(XAssetType type, void(*func)(void*, void*), void *inData)
{
    uint32_t hash;
    switch (type)
    {
    case ASSET_TYPE_XMODEL:
        for (hash = 0; hash < 0x400; ++hash)
            DB_EnumXAssetsFor(com_fileDataHashTable[hash], 5, func, inData);
        break;
    case ASSET_TYPE_MATERIAL:
        R_EnumMaterials((void(__cdecl*)(Material*, void*))func, inData);
        break;
    case ASSET_TYPE_TECHNIQUE_SET:
        R_EnumTechniqueSets((void(__cdecl*)(MaterialTechniqueSet*, void*))func, inData);
        break;
    case ASSET_TYPE_IMAGE:
        R_EnumImages((void(__cdecl*)(GfxImage*, void*))func, inData);
        break;
    default:
        return;
    }
}

static int32_t __cdecl DB_GetAllXAssetOfType_LoadObj(XAssetType type, XAssetHeader *assets, int32_t maxCount)
{
    AssetList assetList;
    assetList.assets = assets;
    assetList.assetCount = 0;
    assetList.maxCount = maxCount;
    DB_EnumXAssets(type, (void(__cdecl*)(XAssetHeader, void*))Hunk_AddAsset, &assetList, 0);
    return assetList.assetCount;
}

void __cdecl R_EnumMaterials(void(__cdecl *func)(Material *, void *), void *data)
{
    for (uint32_t hashIndex = 0; hashIndex < 0x800; ++hashIndex)
    {
        Material *header = rg.materialHashTable[hashIndex];
        if (header)
            func(header, data);
    }
}

void __cdecl R_EnumTechniqueSets(void(__cdecl *func)(MaterialTechniqueSet *, void *), void *data)
{
    for (uint32_t hashIndex = 0; hashIndex < 0x400; ++hashIndex)
    {
        MaterialTechniqueSet *header = materialGlobals.techniqueSetHashTable[hashIndex];
        if (header)
            func(header, data);
    }
}

void __cdecl R_EnumImages(void(__cdecl *func)(GfxImage *, void *), void *data)
{
    for (uint32_t imageIndex = 0; imageIndex < IMAGE_HASH_TABLE_SIZE; ++imageIndex)
    {
        GfxImage *header = imageGlobals.imageHashTable[imageIndex];
        if (header && !Image_IsProg(header))
            func(header, data);
    }
}

void __cdecl DB_EnumXAssets_FastFile(
    XAssetType type,
    void(__cdecl *func)(XAssetHeader, void *),
    void *inData,
    bool includeOverride)
{
    uint32_t hash;
    uint32_t assetEntryIndex;
    XAssetEntryPoolEntry *assetEntry;
    uint32_t overrideAssetEntryIndex;

    InterlockedIncrement(&db_hashCritSect.readCount);
    while (db_hashCritSect.writeCount)
        NET_Sleep(0);
    for (hash = 0; hash < 0x8000; ++hash)
    {
        for (assetEntryIndex = db_hashTable[hash]; assetEntryIndex; assetEntryIndex = assetEntry->entry.nextHash)
        {
            assetEntry = &g_assetEntryPool[assetEntryIndex];
            if (assetEntry->entry.asset.type == type)
            {
                func(assetEntry->entry.asset.header, inData);
                if (includeOverride)
                {
                    for (overrideAssetEntryIndex = assetEntry->entry.nextOverride;
                        overrideAssetEntryIndex;
                        overrideAssetEntryIndex = g_assetEntryPool[overrideAssetEntryIndex].entry.nextOverride)
                    {
                        func(g_assetEntryPool[overrideAssetEntryIndex].entry.asset.header, inData);
                    }
                }
            }
        }
    }
    if (db_hashCritSect.readCount <= 0)
        MyAssertHandler(
            "c:\\trees\\cod3\\src\\gfx_d3d\\../qcommon/threads_interlock.h",
            76,
            0,
            "%s",
            "critSect->readCount > 0");
    InterlockedDecrement(&db_hashCritSect.readCount);
}

static void DB_PostLoadXZone()
{
    uint32_t i;
    int32_t remoteScreenUpdateNesting;

    iassert(Sys_IsMainThread() || Sys_IsRenderThread());
    iassert(!g_loadingZone);
    iassert(!g_zoneInfoCount);

    if (!Sys_IsDatabaseReady2())
    {
        if (g_copyInfoCount)
        {
            remoteScreenUpdateNesting = 0;
            if (!Sys_IsMainThread()
                || (++g_mainThreadBlocked,
                    remoteScreenUpdateNesting = R_PopRemoteScreenUpdate(),
                    --g_mainThreadBlocked,
                    g_copyInfoCount))
            {
                DB_ArchiveAssets();
                Sys_LockWrite(&db_hashCritSect);
                for (i = 0; i < g_copyInfoCount; ++i)
                    DB_LinkXAssetEntry((XAssetEntryPoolEntry *)g_copyInfo[i], 1);
                g_copyInfoCount = 0;
                Sys_UnlockWrite(&db_hashCritSect);
                Material_DirtyTechniqueSetOverrides();
                Material_OverrideTechniqueSets();
                DB_UnarchiveAssets();
                if (Sys_IsMainThread())
                    R_PushRemoteScreenUpdate(remoteScreenUpdateNesting);
                Sys_DatabaseCompleted2();
            }
            else
            {
                R_PushRemoteScreenUpdate(remoteScreenUpdateNesting);
            }
        }
        else
        {
            DB_ExternalInitAssets();
            Sys_DatabaseCompleted2();
        }
    }
}

void __cdecl DB_UpdateDebugZone()
{
    XZoneInfo zoneInfo[2];
    if (g_debugZoneName[0])
    {
        zoneInfo[0].name = 0;
        zoneInfo[0].allocFlags = 0;
        zoneInfo[1].name = g_debugZoneName;
        Com_SyncThreads();
        zoneInfo[0].freeFlags = DB_ZONE_DEV;
        zoneInfo[1].allocFlags = DB_ZONE_DEV;
        zoneInfo[1].freeFlags = DB_ZONE_DEV;
        DB_LoadXAssets(zoneInfo, 2u, 1);
        CG_VisionSetMyChanges();
    }
}


/* DB Registry core linkage restore. */
void __cdecl DB_SetInitializing(bool inUse)
{
    g_initializing = inUse;
}

void __cdecl DB_Update()
{
    if (!Sys_IsMainThread())
        MyAssertHandler(".\\database\\db_registry.cpp", 2805, 0, "%s", "Sys_IsMainThread()");
    if (!Sys_IsDatabaseReady2() && Sys_IsDatabaseReady())
        DB_PostLoadXZone();
}

bool __cdecl DB_OverrideAsset(uint32_t newZoneIndex, uint32_t existingZoneIndex)
{
    if (!newZoneIndex)
        MyAssertHandler(".\\database\\db_registry.cpp", 2959, 0, "%s", "newZoneIndex");
    if (!existingZoneIndex)
        MyAssertHandler(".\\database\\db_registry.cpp", 2960, 0, "%s", "existingZoneIndex");
    return g_zones[newZoneIndex].flags >= g_zones[existingZoneIndex].flags;
}

void __cdecl DB_GetXAsset(XAssetType type, XAssetHeader header)
{
    uint32_t assetEntryIndex;
    XAsset asset;
    const char *name;
    XAssetEntry *assetEntry;

    asset.type = type;
    asset.header = header;
    name = DB_GetXAssetName(&asset);
    for (assetEntryIndex = db_hashTable[DB_HashForName(name, type)]; ; assetEntryIndex = assetEntry->nextHash)
    {
        if (!assetEntryIndex)
            MyAssertHandler(".\\database\\db_registry.cpp", 3163, 0, "%s", "assetEntryIndex");
        assetEntry = &g_assetEntryPool[assetEntryIndex].entry;
        if (assetEntry->asset.type == type && assetEntry->asset.header.xmodelPieces == header.xmodelPieces)
            break;
    }
    assetEntry->inuse = 1;
}

static void __cdecl DB_DelayedCloneXAsset(XAssetEntry *newEntry)
{
    const char *XAssetTypeName;
    const char *XAssetName;
    uint32_t i;

    if (g_sync)
    {
        DB_LinkXAssetEntry((XAssetEntryPoolEntry *)newEntry, 1);
    }
    else
    {
        if (g_copyInfoCount >= 0x800)
        {
            Com_Printf(CON_CHANNEL_DONT_FILTER, "g_copyInfo exceeded\\n");
            for (i = 0; i < 0x800; ++i)
            {
                XAssetName = DB_GetXAssetName(&g_copyInfo[i]->asset);
                XAssetTypeName = DB_GetXAssetTypeName(g_copyInfo[i]->asset.type);
                Com_Printf(CON_CHANNEL_DONT_FILTER, "%s: %s\\n", XAssetTypeName, XAssetName);
            }
            Sys_Error("g_copyInfo exceeded");
        }
        g_copyInfo[g_copyInfoCount++] = newEntry;
    }
}

void DB_SyncLostDevice()
{
    if (g_isRecoveringLostDevice)
    {
        if (g_mayRecoverLostAssets)
            MyAssertHandler(".\\database\\db_registry.cpp", 2945, 0, "%s", "!g_mayRecoverLostAssets");
        g_mayRecoverLostAssets = 1;
        do
            NET_Sleep(0x19u);
        while (g_isRecoveringLostDevice);
        if (g_mayRecoverLostAssets)
            MyAssertHandler(".\\database\\db_registry.cpp", 2951, 0, "%s", "!g_mayRecoverLostAssets");
    }
}

XAssetHeader __cdecl DB_AddXAsset(XAssetType type, XAssetHeader header)
{
    XAssetEntryPoolEntry *existingEntry;
    XAssetEntryPoolEntry newEntry;

    newEntry.entry.asset.type = type;
    newEntry.entry.asset.header = header;
    Sys_LockWrite(&db_hashCritSect);
    existingEntry = DB_LinkXAssetEntry(&newEntry, 0);
    Sys_UnlockWrite(&db_hashCritSect);
    DB_SyncLostDevice();
    return existingEntry->entry.asset.header;
}

XAssetEntryPoolEntry *__cdecl DB_LinkXAssetEntry(XAssetEntryPoolEntry *newEntry, int32_t allowOverride)
{
    int32_t v2;
    const char *XAssetName;
    XAssetEntryPoolEntry *existingEntry;
    uint32_t hash;
    uint32_t existingEntryIndex;
    XAssetEntryPoolEntry *overrideAssetEntry;
    XAsset asset;
    int32_t isStubAsset;
    const char *name;
    uint8_t zoneIndex;
    XAssetType type;
    uint16_t *pOverrideAssetEntryIndex;
    XAssetSize assetSize;

    name = DB_GetXAssetName(&newEntry->entry.asset);
    v2 = *name;
    isStubAsset = v2 == ',';
    if (v2 == ',')
        ++name;
    type = newEntry->entry.asset.type;
    hash = DB_HashForName(name, type);
    existingEntry = NULL;

    for (existingEntryIndex = db_hashTable[hash]; existingEntryIndex; existingEntryIndex = existingEntry->entry.nextHash)
    {
        existingEntry = &g_assetEntryPool[existingEntryIndex];
        if (existingEntry->entry.asset.type == type)
        {
            XAssetName = DB_GetXAssetName(&existingEntry->entry.asset);
            if (!I_stricmp(XAssetName, name))
                break;
        }
    }

    if (allowOverride)
    {
        iassert(!isStubAsset);
    }
    else
    {
        if (isStubAsset)
        {
            if (!existingEntryIndex)
                return (XAssetEntryPoolEntry *)DB_CreateDefaultEntry(type, (char *)name);
            iassert(existingEntry);
            return existingEntry;
        }

        asset.type = newEntry->entry.asset.type;
        asset.header = newEntry->entry.asset.header;
        newEntry = DB_AllocXAssetEntry(asset.type, g_zoneIndex);
        DB_CloneXAssetInternal(&asset, &newEntry->entry.asset);
    }

    if (!existingEntryIndex)
    {
        newEntry->entry.nextHash = db_hashTable[hash];
        db_hashTable[hash] = static_cast<uint16_t>(newEntry - g_assetEntryPool);
        return newEntry;
    }

    iassert(existingEntry);
    if (existingEntry->entry.zoneIndex)
    {
        iassert(existingEntry->entry.zoneIndex != newEntry->entry.zoneIndex);

        if (!*g_defaultAssetName[type] && type != ASSET_TYPE_RAWFILE && type != ASSET_TYPE_MAP_ENTS)
        {
            Sys_UnlockWrite(&db_hashCritSect);
            Com_Error(
                ERR_DROP,
                "Attempting to override asset '%s' from zone '%s' with zone '%s'",
                name,
                g_zones[existingEntry->entry.zoneIndex].name,
                g_zones[newEntry->entry.zoneIndex].name);
        }

        if (!DB_OverrideAsset(newEntry->entry.zoneIndex, existingEntry->entry.zoneIndex))
        {
            for (pOverrideAssetEntryIndex = &existingEntry->entry.nextOverride;
                *pOverrideAssetEntryIndex;
                pOverrideAssetEntryIndex = &overrideAssetEntry->entry.nextOverride)
            {
                overrideAssetEntry = &g_assetEntryPool[*pOverrideAssetEntryIndex];
                if (DB_OverrideAsset(newEntry->entry.zoneIndex, overrideAssetEntry->entry.zoneIndex))
                    break;
            }

            newEntry->entry.nextOverride = *pOverrideAssetEntryIndex;
            *pOverrideAssetEntryIndex = static_cast<uint16_t>(newEntry - g_assetEntryPool);
            return existingEntry;
        }

        goto LABEL_46;
    }

    iassert(g_defaultAssetName[type][0]);
    iassert(!existingEntry->entry.nextOverride);
    iassert(g_defaultAssetCount);

    if (!allowOverride)
    {
    LABEL_46:
        if (allowOverride)
        {
            if (!existingEntry->entry.zoneIndex)
                MyAssertHandler(
                    ".\\database\\db_registry.cpp",
                    3096,
                    0,
                    "%s",
                    "existingEntry->zoneIndex");

            if (existingEntry->entry.inuse)
            {
                varXAsset = &existingEntry->entry.asset;
                Mark_XAsset();
            }

            newEntry->entry.nextOverride = existingEntry->entry.nextOverride;
            existingEntry->entry.nextOverride = static_cast<uint16_t>(newEntry - g_assetEntryPool);
            asset.header.xmodelPieces = reinterpret_cast<XModelPieces *>(&assetSize);
            asset.type = type;
            DB_CloneXAssetInternal(&existingEntry->entry.asset, &asset);

            zoneIndex = existingEntry->entry.zoneIndex;
            DB_CloneXAssetEntry(&newEntry->entry, &existingEntry->entry);
            DB_CloneXAssetInternal(&asset, &newEntry->entry.asset);
            newEntry->entry.zoneIndex = zoneIndex;
        }
        else
        {
            DB_DelayedCloneXAsset(&newEntry->entry);
        }

        return existingEntry;
    }

    --g_defaultAssetCount;

    if (existingEntry->entry.inuse)
    {
        varXAsset = &existingEntry->entry.asset;
        Mark_XAsset();
    }

    DB_CloneXAssetEntry(&newEntry->entry, &existingEntry->entry);
    DB_FreeXAssetEntry(newEntry);
    return existingEntry;
}


/* Database asset Load/Mark implementations restored from upstream KisakCOD. */

void __cdecl Load_PhysPresetAsset(XAssetHeader *physPreset)
{
    physPreset->xmodelPieces = DB_AddXAsset(ASSET_TYPE_PHYSPRESET, (XAssetHeader)physPreset->xmodelPieces).xmodelPieces;
}

void __cdecl Mark_PhysPresetAsset(PhysPreset *physPreset)
{
    DB_GetXAsset(ASSET_TYPE_PHYSPRESET, (XAssetHeader)physPreset);
}

void __cdecl Load_XAnimPartsAsset(XAssetHeader *parts)
{
    parts->xmodelPieces = DB_AddXAsset(ASSET_TYPE_XANIMPARTS, (XAssetHeader)parts->xmodelPieces).xmodelPieces;
}

void __cdecl Mark_XAnimPartsAsset(XAnimParts *parts)
{
    DB_GetXAsset(ASSET_TYPE_XANIMPARTS, (XAssetHeader)parts);
}

void __cdecl Load_XModelAsset(XAssetHeader *model)
{
    model->xmodelPieces = DB_AddXAsset(ASSET_TYPE_XMODEL, (XAssetHeader)model->xmodelPieces).xmodelPieces;
}

void __cdecl Mark_XModelAsset(XModel *model)
{
    DB_GetXAsset(ASSET_TYPE_XMODEL, (XAssetHeader)model);
}

void __cdecl Load_MaterialAsset(XAssetHeader *material)
{
    material->xmodelPieces = DB_AddXAsset(ASSET_TYPE_MATERIAL, (XAssetHeader)material->xmodelPieces).xmodelPieces;
}

void __cdecl Mark_MaterialAsset(Material *material)
{
    DB_GetXAsset(ASSET_TYPE_MATERIAL, (XAssetHeader)material);
}

void __cdecl Load_MaterialTechniqueSetAsset(XAssetHeader *techniqueSet)
{
    techniqueSet->xmodelPieces = DB_AddXAsset(ASSET_TYPE_TECHNIQUE_SET, (XAssetHeader)techniqueSet->xmodelPieces).xmodelPieces;
    Material_OriginalRemapTechniqueSet(techniqueSet->techniqueSet);
    Material_UploadShaders(techniqueSet->techniqueSet);
}

void __cdecl Mark_MaterialTechniqueSetAsset(MaterialTechniqueSet *techniqueSet)
{
    DB_GetXAsset(ASSET_TYPE_TECHNIQUE_SET, (XAssetHeader)techniqueSet);
}

void __cdecl Load_GfxImageAsset(XAssetHeader *image)
{
    image->xmodelPieces = DB_AddXAsset(ASSET_TYPE_IMAGE, (XAssetHeader)image->xmodelPieces).xmodelPieces;
}

void __cdecl Mark_GfxImageAsset(GfxImage *image)
{
    DB_GetXAsset(ASSET_TYPE_IMAGE, (XAssetHeader)image);
}

void __cdecl Load_snd_alias_list_Asset(XAssetHeader *sound)
{
    sound->xmodelPieces = DB_AddXAsset(ASSET_TYPE_SOUND, (XAssetHeader)sound->xmodelPieces).xmodelPieces;
}

void __cdecl Mark_snd_alias_list_Asset(snd_alias_list_t *sound)
{
    DB_GetXAsset(ASSET_TYPE_SOUND, (XAssetHeader)sound);
}

void __cdecl Load_SndCurveAsset(XAssetHeader *sndCurve)
{
    sndCurve->xmodelPieces = DB_AddXAsset(ASSET_TYPE_SOUND_CURVE, (XAssetHeader)sndCurve->xmodelPieces).xmodelPieces;
}

void __cdecl Mark_SndCurveAsset(SndCurve *sndCurve)
{
    DB_GetXAsset(ASSET_TYPE_SOUND_CURVE, (XAssetHeader)sndCurve);
}

void __cdecl Load_LoadedSoundAsset(XAssetHeader *loadSnd)
{
    loadSnd->xmodelPieces = DB_AddXAsset(ASSET_TYPE_LOADED_SOUND, (XAssetHeader)loadSnd->xmodelPieces).xmodelPieces;
}

void __cdecl Mark_LoadedSoundAsset(LoadedSound *loadSnd)
{
    DB_GetXAsset(ASSET_TYPE_LOADED_SOUND, (XAssetHeader)loadSnd);
}

void __cdecl Load_ClipMapAsset(XAssetHeader *clipMap)
{
#ifdef KISAK_MP
    clipMap->clipMap = DB_AddXAsset(ASSET_TYPE_CLIPMAP_PVS, (XAssetHeader)clipMap->clipMap).clipMap;
#elif KISAK_SP
    clipMap->clipMap = DB_AddXAsset(ASSET_TYPE_CLIPMAP, (XAssetHeader)clipMap->clipMap).clipMap;
#endif
}

void __cdecl Mark_ClipMapAsset(clipMap_t *clipMap)
{
#ifdef KISAK_MP
    DB_GetXAsset(ASSET_TYPE_CLIPMAP_PVS, (XAssetHeader)clipMap);
#elif KISAK_SP
    DB_GetXAsset(ASSET_TYPE_CLIPMAP, (XAssetHeader)clipMap);
#endif
}

void __cdecl Load_ComWorldAsset(XAssetHeader *comWorld)
{
    comWorld->xmodelPieces = DB_AddXAsset(ASSET_TYPE_COMWORLD, (XAssetHeader)comWorld->xmodelPieces).xmodelPieces;
}

void __cdecl Mark_ComWorldAsset(ComWorld *comWorld)
{
    DB_GetXAsset(ASSET_TYPE_COMWORLD, (XAssetHeader)comWorld);
}

void __cdecl Load_GameWorldSpAsset(XAssetHeader *gameWorldSp)
{
    gameWorldSp->xmodelPieces = DB_AddXAsset(ASSET_TYPE_GAMEWORLD_SP, (XAssetHeader)gameWorldSp->xmodelPieces).xmodelPieces;
}

void __cdecl Mark_GameWorldSpAsset(GameWorldSp *gameWorldSp)
{
    DB_GetXAsset(ASSET_TYPE_GAMEWORLD_SP, (XAssetHeader)gameWorldSp);
}

void __cdecl Load_GameWorldMpAsset(XAssetHeader *gameWorldMp)
{
    gameWorldMp->xmodelPieces = DB_AddXAsset(ASSET_TYPE_GAMEWORLD_MP, (XAssetHeader)gameWorldMp->xmodelPieces).xmodelPieces;
}

void __cdecl Mark_GameWorldMpAsset(GameWorldMp *gameWorldMp)
{
    DB_GetXAsset(ASSET_TYPE_GAMEWORLD_MP, (XAssetHeader)gameWorldMp);
}

void __cdecl Load_MapEntsAsset(XAssetHeader *mapEnts)
{
    mapEnts->xmodelPieces = DB_AddXAsset(ASSET_TYPE_MAP_ENTS, (XAssetHeader)mapEnts->xmodelPieces).xmodelPieces;
}

void __cdecl Mark_MapEntsAsset(MapEnts *mapEnts)
{
    DB_GetXAsset(ASSET_TYPE_MAP_ENTS, (XAssetHeader)mapEnts);
}

void __cdecl Load_GfxWorldAsset(XAssetHeader *gfxWorld)
{
    gfxWorld->xmodelPieces = DB_AddXAsset(ASSET_TYPE_GFXWORLD, (XAssetHeader)gfxWorld->xmodelPieces).xmodelPieces;
}

void __cdecl Mark_GfxWorldAsset(GfxWorld *gfxWorld)
{
    DB_GetXAsset(ASSET_TYPE_GFXWORLD, (XAssetHeader)gfxWorld);
}

void __cdecl Load_LightDefAsset(XAssetHeader *lightDef)
{
    lightDef->xmodelPieces = DB_AddXAsset(ASSET_TYPE_LIGHT_DEF, (XAssetHeader)lightDef->xmodelPieces).xmodelPieces;
}

void __cdecl Mark_LightDefAsset(GfxLightDef *lightDef)
{
    DB_GetXAsset(ASSET_TYPE_LIGHT_DEF, (XAssetHeader)lightDef);
}

void __cdecl Load_FontAsset(XAssetHeader *font)
{
    font->xmodelPieces = DB_AddXAsset(ASSET_TYPE_FONT, (XAssetHeader)font->xmodelPieces).xmodelPieces;
}

void __cdecl Mark_FontAsset(Font_s *font)
{
    DB_GetXAsset(ASSET_TYPE_FONT, (XAssetHeader)font);
}

void __cdecl Load_MenuListAsset(XAssetHeader *menuList)
{
    menuList->xmodelPieces = DB_AddXAsset(ASSET_TYPE_MENULIST, (XAssetHeader)menuList->xmodelPieces).xmodelPieces;
}

void __cdecl Mark_MenuListAsset(MenuList *menuList)
{
    DB_GetXAsset(ASSET_TYPE_MENULIST, (XAssetHeader)menuList);
}

void __cdecl Load_MenuAsset(XAssetHeader *menu)
{
    XAssetHeader header;
    int32_t i;

    header.menu = menu->menu;
    menu->menu = DB_AddXAsset(ASSET_TYPE_MENU, *menu).menu;

    for (i = 0; i < header.menu->itemCount; ++i)
        header.menu->items[i]->parent = menu->menu;
}

void __cdecl Mark_MenuAsset(menuDef_t *menu)
{
    DB_GetXAsset(ASSET_TYPE_MENU, (XAssetHeader)menu);
}

void __cdecl Load_LocalizeEntryAsset(XAssetHeader *localize)
{
    localize->xmodelPieces = DB_AddXAsset(ASSET_TYPE_LOCALIZE_ENTRY, (XAssetHeader)localize->xmodelPieces).xmodelPieces;
}

void __cdecl Mark_LocalizeEntryAsset(LocalizeEntry *localize)
{
    DB_GetXAsset(ASSET_TYPE_LOCALIZE_ENTRY, (XAssetHeader)localize);
}

void __cdecl Load_WeaponDefAsset(XAssetHeader *weapon)
{
    weapon->xmodelPieces = DB_AddXAsset(ASSET_TYPE_WEAPON, (XAssetHeader)weapon->xmodelPieces).xmodelPieces;
}

void __cdecl Mark_WeaponDefAsset(WeaponDef *weapon)
{
    DB_GetXAsset(ASSET_TYPE_WEAPON, (XAssetHeader)weapon);
}

void __cdecl Load_FxEffectDefAsset(XAssetHeader *fx)
{
    fx->xmodelPieces = DB_AddXAsset(ASSET_TYPE_FX, (XAssetHeader)fx->xmodelPieces).xmodelPieces;
}

void __cdecl Mark_FxEffectDefAsset(FxEffectDef *fx)
{
    DB_GetXAsset(ASSET_TYPE_FX, (XAssetHeader)fx);
}

void __cdecl Load_FxEffectDefFromName(const char **name)
{
    if (*name)
        *(XAssetHeader *)name = DB_FindXAssetHeader(ASSET_TYPE_FX, *name);
}

void __cdecl Load_FxImpactTableAsset(XAssetHeader *impactFx)
{
    impactFx->xmodelPieces = DB_AddXAsset(ASSET_TYPE_IMPACT_FX, (XAssetHeader)impactFx->xmodelPieces).xmodelPieces;
}

void __cdecl Mark_FxImpactTableAsset(FxImpactTable *impactFx)
{
    DB_GetXAsset(ASSET_TYPE_IMPACT_FX, (XAssetHeader)impactFx);
}

void __cdecl Load_RawFileAsset(XAssetHeader *rawfile)
{
    rawfile->xmodelPieces = DB_AddXAsset(ASSET_TYPE_RAWFILE, (XAssetHeader)rawfile->xmodelPieces).xmodelPieces;
}

void __cdecl Mark_RawFileAsset(RawFile *rawfile)
{
    DB_GetXAsset(ASSET_TYPE_RAWFILE, (XAssetHeader)rawfile);
}

void __cdecl Load_StringTableAsset(XAssetHeader *stringTable)
{
    stringTable->xmodelPieces = DB_AddXAsset(ASSET_TYPE_STRINGTABLE, (XAssetHeader)stringTable->xmodelPieces).xmodelPieces;
}

void __cdecl Mark_StringTableAsset(StringTable *stringTable)
{
    DB_GetXAsset(ASSET_TYPE_STRINGTABLE, (XAssetHeader)stringTable);
}


/* Water asset picmip path restored from upstream r_water.cpp. */

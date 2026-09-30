#include <universal/q_shared.h>
#include "database.h"

#include <qcommon/threads.h>
#ifndef __SWITCH__
#include <win32/win_local.h>
#endif
#include <universal/com_files.h>

#include <gfx_d3d/r_image.h>
#include <gfx_d3d/r_buffers.h>

#ifdef __SWITCH__
extern void Switch_LogWrite(const char *msg);
extern uint8_t *AllocLoad_raw_byte();
extern const char *varConstChar;
extern XAssetHeader *varXAssetHeader;
extern void __cdecl Load_XAssetHeader(bool atStreamStart);
#endif

//uint32_t volatile g_loadingAssets      828e3f3c     db_file_load.obj
//int32_t marker_db_file_load  828e3f40     db_file_load.obj

struct DB_LoadData // sizeof=0x68
{                                       // ...
    void* f;                            // ...
    const char* filename;               // ...
    XZoneMemory* zoneMem;               // ...
    int32_t outstandingReads;               // ...
#ifdef __SWITCH__
    uint64_t switchFileOffset;
    uint32_t switchLastRead;
#else
    OVERLAPPED overlapped;
#endif
    z_stream_s stream;                  // ...
    uint8_t* compressBufferStart; // ...
    uint8_t* compressBufferEnd; // ...
    void(__cdecl* interrupt)();        // ...
    int32_t allocType;                      // ...
};

#ifdef KISAK_MP
bool g_minimumFastFileLoaded;
#elif KISAK_SP
bool g_anyFastFileLoaded;
#endif

DB_LoadData g_load;
LONG g_loadedSize;
LONG g_loadedExternalBytes;
volatile int32_t g_totalSize;
volatile int32_t g_totalExternalBytes;
int32_t g_trackLoadProgress;

extern XAssetList g_varXAssetList;

// --- file-local forward declarations (moved out of database.h) ---
static void __cdecl DB_CancelLoadXFile();
static int32_t DB_WaitXFileStage();
static void DB_ReadXFileStage();
static int32_t __cdecl DB_ReadData();
static void Load_XAssetListCustom()
{
#ifdef __SWITCH__
    struct SerializedScriptStringList
    {
        uint32_t count;
        uint32_t strings;
    };
    struct SerializedXAssetList
    {
        SerializedScriptStringList stringList;
        uint32_t assetCount;
        uint32_t assets;
    };

    SerializedXAssetList serialized{};
    DB_LoadXFileData(reinterpret_cast<uint8_t *>(&serialized), sizeof(serialized));
    DB_IncStreamPos(sizeof(serialized));

    varXAssetList = &g_varXAssetList;
    memset(varXAssetList, 0, sizeof(*varXAssetList));
    varXAssetList->stringList.count =
        static_cast<int>(serialized.stringList.count);
    varXAssetList->assetCount =
        static_cast<int>(serialized.assetCount);
    varXAssetList->assets =
        reinterpret_cast<XAsset *>(static_cast<uintptr_t>(serialized.assets));

    DB_PushStreamPos(4);
    if (serialized.stringList.strings)
    {
        const uint32_t count = serialized.stringList.count;
        std::vector<uint32_t> serializedStrings(count);

        // Serialized XString/TmpString pointers are 32-bit on disk. The
        // runtime array contains native pointers, so first read the entire
        // serialized pointer array and reserve the native destination.
        if (count)
            DB_LoadXFileData(reinterpret_cast<uint8_t *>(serializedStrings.data()),
                             sizeof(uint32_t) * static_cast<size_t>(count));

        const char **dst =
            reinterpret_cast<const char **>(DB_AllocStreamPos(3));
        varXAssetList->stringList.strings = dst;
        if (count)
            DB_IncStreamPos(static_cast<int32_t>(
                sizeof(const char *) * static_cast<size_t>(count)));

        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t stringOffset = serializedStrings[i];

            if (!stringOffset)
            {
                dst[i] = nullptr;
            }
            else if (stringOffset == UINT32_MAX)
            {
                dst[i] = reinterpret_cast<const char *>(AllocLoad_raw_byte());
                varConstChar = dst[i];
                Load_XStringCustom((char **)&dst[i]);
            }
            else
            {
                dst[i] = reinterpret_cast<const char *>(
                    DB_ConvertOffsetToPointerValue(stringOffset));
            }
        }
    }
    DB_PopStreamPos();

    char trace[256];
    std::snprintf(trace, sizeof(trace),
        "[SWITCH XASSETLIST] count=%d strings=%u assets=%08x\\n",
        varXAssetList->assetCount,
        serialized.stringList.count,
        serialized.assets);
    Switch_LogWrite(trace);
#else
    varXAssetList = &g_varXAssetList;

    DB_LoadXFileData((uint8_t *)&g_varXAssetList, sizeof(XAssetList));
    DB_PushStreamPos(4);
    varScriptStringList = &varXAssetList->stringList;
    Load_ScriptStringList(0);
    DB_PopStreamPos();
#endif
}

void __cdecl Load_XAssetArrayCustom(int32_t count)
{
#ifdef __SWITCH__
    struct SerializedXAsset
    {
        uint32_t type;
        uint32_t header;
    };

    std::vector<SerializedXAsset> serializedAssets(static_cast<size_t>(count));
    if (count > 0)
    {
        // On disk XAsset is 8 bytes: uint32 type + uint32 header.
        // The native Switch XAsset is larger because header contains a pointer.
        // The native destination has already been reserved by DB_LoadXFileInternal.
        DB_LoadXFileData(
            reinterpret_cast<uint8_t *>(serializedAssets.data()),
            sizeof(SerializedXAsset) * static_cast<size_t>(count));
    }

    XAsset *var = varXAsset;
    for (int32_t i = 0; i < count; ++i)
    {
        const SerializedXAsset &serialized =
            serializedAssets[static_cast<size_t>(i)];

        varXAsset = var;
        memset(varXAsset, 0, sizeof(*varXAsset));
        varXAsset->type = static_cast<XAssetType>(serialized.type);
        memcpy(&varXAsset->header, &serialized.header, sizeof(serialized.header));
        varXAssetHeader = &varXAsset->header;

        Load_XAssetHeader(0);

        if (i < 16)
        {
            const char *assetName = "unresolved";
            if (varXAsset->header.data &&
                varXAsset->type >= 0 &&
                varXAsset->type < ASSET_TYPE_COUNT)
                assetName = DB_GetXAssetName(varXAsset);

            Com_Printf(CON_CHANNEL_FILES,
                "Switch DB: asset[%d] type=%d name=%s header=%p\\n",
                i,
                static_cast<int>(varXAsset->type),
                assetName ? assetName : "<null>",
                varXAsset->header.data);
        }
        ++var;
    }
#else
    XAsset *var;
    int32_t i;

    Load_Stream(1, (uint8_t *)varXAsset, 8 * count);
    var = varXAsset;
    for (i = 0; i < count; ++i)
    {
        varXAsset = var;
        Load_XAsset(0);
        ++var;
    }
#endif
}


void __cdecl DB_ResetZoneSize(int32_t trackLoadProgress)
{
    g_totalSize = 0;
    g_loadedSize = 0;
    g_totalExternalBytes = 0;
    g_loadedExternalBytes = 0;
    g_trackLoadProgress = trackLoadProgress;
}

void __cdecl DB_LoadXFile(
    const char *path,
    void *f,
    const char *filename,
    XZoneMemory *zoneMem,
    void(__cdecl *interrupt)(),
    uint8_t *buf,
    int32_t allocType)
{
    if (((uintptr_t)buf & 3) != 0)
        MyAssertHandler(".\\database\\db_file_load.cpp", 749, 0, "%s", "!(reinterpret_cast< psize_int >( buf ) & 3)");
    memset((uint8_t *)&g_load, 0, sizeof(g_load));
    g_load.f = f;
    g_load.filename = filename;
    g_load.zoneMem = zoneMem;
    g_load.interrupt = interrupt;
    g_load.allocType = allocType;
    if (g_load.compressBufferStart)
        MyAssertHandler(".\\database\\db_file_load.cpp", 762, 0, "%s", "!g_load.compressBufferStart");
    if (!g_load.f)
        MyAssertHandler(".\\database\\db_file_load.cpp", 764, 0, "%s", "g_load.f");
    if (!buf)
        MyAssertHandler(".\\database\\db_file_load.cpp", 766, 0, "%s", "buf");
    g_load.compressBufferStart = buf;
    g_load.compressBufferEnd = buf + 0x80000;
    g_load.stream.next_in = buf;
    g_load.stream.avail_in = 0;
}


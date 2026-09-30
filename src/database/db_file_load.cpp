#include <universal/q_shared.h>
#include "database.h"

#ifdef __SWITCH__
#endif

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
uint32_t g_switchImageAdds = 0;
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
static void Load_XAssetListCustom();
static void __cdecl Load_XAssetArrayCustom(int32_t count)
{
#ifdef __SWITCH__
    struct SerializedXAsset
    {
        uint32_t type;
        uint32_t header;
    };

    uint32_t imageRecords = 0;
    uint32_t imageInline = 0;
    uint32_t imageAlias = 0;
    uint32_t imageNull = 0;
    uint32_t materialRecords = 0;
    uint32_t techsetRecords = 0;
    uint32_t localizeRecords = 0;

    std::vector<SerializedXAsset> serializedAssets(
        count > 0 ? static_cast<size_t>(count) : 0u);

    if (count > 0)
    {
        DB_LoadXFileData(
            reinterpret_cast<uint8_t *>(serializedAssets.data()),
            static_cast<uint32_t>(sizeof(SerializedXAsset) * static_cast<size_t>(count)));
        DB_IncStreamPos(
            static_cast<int32_t>(sizeof(SerializedXAsset) * static_cast<size_t>(count)));
    }

    XAsset *var = varXAsset;
    for (int32_t i = 0; i < count; ++i)
    {
        const SerializedXAsset &serialized = serializedAssets[static_cast<size_t>(i)];

        if (serialized.type == ASSET_TYPE_IMAGE)
        {
            ++imageRecords;
            if (serialized.header == UINT32_MAX || serialized.header == UINT32_MAX - 1)
                ++imageInline;
            else if (serialized.header)
                ++imageAlias;
            else
                ++imageNull;
        }
        else if (serialized.type == ASSET_TYPE_MATERIAL)
        {
            ++materialRecords;
        }
        else if (serialized.type == ASSET_TYPE_TECHNIQUE_SET)
        {
            ++techsetRecords;
        }
        else if (serialized.type == ASSET_TYPE_LOCALIZE_ENTRY)
        {
            ++localizeRecords;
        }

        varXAsset = var;
        memset(varXAsset, 0, sizeof(*varXAsset));
        varXAsset->type = static_cast<XAssetType>(serialized.type);
        memcpy(&varXAsset->header, &serialized.header, sizeof(serialized.header));
        varXAssetHeader = &varXAsset->header;

#ifdef __SWITCH__
        if (serialized.type == ASSET_TYPE_MATERIAL)
        {
            char trace[192];
            std::snprintf(
                trace,
                sizeof(trace),
                "[SWITCH XASSET] material load begin index=%d header=%08x\n",
                i,
                serialized.header);
            Switch_LogWrite(trace);
        }
#endif

        Load_XAssetHeader(0);

#ifdef __SWITCH__
        if (serialized.type == ASSET_TYPE_MATERIAL)
            Switch_LogWrite("[SWITCH XASSET] material Load_XAssetHeader done\n");
#endif

        ++var;
    }

#ifdef __SWITCH__
    {
        char trace[192];
        std::snprintf(trace, sizeof(trace),
            "[SWITCH XASSET] records=%d image=%u inline=%u alias=%u null=%u adds=%u material=%u techset=%u localize=%u\n",
            count, imageRecords, imageInline, imageAlias, imageNull, g_switchImageAdds,
            materialRecords, techsetRecords, localizeRecords);
        Switch_LogWrite(trace);
    }
#endif
#else
    XAsset *var;
    int32_t i;

    Load_Stream(1, (uint8_t *)varXAsset, 8 * count);
    var = varXAsset;
    for (i = 0; i < count; ++i)
    {
        varXAsset = var;
        Load_XAssetHeader(0);
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


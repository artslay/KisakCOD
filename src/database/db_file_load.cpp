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
static void __cdecl Load_XAssetArrayCustom(int32_t count);

void __cdecl DB_CancelLoadXFile()
{
    if (g_load.compressBufferStart)
    {
        while (g_load.outstandingReads)
            DB_WaitXFileStage();
        DB_AuthLoad_InflateEnd(&g_load.stream);
        if (!g_load.f)
            MyAssertHandler(".\\database\\db_file_load.cpp", 165, 0, "%s", "g_load.f");
#ifdef __SWITCH__
        fclose(static_cast<FILE *>(g_load.f));
#else
        CloseHandle(g_load.f);
#endif
        g_load.f = nullptr;
    }
}

int32_t DB_WaitXFileStage()
{
    int32_t result; // eax

    if (!g_load.f)
        MyAssertHandler(".\\database\\db_file_load.cpp", 278, 0, "%s", "g_load.f");
#ifdef __SWITCH__
    // Switch fastfile reads are synchronous. DB_ReadData() already installs
    // the bytes in zlib's input buffer, so there is no outstanding read to wait for.
    return g_loadedSize;
#else
    if (g_load.outstandingReads <= 0)
        MyAssertHandler(".\\database\\db_file_load.cpp", 280, 0, "%s", "g_load.outstandingReads > 0");
    --g_load.outstandingReads;
    SleepEx(0xFFFFFFFF, 1);
    result = InterlockedIncrement(&g_loadedSize);
    g_load.stream.avail_in += 0x40000;
    return result;
#endif
}

void __cdecl DB_LoadedExternalData(int32_t size)
{
    InterlockedExchangeAdd(&g_loadedExternalBytes, size);
}

double __cdecl DB_GetLoadedFraction()
{
    double loadedBytesInternal; // [esp+14h] [ebp-20h]
    double totalBytesInternal; // [esp+1Ch] [ebp-18h]
    double loadedBytesExternal; // [esp+24h] [ebp-10h]
    double totalBytesExternal; // [esp+2Ch] [ebp-8h]

    if (!g_totalSize)
        return 0.0;
    totalBytesInternal = (double)g_totalSize * 262144.0;
    loadedBytesInternal = (double)g_loadedSize * 262144.0;
    if (loadedBytesInternal < 0.0)
        MyAssertHandler(".\\database\\db_file_load.cpp", 341, 0, "%s", "loadedBytesInternal >= 0");
    if (totalBytesInternal < loadedBytesInternal)
        loadedBytesInternal = totalBytesInternal;
    totalBytesExternal = (double)g_totalExternalBytes;
    loadedBytesExternal = (double)g_loadedExternalBytes;
    if (totalBytesExternal < loadedBytesExternal)
        loadedBytesExternal = totalBytesExternal;
    return (float)((loadedBytesInternal + loadedBytesExternal) / (totalBytesInternal + totalBytesExternal));
}

void __cdecl DB_LoadXFileData(uint8_t *pos, uint32_t size)
{
    const char *v2; // eax
    uint32_t err; // [esp+0h] [ebp-4h]

    iassert(size);
    iassert(g_load.f);
    iassert(!g_load.stream.avail_out);

    g_load.stream.next_out = pos;
    g_load.stream.avail_out = size;
    while (1)
    {
        if (!g_load.stream.avail_in)
            goto LABEL_19;
        err = DB_AuthLoad_Inflate(&g_load.stream, 2);
        if (err >= 2)
        {
            KISAK_NULLSUB();
            DB_CancelLoadXFile();
            Com_Error(ERR_DROP, "Fastfile for zone '%s' appears corrupt or unreadable (code %i.)", g_load.filename, err + 110);
        }
        if (g_load.f)
        {
            if ((uint32_t)(g_load.stream.next_in - g_load.compressBufferStart) > 0x80000)
                MyAssertHandler(
                    ".\\database\\db_file_load.cpp",
                    392,
                    0,
                    "%s",
                    "static_cast< unsigned >( g_load.stream.next_in - g_load.compressBufferStart ) <= FILE_BUFFER_SIZE * 2");
            if (g_load.stream.next_in == g_load.compressBufferEnd)
                g_load.stream.next_in = g_load.compressBufferStart;
        }
        if (!g_load.stream.avail_out)
            break;
        if (err)
        {
            v2 = va("Invalid fast file '%s' (%d != Z_OK)", g_load.filename, err);
            MyAssertHandler(".\\database\\db_file_load.cpp", 402, 0, "%s\n\t%s", "err == Z_OK", v2);
        }
    LABEL_19:
        DB_WaitXFileStage();
        DB_ReadXFileStage();
    }
}

void DB_ReadXFileStage()
{
    if (g_load.f)
    {
#ifdef __SWITCH__
        // Do not prefetch while zlib still owns the current input window.
        if (!g_load.stream.avail_in &&
            !DB_ReadData() &&
            !feof(static_cast<FILE *>(g_load.f)))
            Com_Error(ERR_DROP, "Read error of file '%s'", g_load.filename);
#else
        if (g_load.outstandingReads)
            MyAssertHandler(".\\database\\db_file_load.cpp", 254, 0, "%s", "!g_load.outstandingReads");
        if (!DB_ReadData() && GetLastError() != 38)
            Com_Error(ERR_DROP, "Read error of file '%s'", g_load.filename);
#endif
    }
}

int32_t __cdecl DB_ReadData()
{
    uint8_t *fileBuffer; // [esp+0h] [ebp-4h]

    if (!g_load.compressBufferStart)
        MyAssertHandler(".\\database\\db_file_load.cpp", 188, 0, "%s", "g_load.compressBufferStart");
    if (!g_load.f)
        MyAssertHandler(".\\database\\db_file_load.cpp", 189, 0, "%s", "g_load.f");
    if (g_load.interrupt)
        g_load.interrupt();
#ifdef __SWITCH__
    // Keep one synchronous 256-KB input window. The next read happens only
    // after zlib has consumed the current input buffer.
    if (g_load.stream.avail_in)
        return 1;

    fileBuffer = g_load.compressBufferStart;
    FILE *file = static_cast<FILE *>(g_load.f);
    if (std::fseek(file, static_cast<long>(g_load.switchFileOffset), SEEK_SET) != 0)
        return 0;

    g_load.switchLastRead = static_cast<uint32_t>(
        std::fread(fileBuffer, 1, 0x40000, file));
    g_load.switchFileOffset += g_load.switchLastRead;

    if (!g_load.switchLastRead)
        return 0;

    g_load.stream.next_in = fileBuffer;
    g_load.stream.avail_in = g_load.switchLastRead;
    ++g_loadedSize;
    return 1;
#else
    fileBuffer = &g_load.compressBufferStart[g_load.overlapped.Offset % 0x80000];
    Sys_WaitDatabaseThread();
    if (!ReadFileEx(g_load.f, fileBuffer, 0x40000u, &g_load.overlapped, (LPOVERLAPPED_COMPLETION_ROUTINE)DB_FileReadCompletion))
        return 0;
    ++g_load.outstandingReads;
    g_load.overlapped.Offset += 0x40000;
    return 1;
#endif
}

void __stdcall DB_FileReadCompletion(
    uint32_t dwErrorCode,
    uint32_t dwNumberOfBytesTransfered,
    _OVERLAPPED *lpOverlapped)
{
    ;
}

void __cdecl DB_LoadDelayedImages()
{
    uint32_t copyIter; // [esp+0h] [ebp-4h]

    DB_EnumXAssets(ASSET_TYPE_IMAGE, (void(__cdecl *)(XAssetHeader, void *))R_DelayLoadImage, 0, 0);
    for (copyIter = 0; copyIter < g_copyInfoCount; ++copyIter)
    {
        if (g_copyInfo[copyIter]->asset.type == ASSET_TYPE_IMAGE)
            R_DelayLoadImage(g_copyInfo[copyIter]->asset.header);
    }
}

void __cdecl DB_FinishGeometryBlocks(XZoneMemory *zoneMem)
{
    if (zoneMem->lockedVertexData)
    {
        R_FinishStaticVertexBuffer((IDirect3DVertexBuffer9*)zoneMem->vertexBuffer);
        zoneMem->lockedVertexData = 0;
    }
    if (zoneMem->lockedIndexData)
    {
        R_FinishStaticIndexBuffer((IDirect3DIndexBuffer9*)zoneMem->indexBuffer);
        zoneMem->lockedIndexData = 0;
    }
}

void __cdecl DB_LoadXFileInternal()
{
#ifdef __SWITCH__
    Switch_LogWrite("[SWITCH DBSTAGE] ENTER DB_LoadXFileInternal\n");
#endif

    int32_t err; // [esp+8h] [ebp-4Ch]
    bool fileIsSecure; // [esp+Fh] [ebp-45h]
    uint32_t version; // [esp+10h] [ebp-44h]
    XFile file; // [esp+14h] [ebp-40h] BYREF
    int32_t fileSize; // [esp+40h] [ebp-14h]
    const char *failureReason; // [esp+44h] [ebp-10h]
    char magic[8]; // [esp+48h] [ebp-Ch] BYREF

    iassert(g_load.f);
    DB_ReadXFileStage();
#ifdef __SWITCH__
    Switch_LogWrite("[SWITCH DBSTAGE] after ReadXFileStage#1\n");
    if (!g_load.stream.avail_in)
        Com_Error(ERR_DROP, "Fastfile for zone '%s' is empty.", g_load.filename);
#else
    if (!g_load.outstandingReads)
        Com_Error(ERR_DROP, "Fastfile for zone '%s' is empty.", g_load.filename);
    DB_WaitXFileStage();
    DB_ReadXFileStage();
#endif
    if (g_load.stream.avail_in < 8)
        MyAssertHandler(".\\database\\db_file_load.cpp", 598, 0, "%s", "sizeof( magic ) <= g_load.stream.avail_in");
    *(uint32_t *)magic = *(uint32_t *)g_load.stream.next_in;
    *(uint32_t *)&magic[4] = *((uint32_t *)g_load.stream.next_in + 1);
    g_load.stream.next_in += 8;
    g_load.stream.avail_in -= 8;
    if (memcmp(magic, "IWff0100", 8u) && memcmp(magic, "IWffu100", 8u))
    {
        KISAK_NULLSUB();
        Com_Error(ERR_DROP, "Fastfile for zone '%s' is corrupt or unreadable.", g_load.filename);
    }
    iassert(sizeof(version) <= g_load.stream.avail_in);
    version = *(uint32_t *)g_load.stream.next_in;
    g_load.stream.next_in += 4;
    g_load.stream.avail_in -= 4;
    if (version != 5)
    {
        if (version >= 5)
            Com_Error(
                ERR_DROP,
                "Fastfile for zone '%s' is newer than client executable (version %d, expecting %d)",
                g_load.filename,
                version,
                5);
        else
            Com_Error(
                ERR_DROP,
                "Fastfile for zone '%s' is out of date (version %d, expecting %d)",
                g_load.filename,
                version,
                5);
    }
    fileIsSecure = memcmp(magic, "IWffu100", 8u) != 0;
    err = DB_AuthLoad_InflateInit(&g_load.stream, fileIsSecure);
#ifdef __SWITCH__
    Switch_LogWrite("[SWITCH DBSTAGE] after InflateInit\n");
#endif
    failureReason = 0;
    if (fileIsSecure)
        failureReason = "authenticated file not supported";
    if (err)
        failureReason = "init failed";
    if (failureReason)
    {
        KISAK_NULLSUB();
        DB_CancelLoadXFile();
        Com_Error(ERR_DROP, "Fastfile for zone '%s' could not be loaded (%s)", g_load.filename, failureReason);
    }
    
    DB_LoadXFileData((uint8_t *)&file, sizeof(XFile));
    if (g_trackLoadProgress)
    {
#ifdef __SWITCH__
        FILE *switchFile = static_cast<FILE *>(g_load.f);
        long saved = std::ftell(switchFile);
        std::fseek(switchFile, 0, SEEK_END);
        fileSize = static_cast<int32_t>(std::ftell(switchFile));
        std::fseek(switchFile, saved, SEEK_SET);
#else
        fileSize = GetFileSize(g_load.f, 0);
#endif
        if (file.externalSize + fileSize >= 0x100000)
        {
            g_totalSize = (fileSize + 0x3FFFF) / 0x40000 - g_loadedSize;
            g_loadedSize = 0;
            g_totalExternalBytes = file.externalSize - g_loadedExternalBytes;
            g_loadedExternalBytes = 0;
        }
    }
#ifdef __SWITCH__
    Switch_LogWrite("[SWITCH DBSTAGE] before DB_AllocXZoneMemory\n");
#endif
    DB_AllocXZoneMemory(file.blockSize, g_load.filename, g_load.zoneMem, g_load.allocType);
#ifdef __SWITCH__
    Switch_LogWrite("[SWITCH DBSTAGE] after DB_AllocXZoneMemory\n");
#endif
    DB_InitStreams(g_load.zoneMem);
    Load_XAssetListCustom();
#ifdef __SWITCH__
    Switch_LogWrite("[SWITCH DBSTAGE] after Load_XAssetListCustom\n");
#endif
    DB_PushStreamPos(4);
    if (varXAssetList->assets)
    {
        varXAssetList->assets =
            reinterpret_cast<XAsset *>(DB_AllocStreamPos(3));
        varXAsset = varXAssetList->assets;
        if (varXAssetList->assetCount > 0)
            DB_IncStreamPos(static_cast<int32_t>(
                sizeof(XAsset) * static_cast<size_t>(varXAssetList->assetCount)));
        Load_XAssetArrayCustom(varXAssetList->assetCount);
#ifdef __SWITCH__
        Switch_LogWrite("[SWITCH DBSTAGE] after Load_XAssetArrayCustom\n");
#endif
    }
    DB_PopStreamPos();
    DB_FinishGeometryBlocks(g_load.zoneMem);
    --g_loadingAssets;
    Load_DelayStream();
    DB_LoadDelayedImages();
#ifdef __SWITCH__
    Switch_LogWrite("[SWITCH DBSTAGE] after DB_LoadDelayedImages\n");
#endif
    iassert(g_load.compressBufferStart);
    Com_Printf(CON_CHANNEL_FILES, "Loaded zone '%s'\n", g_load.filename);
#ifdef __SWITCH__
    Switch_LogWrite("[SWITCH DBSTAGE] COMPLETE zone\n");
#endif
#ifdef KISAK_MP
    if (!g_minimumFastFileLoaded)
        g_minimumFastFileLoaded = I_stricmp("localized_code_post_gfx_mp", g_load.filename) == 0;
#elif KISAK_SP
	g_anyFastFileLoaded = true;
#endif
    DB_CancelLoadXFile();
}

bool __cdecl DB_IsMinimumFastFileLoaded()
{
#ifdef KISAK_MP
    return g_minimumFastFileLoaded;
#elif KISAK_SP
	return g_anyFastFileLoaded;
#endif
}

void Load_XAssetListCustom()
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

        if (count)
            DB_LoadXFileData(
                reinterpret_cast<uint8_t *>(serializedStrings.data()),
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

#ifdef __SWITCH__
    char trace[256];
    std::snprintf(trace, sizeof(trace),
        "[SWITCH XASSETLIST] count=%d strings=%u assets=%08x\n",
        varXAssetList->assetCount,
        serialized.stringList.count,
        serialized.assets);
    Switch_LogWrite(trace);
#endif
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

    XAsset *var = varXAsset;
    for (int32_t i = 0; i < count; ++i)
    {
        SerializedXAsset serialized{};
        DB_LoadXFileData(
            reinterpret_cast<uint8_t *>(&serialized),
            sizeof(serialized));
        DB_IncStreamPos(sizeof(serialized));

        varXAsset = var;
        memset(varXAsset, 0, sizeof(*varXAsset));
        varXAsset->type = static_cast<XAssetType>(serialized.type);
        memcpy(&varXAsset->header, &serialized.header, sizeof(serialized.header));
        varXAssetHeader = &varXAsset->header;
        Load_XAssetHeader(0);

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


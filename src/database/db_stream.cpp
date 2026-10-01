#include <cstdio>
#include <universal/q_shared.h>
#include "database.h"

#ifdef __SWITCH__
extern void Switch_LogWrite(const char *msg);
extern int32_t g_switchCurrentAssetIndex;
#endif

uint32_t g_streamDelayIndex;
XBlock * g_streamBlocks;
uint8_t *g_streamPosArray[9];
StreamDelayInfo g_streamDelayArray[4096];
uint32_t g_streamPosIndex;
XZoneMemory *g_streamZoneMem;
uint8_t *g_streamPos;

StreamPosInfo g_streamPosStack[64];
uint32_t g_streamPosStackIndex;

#ifdef __SWITCH__
static uintptr_t g_switchStreamHighWater[9] = {};
static uint32_t g_switchStreamRegressionCount = 0;
static uint32_t g_switchStreamMismatchCount = 0;
uint32_t g_switchPointerInsertCount = 0;
uint32_t g_switchPointerInsertExtraBytes = 0;

static int32_t Switch_StreamOwner(
    const uint8_t *pos,
    uintptr_t *offsetOut)
{
    if (!pos || !g_streamBlocks)
        return -1;

    const uintptr_t ptr = reinterpret_cast<uintptr_t>(pos);
    for (uint32_t i = 0; i < ARRAY_COUNT(g_streamPosArray); ++i)
    {
        if (!g_streamBlocks[i].data)
            continue;

        const uintptr_t base =
            reinterpret_cast<uintptr_t>(g_streamBlocks[i].data);
        const uintptr_t end = base + g_streamBlocks[i].size;
        if (ptr >= base && ptr <= end)
        {
            if (offsetOut)
                *offsetOut = ptr - base;
            return static_cast<int32_t>(i);
        }
    }

    return -1;
}

static void Switch_CheckStreamCursor(const char *where)
{
    if (!g_streamPos)
        return;

    uintptr_t offset = 0;
    const int32_t owner = Switch_StreamOwner(g_streamPos, &offset);
    if (owner == static_cast<int32_t>(g_streamPosIndex))
        return;

    if (g_switchStreamMismatchCount >= 32)
        return;

    char trace[320];
    std::snprintf(
        trace,
        sizeof(trace),
        "[SWITCH STREAM MISMATCH] #%u where=%s current=%u owner=%d offset=%08x pos=%p stack=%u\n",
        g_switchStreamMismatchCount,
        where,
        static_cast<unsigned>(g_streamPosIndex),
        owner,
        static_cast<unsigned>(offset),
        static_cast<void *>(g_streamPos),
        static_cast<unsigned>(g_streamPosStackIndex));
    Switch_LogWrite(trace);
    ++g_switchStreamMismatchCount;
}

static void Switch_CheckStreamArrayEntry(
    uint32_t index,
    const char *where)
{
    if (index >= ARRAY_COUNT(g_streamPosArray) ||
        !g_streamPosArray[index])
        return;

    uintptr_t offset = 0;
    const int32_t owner =
        Switch_StreamOwner(g_streamPosArray[index], &offset);
    if (owner == static_cast<int32_t>(index))
        return;

    if (g_switchStreamMismatchCount >= 32)
        return;

    char trace[320];
    std::snprintf(
        trace,
        sizeof(trace),
        "[SWITCH STREAM ARRAY MISMATCH] #%u where=%s index=%u owner=%d offset=%08x pos=%p stack=%u current=%u\n",
        g_switchStreamMismatchCount,
        where,
        static_cast<unsigned>(index),
        owner,
        static_cast<unsigned>(offset),
        static_cast<void *>(g_streamPosArray[index]),
        static_cast<unsigned>(g_streamPosStackIndex),
        static_cast<unsigned>(g_streamPosIndex));
    Switch_LogWrite(trace);
    ++g_switchStreamMismatchCount;
}

static void Switch_CheckStreamRegression(
    uint32_t index,
    uint8_t *pos,
    const char *where)
{
    if (index >= ARRAY_COUNT(g_streamPosArray) ||
        !g_streamBlocks ||
        !g_streamBlocks[index].data ||
        !pos)
        return;

    const uintptr_t base =
        reinterpret_cast<uintptr_t>(g_streamBlocks[index].data);
    const uintptr_t ptr = reinterpret_cast<uintptr_t>(pos);
    if (ptr < base ||
        ptr > base + g_streamBlocks[index].size)
        return;

    const uintptr_t offset = ptr - base;
    if (offset < g_switchStreamHighWater[index] &&
        g_switchStreamRegressionCount < 32)
    {
        char trace[256];
        std::snprintf(
            trace,
            sizeof(trace),
            "[SWITCH STREAM REGRESS] #%u where=%s stream=%u old=%08x new=%08x stack=%u current=%u\n",
            g_switchStreamRegressionCount,
            where,
            index,
            static_cast<unsigned>(g_switchStreamHighWater[index]),
            static_cast<unsigned>(offset),
            static_cast<unsigned>(g_streamPosStackIndex),
            static_cast<unsigned>(g_streamPosIndex));
        Switch_LogWrite(trace);
        ++g_switchStreamRegressionCount;
    }

    if (offset > g_switchStreamHighWater[index])
        g_switchStreamHighWater[index] = offset;
}
#endif

// --- file-local forward declarations (moved out of database.h) ---
static void __cdecl DB_SetStreamIndex(uint32_t index);

void __cdecl DB_InitStreams(XZoneMemory *zoneMem)
{
    int32_t i; // [esp+0h] [ebp-4h]

    g_streamZoneMem = zoneMem;
    g_streamBlocks = zoneMem->blocks;
    g_streamPos = zoneMem->blocks[0].data;
    g_streamPosIndex = 0;
    g_streamDelayIndex = 0;
    g_streamPosStackIndex = 0;
#ifdef __SWITCH__
    std::memset(g_switchStreamHighWater, 0, sizeof(g_switchStreamHighWater));
    g_switchStreamRegressionCount = 0;
    g_switchPointerInsertCount = 0;
    g_switchPointerInsertExtraBytes = 0;
#endif
    for (i = 0; i < 9; ++i)
        g_streamPosArray[i] = zoneMem->blocks[i].data;
#ifdef __SWITCH__
    Switch_CheckStreamCursor("DB_InitStreams:initial");
    for (i = 0; i < 9; ++i)
        Switch_CheckStreamArrayEntry(i, "DB_InitStreams:array");
#endif
}

void __cdecl DB_PushStreamPos(uint32_t index)
{
    iassert(index < ARRAY_COUNT(g_streamPosArray));
    iassert(g_streamPosIndex < ARRAY_COUNT(g_streamPosArray));
    iassert(g_streamPosStackIndex < ARRAY_COUNT(g_streamPosStack));

    g_streamPosStack[g_streamPosStackIndex].index = g_streamPosIndex;
    DB_SetStreamIndex(index);

    g_streamPosStack[g_streamPosStackIndex++].pos = g_streamPos;
#ifdef __SWITCH__
    Switch_CheckStreamCursor("DB_PushStreamPos:after");
#endif
}

void __cdecl DB_CloneStreamData(uint8_t *destStart)
{
    if (destStart)
        memcpy(
            &destStart[g_streamPosArray[g_streamPosIndex] - g_streamZoneMem->blocks[g_streamPosIndex].data],
            g_streamPosArray[g_streamPosIndex],
            g_streamPos - g_streamPosArray[g_streamPosIndex]);
}

void __cdecl DB_SetStreamIndex(uint32_t index)
{
    if (index != g_streamPosIndex)
    {
#ifdef __SWITCH__
        Switch_CheckStreamRegression(
            g_streamPosIndex,
            g_streamPos,
            "DB_SetStreamIndex:save");
#endif
        if (g_streamPosIndex == 7)
        {
            DB_CloneStreamData(g_streamZoneMem->lockedVertexData);
        }
        else if (g_streamPosIndex == 8)
        {
            DB_CloneStreamData(g_streamZoneMem->lockedIndexData);
        }
        iassert(index < arr_cnt(g_streamPosArray));
        g_streamPosArray[g_streamPosIndex] = g_streamPos;
#ifdef __SWITCH__
        Switch_CheckStreamArrayEntry(g_streamPosIndex, "DB_SetStreamIndex:saved");
        Switch_CheckStreamArrayEntry(index, "DB_SetStreamIndex:target");
#endif
        g_streamPosIndex = index;
        g_streamPos = g_streamPosArray[index];
#ifdef __SWITCH__
        Switch_CheckStreamCursor("DB_SetStreamIndex:after");
#endif
    }
}

void __cdecl DB_PopStreamPos()
{
    vassert(g_streamPosStackIndex > 0, "(g_streamPosStackIndex = %d)", g_streamPosStackIndex);

    --g_streamPosStackIndex;

    if (!g_streamPosIndex)
        g_streamPos = g_streamPosStack[g_streamPosStackIndex].pos;
    DB_SetStreamIndex(g_streamPosStack[g_streamPosStackIndex].index);
}

uint8_t *__cdecl DB_GetStreamPos()
{
    return g_streamPos;
}

uint8_t *__cdecl DB_AllocStreamPos(int32_t alignment)
{
    iassert(g_streamPos);
    g_streamPos = reinterpret_cast<uint8_t *>(reinterpret_cast<uintptr_t>(&g_streamPos[alignment]) & ~static_cast<uintptr_t>(alignment));
    return g_streamPos;
}

void __cdecl DB_IncStreamPos(int32_t size)
{
    iassert(g_streamPos);
    iassert(g_streamPos + size <= g_streamZoneMem->blocks[g_streamPosIndex].data + g_streamZoneMem->blocks[g_streamPosIndex].size);

    g_streamPos += size;
#ifdef __SWITCH__
    Switch_CheckStreamCursor("DB_IncStreamPos:after");
#endif
}

const void **__cdecl DB_InsertPointer()
{
    const void **pData; // [esp+0h] [ebp-4h]

#ifdef __SWITCH__
    const uint32_t traceIndex = g_switchCurrentAssetIndex < 0
        ? UINT32_MAX
        : static_cast<uint32_t>(g_switchCurrentAssetIndex);
    const uint32_t beforeCount = g_switchPointerInsertCount;
    const uintptr_t beforePos =
        reinterpret_cast<uintptr_t>(g_streamPos);
#endif

    DB_PushStreamPos(4);
#ifdef __SWITCH__
    pData = reinterpret_cast<const void **>(DB_AllocStreamPos(7));
    DB_IncStreamPos(static_cast<int32_t>(sizeof(void *)));
    ++g_switchPointerInsertCount;
    g_switchPointerInsertExtraBytes +=
        static_cast<uint32_t>(sizeof(void *) - 4);

    if (traceIndex >= 1190u && traceIndex <= 1210u)
    {
        char trace[256];
        std::snprintf(
            trace,
            sizeof(trace),
            "[SWITCH PTR INSERT] asset=%u #%u extra=%u before=%p after=%p slot=%p\n",
            traceIndex,
            beforeCount,
            g_switchPointerInsertExtraBytes,
            reinterpret_cast<void *>(beforePos),
            reinterpret_cast<void *>(g_streamPos),
            static_cast<void *>(pData));
        Switch_LogWrite(trace);
    }
#else
    pData = (const void **)DB_AllocStreamPos(3);
    DB_IncStreamPos(4);
#endif
    DB_PopStreamPos();
    return pData;
}


#include <cstring>
#include <universal/q_shared.h>
#include "database.h"

#ifdef __SWITCH__
extern void Switch_LogWrite(const char *msg);
extern int32_t g_switchCurrentAssetIndex;
extern uint32_t g_switchCurrentAssetRawType;
extern uint32_t g_switchCurrentAssetHeader;
#endif




void __cdecl Load_Stream(bool atStreamStart, uint8_t *ptr, int32_t size)
{
    iassert(atStreamStart == (ptr == DB_GetStreamPos()));
    if (atStreamStart && size)
    {
        if (g_streamPosIndex - 1 < 3)
        {
            if (g_streamPosIndex == 1)
            {
                memset(ptr, 0, size);
            }
            else
            {
                bcassert(g_streamDelayIndex, ARRAY_COUNT(g_streamDelayArray));
                g_streamDelayArray[g_streamDelayIndex].ptr = ptr;
                g_streamDelayArray[g_streamDelayIndex++].size = size;
            }
        }
        else
        {
            DB_LoadXFileData(ptr, size);
        }
        DB_IncStreamPos(size);
    }
}

void __cdecl Load_DelayStream()
{
    uint32_t index; // [esp+4h] [ebp-8h]

    for (index = 0; index < g_streamDelayIndex; ++index)
        DB_LoadXFileData((unsigned char*)g_streamDelayArray[index].ptr, g_streamDelayArray[index].size);
}

uintptr_t __cdecl DB_ConvertOffsetToPointerValue(uint32_t offset)
{
    iassert(offset && offset != UINT32_MAX && offset != UINT32_MAX - 1);

    const uint32_t block = (offset - 1) >> 28;
    const uint32_t blockOffset = (offset - 1) & 0x0FFFFFFF;

#ifdef __SWITCH__
    if (block >= ARRAY_COUNT(g_streamPosArray))
    {
        char trace[256];
        std::snprintf(
            trace,
            sizeof(trace),
            "[SWITCH OFFSET INVALID] token=%08x block=%u offset=%08x size=0 assetIdx=%d rawType=%u rawHeader=%08x stream=%u pos=%p\n",
            offset,
            block,
            blockOffset,
            g_switchCurrentAssetIndex,
            g_switchCurrentAssetRawType,
            g_switchCurrentAssetHeader,
            g_streamPosIndex,
            static_cast<void *>(DB_GetStreamPos()));
        Switch_LogWrite(trace);
        return 0;
    }

    if (!g_streamBlocks[block].data ||
        blockOffset >= g_streamBlocks[block].size)
    {
        char trace[256];
        std::snprintf(
            trace,
            sizeof(trace),
            "[SWITCH OFFSET INVALID] token=%08x block=%u offset=%08x size=%u assetIdx=%d rawType=%u rawHeader=%08x stream=%u pos=%p\n",
            offset,
            block,
            blockOffset,
            g_streamBlocks[block].size,
            g_switchCurrentAssetIndex,
            g_switchCurrentAssetRawType,
            g_switchCurrentAssetHeader,
            g_streamPosIndex,
            static_cast<void *>(DB_GetStreamPos()));
        Switch_LogWrite(trace);
        return 0;
    }
#endif

    return reinterpret_cast<uintptr_t>(
        &g_streamBlocks[block].data[blockOffset]);
}

void __cdecl DB_ConvertOffsetToAlias(void *data)
{
    const uint32_t offset = *reinterpret_cast<const uint32_t *>(data);
    iassert(offset && offset != UINT32_MAX && offset != UINT32_MAX - 1);

    const uintptr_t aliasSlot = DB_ConvertOffsetToPointerValue(offset);
    if (!aliasSlot)
        return;
#ifdef __SWITCH__
    const uint32_t serializedAliasValue =
        *reinterpret_cast<const uint32_t *>(aliasSlot);
    *reinterpret_cast<uintptr_t *>(data) =
        static_cast<uintptr_t>(serializedAliasValue);
#else
    const uint32_t aliasValue =
        *reinterpret_cast<const uint32_t *>(aliasSlot);
    *reinterpret_cast<uint32_t *>(data) = aliasValue;
#endif
}

void __cdecl DB_ConvertOffsetToPointer(void *data)
{
    const uint32_t offset = *reinterpret_cast<const uint32_t *>(data);
#ifdef __SWITCH__
    *reinterpret_cast<uintptr_t *>(data) = DB_ConvertOffsetToPointerValue(offset);
#else
    *reinterpret_cast<uint32_t *>(data) =
        static_cast<uint32_t>(DB_ConvertOffsetToPointerValue(offset));
#endif
}



void __cdecl DB_LoadSwitchSerialized(void *dst, uint32_t size)
{
#ifdef __SWITCH__
    iassert(dst);
    iassert(size);
    uint8_t *streamPos = DB_GetStreamPos();
    if (g_switchCurrentAssetIndex == 1363)
    {
        char trace[320];
        std::snprintf(
            trace,
            sizeof(trace),
            "[SWITCH XASSET TRACE] IMAGE1363 serialized copy dst=%p size=%u stream=%u pos=%p ra=%p\n",
            dst,
            static_cast<unsigned>(size),
            static_cast<unsigned>(g_streamPosIndex),
            static_cast<void *>(streamPos),
            __builtin_return_address(0));
        Switch_LogWrite(trace);
    }
    DB_LoadXFileData(streamPos, size);
    std::memcpy(dst, streamPos, size);
    DB_IncStreamPos(static_cast<int32_t>(size));
#else
    (void)dst;
    (void)size;
#endif
}

void __cdecl Load_XStringCustom(char **str)
{
    uint8_t *pos; // [esp+0h] [ebp-8h]
    char *s; // [esp+4h] [ebp-4h]

    s = *str;
    for (pos = (uint8_t *)*str; ; ++pos)
    {
        DB_LoadXFileData(pos, 1u);
        if (!*pos)
            break;
    }
    DB_IncStreamPos(pos - (uint8_t *)s + 1);
}

void __cdecl Load_TempStringCustom(char **str)
{
    const char * string; // [esp+0h] [ebp-4h]

    Load_XStringCustom(str);
    if (*str)
        string = reinterpret_cast<const char *>(static_cast<uintptr_t>(SL_GetString(*str, 4u))); // KISAKTODO: this seems way wrong but it's what the decomp is showing
    else
        string= 0;
    *str = (char *)string;
}


#include <universal/q_shared.h>
#include "database.h"


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

void __cdecl DB_ConvertOffsetToAlias(uint32_t *data)
{
    const uint32_t offset = *data;
    iassert(offset && offset != UINT32_MAX && offset != UINT32_MAX - 1);
    const uint32_t block = (offset - 1) >> 28;
    const uint32_t blockOffset = (offset - 1) & 0xFFFFFFF;
    const uint32_t alias32 = *reinterpret_cast<const uint32_t *>(
        &g_streamZoneMem->blocks[block].data[blockOffset]);
#ifdef __SWITCH__
    *reinterpret_cast<uintptr_t *>(data) = static_cast<uintptr_t>(alias32);
#else
    *data = alias32;
#endif
}

void __cdecl DB_ConvertOffsetToPointer(uint32_t *data)
{
    const uint32_t offset = *data;
    iassert(offset && offset != UINT32_MAX && offset != UINT32_MAX - 1);
    const uint32_t block = (offset - 1) >> 28;
    const uint32_t blockOffset = (offset - 1) & 0xFFFFFFF;
    const uintptr_t ptr = reinterpret_cast<uintptr_t>(
        &g_streamZoneMem->blocks[block].data[blockOffset]);
#ifdef __SWITCH__
    *reinterpret_cast<uintptr_t *>(data) = ptr;
#else
    *data = static_cast<uint32_t>(ptr);
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
        string = (const char*)SL_GetString(*str, 4u); // KISAKTODO: this seems way wrong but it's what the decomp is showing
    else
        string= 0;
    *str = (char *)string;
}


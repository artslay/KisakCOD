#ifdef __SWITCH__
#include <cstring>
#include <cstdint>
#include <win32/win_storage.h>

playerStatNetworkData statData = {};

int __cdecl LiveStorage_GetStat(int, int index)
{
    if (index < 0 || index >= 3498 || !statData.statsFetched)
        return 0;

    if (index < 2000)
        return statData.playerStats[index + 4];

    const int offset = 4 * index - 5996;
    if (offset < 0 || offset + 4 > (int)sizeof(statData.playerStats))
        return 0;

    uint32_t value = 0;
    std::memcpy(&value, &statData.playerStats[offset], sizeof(value));
    return (int)value;
}

void __cdecl LiveStorage_SetStat(int, int index, uint32_t value)
{
    if (index < 0 || index >= 3498)
        return;

    if (!statData.statsFetched)
        statData.statsFetched = true;

    if (index < 2000)
    {
        statData.playerStats[index + 4] = (unsigned char)value;
        statData.statWriteNeeded = true;
        return;
    }

    const int offset = 4 * index - 5996;
    if (offset < 0 || offset + 4 > (int)sizeof(statData.playerStats))
        return;

    std::memcpy(&statData.playerStats[offset], &value, sizeof(value));
    statData.statWriteNeeded = true;
}

void __cdecl LiveStorage_NewUser()
{
    std::memset(&statData, 0, sizeof(statData));
    statData.statsFetched = true;
}

#endif

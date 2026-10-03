#include <universal/q_shared.h>
#include "database.h"

void __cdecl Load_ScriptStringCustom(uint16_t *var)
{
    const uint16_t index = *var;
    if (!index || index >= static_cast<uint16_t>(varXAssetList->stringList.count))
    {
        *var = 0;
        return;
    }

    const char *string = varXAssetList->stringList.strings[index];
    *var = string ? SL_GetString(string, 4u) : 0;
}

void __cdecl Mark_ScriptStringCustom(uint16_t *var)
{
    if (*var)
        SL_AddUser(*var, 4u);
}


#ifdef __SWITCH__
#include "profile.h"

ProfileScript profileScript = {};
int g_profileStack[256] = {};
int prof_parity[2] = {};
ProfileStack g_prof_stack[7] = {};
const dvar_t *profile = nullptr;
const dvar_t *profile_thread = nullptr;
const dvar_t *profile_rowcount = nullptr;

void Profile_Init() {}
void __cdecl Profile_Guard(int) {}
void __cdecl Profile_Unguard(int) {}
void __cdecl Profile_SetTotal(int, int) {}
void Profile_ResetScriptCounters() {}
void __cdecl Profile_ResetCounters(int) {}
void __cdecl Profile_Recover(int) {}
void __cdecl Profile_InitContext(int) {}
ProfileStack *__cdecl Profile_GetStackForContext(int context) { return context >= 0 && context < 7 ? &g_prof_stack[context] : nullptr; }
ProfileScript *__cdecl Profile_GetScript() { return &profileScript; }
int __cdecl Profile_GetEnumParity(uint32_t) { return 0; }
int __cdecl Profile_GetDisplayThread() { return 0; }
void __cdecl Profile_EndScripts(uint32_t) {}
void __cdecl Profile_EndScript(int) {}
void __cdecl Profile_BeginScripts(uint32_t) {}
void __cdecl Profile_BeginScript(int) {}
int __cdecl Profile_AddScriptName(const char *) { return 0; }
void __cdecl Profile_ResetCountersForContext(int, int) {}
const char *__cdecl Profile_MissingEnd() { return ""; }
void __cdecl Profile_Begin(int) {}
int __cdecl Profile_EndInternal(long double *) { return 0; }
#endif

#ifdef __SWITCH__
#include <switch.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <cstdarg>
#include <qcommon/qcommon.h>
#include <qcommon/threads.h>

static const auto g_sysStart = std::chrono::steady_clock::now();
static std::mutex g_sysCritical[32];

uint32_t __cdecl Sys_Milliseconds()
{
    return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - g_sysStart).count();
}

void __cdecl Sys_LockWrite(FastCriticalSection *critSect)
{
    if (!critSect) return;
    while (__atomic_exchange_n(&critSect->writeCount, 1u, __ATOMIC_ACQUIRE)) std::this_thread::yield();
}

void __cdecl Sys_UnlockWrite(FastCriticalSection *critSect)
{
    if (critSect) __atomic_store_n(&critSect->writeCount, 0u, __ATOMIC_RELEASE);
}

void __cdecl Sys_EnterCriticalSection(int section)
{
    if (section >= 0 && section < 32) g_sysCritical[section].lock();
}

void __cdecl Sys_LeaveCriticalSection(int section)
{
    if (section >= 0 && section < 32) g_sysCritical[section].unlock();
}

bool __cdecl Sys_IsRemoteDebugClient() { return false; }

void __cdecl Sys_Print(const char *msg)
{
    if (msg) std::fputs(msg, stdout);
}

sysEvent_t *__cdecl Sys_GetEvent(sysEvent_t *result)
{
    static sysEvent_t ev = {};
    ev.evTime = Sys_Milliseconds();
    ev.evType = SE_NONE;
    ev.evValue = 0;
    ev.evValue2 = 0;
    ev.evPtrLength = 0;
    ev.evPtr = nullptr;
    if (result) *result = ev;
    return result;
}

void __cdecl Sys_Error(const char *error, ...)
{
    char message[4096];
    va_list ap;
    va_start(ap, error);
    std::vsnprintf(message, sizeof(message), error, ap);
    va_end(ap);
    std::printf("FATAL: %s\n", message);
    std::fflush(stdout);
    appletRequestExit();
    std::abort();
}

void __cdecl Sys_Quit()
{
    appletRequestExit();
    std::exit(0);
}

void __cdecl Sys_Init()
{
    s_cpuCount = std::thread::hardware_concurrency();
    if (!s_cpuCount) s_cpuCount = 1;
    s_cpuCount = s_cpuCount > 4 ? 4 : s_cpuCount;
    Com_Printf(CON_CHANNEL_SYSTEM, "Switch CPU threads: %u\n", s_cpuCount);
}

void __cdecl Sys_LoadingKeepAlive() {}
void __cdecl Sys_DestroySplashWindow() {}
void __cdecl Sys_NormalExit() {}
void __cdecl Sys_OpenURL(const char *, int) {}
void __cdecl Sys_OutOfMemErrorInternal(const char *filename, int line)
{
    Sys_Error("Out of memory: %s:%d", filename, line);
}
void __cdecl Sys_NoFreeFilesError() { Sys_Error("Filesystem is full"); }

#endif

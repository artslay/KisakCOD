#ifdef __SWITCH__
#include <switch.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <string>
#include <cstring>
#include <cstdarg>
#include <qcommon/qcommon.h>
#include <qcommon/threads.h>
#include <win32/win_local.h>

static const auto g_sysStart = std::chrono::steady_clock::now();
static std::recursive_mutex g_sysCritical[32];

static FILE *g_switchLogFile = nullptr;
static const char *const kSwitchLogPath = "sdmc:/switch/KisakCOD/kisakcod.log";
static bool g_switchScreenLog = false;

void Switch_LogInit()
{
    consoleInit(nullptr);
    g_switchScreenLog = true;

    if (g_switchLogFile)
        return;

    g_switchLogFile = std::fopen(kSwitchLogPath, "wb");
    if (!g_switchLogFile)
    {
        std::printf("[KisakCOD][LOG] Failed to open %s\n", kSwitchLogPath);
        std::fflush(stdout);
        return;
    }

    std::setvbuf(g_switchLogFile, nullptr, _IOLBF, BUFSIZ);
    std::fprintf(g_switchLogFile, "========================================\n");
    std::fprintf(g_switchLogFile, "KisakCOD Switch engine log\n");
    std::fprintf(g_switchLogFile, "Log file: %s\n", kSwitchLogPath);
    std::fprintf(g_switchLogFile, "========================================\n");
    std::fflush(g_switchLogFile);
    consoleUpdate(nullptr);
}

void Switch_LogRaw(const char *msg)
{
    if (!msg)
        return;

    std::fputs(msg, stdout);
    std::fflush(stdout);

    if (g_switchLogFile)
    {
        std::fputs(msg, g_switchLogFile);
        std::fflush(g_switchLogFile);
    }
}

void Switch_LogShutdown()
{
    if (!g_switchLogFile)
        return;

    std::fflush(g_switchLogFile);
    std::fclose(g_switchLogFile);
    g_switchLogFile = nullptr;

    if (g_switchScreenLog)
    {
        consoleUpdate(nullptr);
        consoleExit(nullptr);
        g_switchScreenLog = false;
    }
}

SysInfo sys_info = {};

int g_debugClient = 0;
unsigned char g_debugPacket[1][8192] = {};

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

int __cdecl Sys_IsRemoteDebugClient() { return 0; }

char *__cdecl Sys_GetClipboardData()
{
    return nullptr;
}

int __cdecl Sys_SetClipboardData(const char *text)
{
    (void)text;
    return 0;
}

void __cdecl Sys_Print(const char *msg)
{
    if (!msg)
        return;

    std::fputs(msg, stdout);
    std::fflush(stdout);

    if (g_switchLogFile)
    {
        std::fputs(msg, g_switchLogFile);
        std::fflush(g_switchLogFile);
    }

    if (g_switchScreenLog && Sys_IsMainThread())
        consoleUpdate(nullptr);
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
    char fatalLine[4096];
    std::snprintf(fatalLine, sizeof(fatalLine), "FATAL: %s\n", message);
    Sys_Print(fatalLine);
    appletRequestExitToSelf();
    std::abort();
}

void __cdecl Sys_Quit()
{
    appletRequestExitToSelf();
    std::exit(0);
}

void __cdecl Sys_OutOfMemErrorInternal(const char *filename, int line)
{
    Sys_Error("Out of memory: %s:%d", filename ? filename : "?", line);
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
void NET_RestartDebug() {}

void __cdecl NET_ShutdownDebug()
{
    g_debugClient = 0;
}

void NET_InitDebug()
{
    g_debugClient = 0;
}

void __cdecl Sys_Listen_f()
{
}

void Sys_DebugSocketError(const char *message)
{
    if (message)
        Com_Printf(CON_CHANNEL_SYSTEM, "%s\n", message);
}

int __cdecl Sys_ReadDebugSocketInt()
{
    return 0;
}

void __cdecl Sys_WriteDebugSocketInt(int)
{
}

void __cdecl Sys_WriteDebugSocketString(char *)
{
}

int __cdecl Sys_ReadDebugSocketMessageType(unsigned char *type, int)
{
    if (type)
        *type = 0;
    return 0;
}

int __cdecl Sys_UpdateDebugSocket()
{
    return 0;
}

int __cdecl Sys_ReadDebugSocketData(char *buffer, int len, int)
{
    if (buffer && len > 0)
        std::memset(buffer, 0, static_cast<size_t>(len));
    return 0;
}

void __cdecl Sys_ReadDebugSocketStringBuffer(char *buffer, int len)
{
    if (buffer && len > 0)
        buffer[0] = '\0';
}

void __cdecl Sys_FlushDebugSocketData()
{
}

void __cdecl Sys_AckDebugSocket()
{
}

char *__cdecl Sys_ReadDebugSocketString()
{
    static char empty[] = "";
    return empty;
}

void __cdecl Sys_WriteDebugSocketData(unsigned char *, int)
{
}

void __cdecl Sys_WriteDebugSocketMessageType(unsigned char)
{
}

void __cdecl Sys_EndWriteDebugSocket()
{
}


void __cdecl Sys_NoFreeFilesError() { Sys_Error("Filesystem is full"); }


uint32_t __cdecl Sys_MillisecondsRaw()
{
    return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

void __cdecl Sys_SnapVector(float *v)
{
    if (!v)
        return;

    v[0] = SnapFloat(v[0]);
    v[1] = SnapFloat(v[1]);
    v[2] = SnapFloat(v[2]);
}

void __cdecl NET_Sleep(int msec)
{
    if (msec <= 0)
    {
        std::this_thread::yield();
        return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(msec));
}

char *__cdecl Sys_DefaultInstallPath()
{
    static char installPath[] = "sdmc:/switch/KisakCOD/game";
    return installPath;
}

BOOL __cdecl Sys_RemoveDirTree(const char *path)
{
    if (!path || !*path)
        return 0;

    struct stat st {};
    if (stat(path, &st) != 0)
        return 0;

    if (!S_ISDIR(st.st_mode))
        return std::remove(path) == 0 ? 1 : 0;

    DIR *dir = opendir(path);
    if (!dir)
        return 0;

    bool ok = true;
    while (dirent *entry = readdir(dir))
    {
        if (!entry)
            continue;

        const char *name = entry->d_name;
        if (!std::strcmp(name, ".") || !std::strcmp(name, ".."))
            continue;

        std::string child(path);
        if (!child.empty() && child.back() != '/')
            child.push_back('/');
        child += name;

        struct stat childStat {};
        if (stat(child.c_str(), &childStat) != 0)
        {
            ok = false;
            break;
        }

        if (S_ISDIR(childStat.st_mode))
        {
            if (!Sys_RemoveDirTree(child.c_str()))
            {
                ok = false;
                break;
            }
        }
        else if (std::remove(child.c_str()) != 0)
        {
            ok = false;
            break;
        }
    }

    closedir(dir);

    if (!ok)
        return 0;

    return rmdir(path) == 0 ? 1 : 0;
}

bool __cdecl IN_IsForegroundWindow()
{
    return true;
}

void __cdecl IN_SetForegroundWindow()
{
}

void __cdecl IN_ActivateMouse(int)
{
}

void __cdecl IN_Frame()
{
}

void __cdecl IN_Activate(qboolean active)
{
    (void)active;
}

#endif

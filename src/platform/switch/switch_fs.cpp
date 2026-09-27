#ifdef __SWITCH__
#include <switch.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <vector>
#include <string>

#include <universal/q_shared.h>
#include <universal/com_files.h>
#include <universal/com_memory.h>
#include <qcommon/com_fileaccess.h>
#include <qcommon/qcommon.h>

const dvar_t *fs_remotePCDirectory = nullptr;
const dvar_t *fs_remotePCName = nullptr;
const dvar_t *fs_homepath = nullptr;
const dvar_s *fs_debug = nullptr;
const dvar_s *fs_restrict = nullptr;
const dvar_s *fs_ignoreLocalized = nullptr;
const dvar_s *fs_basepath = nullptr;
const dvar_s *fs_copyfiles = nullptr;
const dvar_s *fs_cdpath = nullptr;
const dvar_s *fs_gameDirVar = nullptr;
const dvar_s *fs_basegame = nullptr;

int fs_fakeChkSum = 0;
int fs_numServerIwds = 0;
int fs_serverIwds[1024] = {};
int com_fileAccessed = 0;
void *g_writeLogEvent = nullptr;
int marker_com_files = 0;
void *g_writeLogCompleteEvent = nullptr;
int fs_loadStack = 0;
const char *fs_serverIwdNames[1024] = {};
int fs_checksumFeed = 0;
char fs_gamedir[256] = "main";
searchpath_s *fs_searchpaths = nullptr;

static fileHandleData_t g_fsh[65] = {};
static const char *const kSwitchRoot = "sdmc:/switch/KisakCOD/game";

static void SwitchPath(char *dst, size_t dstSize, const char *base, const char *game, const char *qpath)
{
    if (!base || !*base) base = kSwitchRoot;
    if (!game || !*game) game = fs_gamedir;
    while (*qpath == '/' || *qpath == '\') ++qpath;
    std::snprintf(dst, dstSize, "%s/%s/%s", base, game, qpath);
    for (char *p = dst; *p; ++p) if (*p == '\') *p = '/';
}

static int AllocHandle()
{
    for (int i = 1; i < 65; ++i)
        if (!g_fsh[i].handleFiles.file.o)
            return i;
    return 0;
}

FILE *FS_SwitchOpenFile(const char *path)
{
    char resolved[256];
    SwitchPath(resolved, sizeof(resolved), fs_basepath ? fs_basepath->current.string : kSwitchRoot, fs_gamedir, path);
    return FS_FileOpenReadBinary(resolved);
}

bool __cdecl FS_Initialized() { return fs_searchpaths != nullptr; }

void __cdecl FS_CheckFileSystemStarted()
{
    if (!FS_Initialized())
        Com_Error(ERR_FATAL, "Switch filesystem is not initialized");
}

void __cdecl FS_RegisterDvars()
{
    fs_debug = Dvar_RegisterInt("fs_debug", 0, 0, 2, DVAR_NOFLAG, "Filesystem debug");
    fs_copyfiles = Dvar_RegisterBool("fs_copyfiles", 0, DVAR_INIT, "Copy files");
    fs_cdpath = Dvar_RegisterString("fs_cdpath", (char*)kSwitchRoot, DVAR_INIT, "Switch game root");
    fs_basepath = Dvar_RegisterString("fs_basepath", (char*)kSwitchRoot, DVAR_INIT | DVAR_AUTOEXEC, "Switch game root");
    fs_homepath = Dvar_RegisterString("fs_homepath", (char*)kSwitchRoot, DVAR_INIT | DVAR_AUTOEXEC, "Switch game root");
    fs_basegame = Dvar_RegisterString("fs_basegame", (char*)"", DVAR_INIT, "Base game");
    fs_gameDirVar = Dvar_RegisterString("fs_game", (char*)"", DVAR_SERVERINFO | DVAR_SYSTEMINFO | DVAR_INIT, "Game directory");
    fs_ignoreLocalized = Dvar_RegisterBool("fs_ignoreLocalized", 0, DVAR_LATCH | DVAR_CHEAT, "Ignore localized files");
    fs_restrict = Dvar_RegisterBool("fs_restrict", 0, DVAR_INIT, "Restricted mode");
}

void __cdecl FS_AddSearchPath(searchpath_s *search)
{
    search->next = fs_searchpaths;
    fs_searchpaths = search;
}

void __cdecl FS_AddGameDirectory(char *path, char *dir, int localized, int language)
{
    (void)language;
    searchpath_s *s = (searchpath_s*)Z_Malloc(sizeof(searchpath_s), "FS_AddGameDirectory", 3);
    s->dir = (directory_t*)Z_Malloc(sizeof(directory_t), "FS_AddGameDirectory", 3);
    s->iwd = nullptr;
    s->bLocalized = localized;
    s->ignore = 0;
    s->ignorePureCheck = 0;
    s->language = language;
    I_strncpyz(s->dir->path, path, sizeof(s->dir->path));
    I_strncpyz(s->dir->gamedir, dir, sizeof(s->dir->gamedir));
    FS_AddSearchPath(s);
    if (!localized)
        I_strncpyz(fs_gamedir, dir, sizeof(fs_gamedir));
}

void __cdecl FS_AddLocalizedGameDirectory(char *path, char *dir)
{
    FS_AddGameDirectory(path, dir, 0, 0);
}

void __cdecl FS_Startup(char *gameName)
{
    Com_Printf(CON_CHANNEL_FILES, "----- Switch FS_Startup -----\n");
    FS_RegisterDvars();
    FS_AddLocalizedGameDirectory((char*)kSwitchRoot, gameName);
    if (fs_basegame && fs_basegame->current.string[0])
        FS_AddLocalizedGameDirectory((char*)kSwitchRoot, (char*)fs_basegame->current.string);
    Com_Printf(CON_CHANNEL_FILES, "Switch game root: %s\n", kSwitchRoot);
    Com_Printf(CON_CHANNEL_FILES, "Game directory: %s\n", fs_gamedir);
    Com_Printf(CON_CHANNEL_FILES, "-----------------------------\n");
}

void __cdecl FS_InitFilesystem()
{
    SEH_InitLanguage();
    FS_Startup((char*)"main");
    SEH_Init_StringEd();
    SEH_UpdateLanguageInfo();
}

int __cdecl FS_HashFileName(const char *fname, int hashSize)
{
    if (hashSize <= 0) hashSize = 1024;
    unsigned hash = 0;
    for (int i = 0; fname[i]; ++i) {
        int c = std::tolower((unsigned char)fname[i]);
        if (c == '.') break;
        if (c == '\\') c = '/';
        hash += c * (i + 119);
    }
    return ((hash >> 20) ^ hash ^ (hash >> 10)) & (hashSize - 1);
}

int __cdecl FS_FilenameCompare(const char *a, const char *b)
{
    while (*a || *b) {
        char ca = *a++, cb = *b++;
        if (ca == '\\' || ca == ':') ca = '/';
        if (cb == '\\' || cb == ':') cb = '/';
        ca = (char)std::toupper((unsigned char)ca);
        cb = (char)std::toupper((unsigned char)cb);
        if (ca != cb) return -1;
    }
    return 0;
}

void __cdecl FS_ReplaceSeparators(char *path)
{
    for (; *path; ++path) if (*path == '\\' || *path == ':') *path = '/';
}

void __cdecl FS_BuildOSPathForThread(const char *base, const char *game, const char *qpath, char *out, FsThread)
{
    SwitchPath(out, 256, base, game, qpath);
}

void __cdecl FS_BuildOSPath(const char *base, const char *game, const char *qpath, char *out)
{
    FS_BuildOSPathForThread(base, game, qpath, out, FS_THREAD_MAIN);
}

int __cdecl FS_CreatePath(char *path)
{
    char tmp[256];
    I_strncpyz(tmp, path, sizeof(tmp));
    for (char *p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0777);
            *p = '/';
        }
    }
    mkdir(tmp, 0777);
    return 0;
}

uint32_t __cdecl FS_FOpenFileReadForThread(const char *filename, int *file, FsThread)
{
    FS_CheckFileSystemStarted();
    if (file) *file = 0;
    if (!filename || !*filename) return (uint32_t)-1;

    char path[256];
    SwitchPath(path, sizeof(path), fs_basepath ? fs_basepath->current.string : kSwitchRoot,
               fs_gamedir, filename);
    FILE *fp = FS_FileOpenReadBinary(path);
    if (!fp) return (uint32_t)-1;

    int h = AllocHandle();
    if (!h) { fclose(fp); return (uint32_t)-1; }
    g_fsh[h].handleFiles.file.o = fp;
    g_fsh[h].fileSize = FS_FileGetFileSize(fp);
    g_fsh[h].streamed = 0;
    g_fsh[h].zipFile = nullptr;
    I_strncpyz(g_fsh[h].name, filename, sizeof(g_fsh[h].name));
    if (file) *file = h;
    return g_fsh[h].fileSize;
}

uint32_t __cdecl FS_FOpenFileRead(const char *filename, int *file)
{
    return FS_FOpenFileReadForThread(filename, file, FS_THREAD_MAIN);
}

uint32_t __cdecl FS_FOpenFileReadStream(const char *filename, int *file)
{
    return FS_FOpenFileReadForThread(filename, file, FS_THREAD_STREAM);
}

int __cdecl FS_FOpenFileReadDatabase(const char *filename, int *file)
{
    return (int)FS_FOpenFileReadForThread(filename, file, FS_THREAD_DATABASE);
}

int __cdecl FS_filelength(int f)
{
    if (f <= 0 || f >= 65 || !g_fsh[f].handleFiles.file.o) return -1;
    return g_fsh[f].fileSize;
}

uint32_t __cdecl FS_Read(uint8_t *buffer, uint32_t len, int h)
{
    if (h <= 0 || h >= 65 || !g_fsh[h].handleFiles.file.o) return 0;
    return FS_FileRead(buffer, len, g_fsh[h].handleFiles.file.o);
}

uint32_t __cdecl FS_Write(const char *buffer, uint32_t len, int h)
{
    if (h <= 0 || h >= 65 || !g_fsh[h].handleFiles.file.o) return 0;
    return FS_FileWrite(buffer, len, g_fsh[h].handleFiles.file.o);
}

int __cdecl FS_Seek(int h, int offset, int origin)
{
    if (h <= 0 || h >= 65 || !g_fsh[h].handleFiles.file.o) return -1;
    int whence = origin == 0 ? SEEK_SET : (origin == 1 ? SEEK_CUR : SEEK_END);
    return FS_FileSeek(g_fsh[h].handleFiles.file.o, offset, whence);
}

uint32_t __cdecl FS_FTell(int h)
{
    if (h <= 0 || h >= 65 || !g_fsh[h].handleFiles.file.o) return 0;
    return FS_FileTell(g_fsh[h].handleFiles.file.o);
}

void __cdecl FS_FCloseFile(int h)
{
    if (h <= 0 || h >= 65) return;
    if (g_fsh[h].handleFiles.file.o) fclose(g_fsh[h].handleFiles.file.o);
    std::memset(&g_fsh[h], 0, sizeof(g_fsh[h]));
}

void __cdecl FS_FCloseLogFile(int h) { FS_FCloseFile(h); }

int __cdecl FS_ReadFile(const char *qpath, void **buffer)
{
    if (buffer) *buffer = nullptr;
    int h = 0;
    int len = (int)FS_FOpenFileRead(qpath, &h);
    if (h && len >= 0) {
        if (buffer) {
            ++fs_loadStack;
            uint8_t *buf = (uint8_t*)FS_AllocMem(len + 1);
            *buffer = buf;
            FS_Read(buf, len, h);
            buf[len] = 0;
        }
        FS_FCloseFile(h);
        return len;
    }
    return -1;
}

uint32_t *__cdecl FS_AllocMem(int bytes) { return Hunk_AllocateTempMemory(bytes, "FS_AllocMem"); }
void __cdecl FS_ResetFiles() { fs_loadStack = 0; }
void __cdecl FS_FreeMem(char *buffer) { Hunk_FreeTempMemory(buffer); }
void __cdecl FS_FreeFile(char *buffer) { if (buffer) { --fs_loadStack; FS_FreeMem(buffer); } }

int __cdecl FS_FileExists(char *file)
{
    char path[256];
    SwitchPath(path, sizeof(path), fs_basepath ? fs_basepath->current.string : kSwitchRoot, fs_gamedir, file);
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    fclose(fp);
    return 1;
}

int __cdecl FS_FOpenFileWrite(const char *filename)
{
    return FS_FOpenFileWriteToDirForThread(filename, fs_gamedir, FS_THREAD_MAIN);
}

int __cdecl FS_FOpenFileWriteToDirForThread(const char *filename, const char *dir, FsThread)
{
    char path[256];
    SwitchPath(path, sizeof(path), fs_homepath ? fs_homepath->current.string : kSwitchRoot, dir, filename);
    FS_CreatePath(path);
    FILE *fp = FS_FileOpenWriteBinary(path);
    if (!fp) return 0;
    int h = AllocHandle();
    if (!h) { fclose(fp); return 0; }
    g_fsh[h].handleFiles.file.o = fp;
    I_strncpyz(g_fsh[h].name, filename, sizeof(g_fsh[h].name));
    return h;
}

int __cdecl FS_FOpenFileWriteToDir(const char *filename, const char *dir)
{
    return FS_FOpenFileWriteToDirForThread(filename, dir, FS_THREAD_MAIN);
}

int __cdecl FS_FOpenTextFileWrite(const char *filename)
{
    return FS_FOpenFileWrite(filename);
}

int __cdecl FS_FOpenFileAppend(const char *filename)
{
    char path[256];
    SwitchPath(path, sizeof(path), fs_homepath ? fs_homepath->current.string : kSwitchRoot, fs_gamedir, filename);
    FS_CreatePath(path);
    FILE *fp = FS_FileOpenAppendText(path);
    if (!fp) return 0;
    int h = AllocHandle();
    if (!h) { fclose(fp); return 0; }
    g_fsh[h].handleFiles.file.o = fp;
    I_strncpyz(g_fsh[h].name, filename, sizeof(g_fsh[h].name));
    return h;
}

int __cdecl FS_WriteFile(char *filename, char *buffer, uint32_t size)
{
    int h = FS_FOpenFileWrite(filename);
    if (!h) return 0;
    uint32_t n = FS_Write(buffer, size, h);
    FS_FCloseFile(h);
    return n == size;
}

int __cdecl FS_WriteFileToDir(const char *filename, const char *dir, char *buffer, uint32_t size)
{
    int h = FS_FOpenFileWriteToDir(filename, dir);
    if (!h) return 0;
    uint32_t n = FS_Write(buffer, size, h);
    FS_FCloseFile(h);
    return n == size;
}

bool __cdecl FS_Delete(const char *filename)
{
    char path[256];
    SwitchPath(path, sizeof(path), fs_homepath ? fs_homepath->current.string : kSwitchRoot, fs_gamedir, filename);
    return std::remove(path) == 0;
}

bool __cdecl FS_DeleteInDir(char *filename, char *dir)
{
    char path[256];
    SwitchPath(path, sizeof(path), fs_homepath ? fs_homepath->current.string : kSwitchRoot, dir, filename);
    return std::remove(path) == 0;
}

int __cdecl FS_TouchFile(const char *name)
{
    int h = FS_FOpenFileWrite(name);
    if (!h) return 0;
    FS_FCloseFile(h);
    return 1;
}

void __cdecl FS_Flush(int h) { if (h > 0 && h < 65 && g_fsh[h].handleFiles.file.o) fflush(g_fsh[h].handleFiles.file.o); }

uint32_t __cdecl FS_FOpenFileByMode(char *qpath, int *f, fsMode_t mode)
{
    if (!f) return (uint32_t)-1;
    switch (mode) {
        case FS_READ: return FS_FOpenFileRead(qpath, f);
        case FS_WRITE: *f = FS_FOpenFileWrite(qpath); return *f ? 0 : (uint32_t)-1;
        case FS_APPEND:
        case FS_APPEND_SYNC: *f = FS_FOpenFileAppend(qpath); return *f ? 0 : (uint32_t)-1;
        default: *f = 0; return (uint32_t)-1;
    }
}

int __cdecl FS_SV_FOpenFileRead(const char *filename, int *fp) { return (int)FS_FOpenFileRead(filename, fp); }
int __cdecl FS_SV_FOpenFileWrite(const char *filename) { return FS_FOpenFileWrite(filename); }
int __cdecl FS_SV_FileExists(char *file) { return FS_FileExists(file); }

void __cdecl FS_CopyFile(char *from, char *to)
{
    FILE *a = fopen(from, "rb"), *b = fopen(to, "wb");
    if (!a || !b) { if (a) fclose(a); if (b) fclose(b); return; }
    char buf[8192]; size_t n;
    while ((n = fread(buf,1,sizeof(buf),a)) != 0) fwrite(buf,1,n,b);
    fclose(a); fclose(b);
}

void __cdecl FS_Remove(const char *path) { std::remove(path); }
void __cdecl FS_Rename(char *from, char *fromDir, char *to, char *toDir)
{
    char a[256], b[256];
    SwitchPath(a,sizeof(a),fs_homepath ? fs_homepath->current.string : kSwitchRoot,fromDir,from);
    SwitchPath(b,sizeof(b),fs_homepath ? fs_homepath->current.string : kSwitchRoot,toDir,to);
    std::rename(a,b);
}
void __cdecl FS_SV_Rename(char *from, char *to) { std::rename(from,to); }

bool __cdecl FS_IsFileInZip(int) { return false; }
int __cdecl FS_ConditionalRestart(int, int) { return 0; }
int __cdecl FS_LoadStack() { return fs_loadStack; }
int __cdecl FS_OpenFileOverwrite(char *qpath) { return FS_FOpenFileWrite(qpath); }
void __cdecl FS_ConvertPath(char *s) { FS_ReplaceSeparators(s); }
char *__cdecl FS_ShiftStr(const char *s, char shift) { static char out[256]; I_strncpyz(out,s,sizeof(out)); for(char *p=out;*p;++p)*p+=shift; return out; }
bool __cdecl FS_NeedRestart(int) { return false; }
void __cdecl FS_Restart(int, int) {}
void __cdecl FS_ClearIwdReferences() {}
void __cdecl FS_DisplayPath(int) {}
void __cdecl FS_Path_f() {}
void __cdecl FS_FullPath_f() {}
void __cdecl FS_Dir_f() {}
void __cdecl FS_TouchFile_f() {}
void __cdecl FS_AddCommands() {}
void __cdecl FS_SetRestrictions() {}
void __cdecl FS_RemoveCommands() {}
void __cdecl FS_ShutdownSearchPaths() { fs_searchpaths = nullptr; }
void __cdecl FS_Shutdown() { for(int i=1;i<65;++i) FS_FCloseFile(i); FS_ShutdownSearchPaths(); }
void __cdecl FS_FreeFileList(const char **) {}
int __cdecl FS_GetModList(char *, int) { return 0; }
int __cdecl FS_GetFileList(const char *, const char *, FsListBehavior_e, char *buf, int size) { if(size) *buf=0; return 0; }
const char **__cdecl FS_ListFiles(const char *, const char *, FsListBehavior_e, int *num) { if(num)*num=0; return nullptr; }
const char **__cdecl FS_ListFilesInLocation(const char *, const char *, FsListBehavior_e, int *num, int) { if(num)*num=0; return nullptr; }
char *__cdecl FS_ReferencedIwdPureChecksums() { static char s[4] = ""; return s; }
char *__cdecl FS_ReferencedIwdNames() { static char s[4] = ""; return s; }
char *__cdecl FS_ReferencedIwdChecksums() { static char s[4] = ""; return s; }
char *__cdecl FS_LoadedIwdNames() { static char s[4] = ""; return s; }
char *__cdecl FS_LoadedIwdChecksums() { static char s[4] = ""; return s; }
char *__cdecl FS_LoadedIwdPureChecksums() { static char s[4] = ""; return s; }
void __cdecl FS_Printf(int h, const char *fmt, ...) { if(h<=0||h>=65||!g_fsh[h].handleFiles.file.o)return; va_list ap; va_start(ap,fmt); vfprintf(g_fsh[h].handleFiles.file.o,fmt,ap); va_end(ap); }
int __cdecl FS_WriteLog(const char *b,uint32_t n,int h){return (int)FS_Write(b,n,h);}
int __cdecl FS_FOpenFileWriteToDirForThread(const char*,const char*,FsThread);

#endif

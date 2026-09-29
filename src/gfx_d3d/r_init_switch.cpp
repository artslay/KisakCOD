#include <universal/q_shared.h>
#include "r_init.h"
#include "r_material.h"
#include "r_fog.h"
#include "r_state.h"
#include "r_image.h"
#include "r_rendercmds.h"
#include "r_rendertarget.h"
#include "r_buffers.h"
#include "r_scene.h"
#include "r_water.h"
#include "r_light.h"
#include "r_workercmds.h"
#include "r_draw_method.h"
#include <gfx/gfx_backend.h>

// These are implemented by the shared renderer dvar/command modules.
extern void __cdecl R_RegisterDvars();
extern void __cdecl R_RegisterCmds();
#include <gfx/opengl/gl_backend.h>

#ifdef __SWITCH__

GfxAssets gfxAssets{};
DxGlobals dx{};
r_global_permanent_t rgp{};
vidConfig_t vidConfig{};
GfxMetrics gfxMetrics{};
bool g_allocateMinimalResources = false;
GfxConfiguration gfxCfg{};
GfxGlobals r_glob{};
int g_disableRendering = 0;

const dvar_t *r_mode = nullptr;
const dvar_t *r_displayRefresh = nullptr;
const dvar_t *r_noborder = nullptr;

static bool s_registered = false;

void TRACK_r_init() {}
void R_SyncGpu(int(__cdecl *)(unsigned __int64)) { if (g_gfxBackend) g_gfxBackend->WaitForGpu(); }
bool R_IsUsingAdaptiveGpuSync() { return false; }
bool __cdecl RB_IsGpuFenceFinished()
{
    if (!dx.flushGpuQuery || !dx.flushGpuQueryIssued)
        return true;
    uint32_t data = 0;
    return dx.flushGpuQuery->GetData(&data, sizeof(data), 1) == S_OK;
}
void R_FatalInitError(const char *msg) { Com_Error(ERR_FATAL, "%s", msg ? msg : "renderer init failed"); }
void R_FatalLockError(HRESULT) { R_FatalInitError("renderer lock failed"); }
const char *R_ErrorDescription(HRESULT hr) { return hr == S_OK ? "S_OK" : "OpenGL backend error"; }

void R_SetColorMappings() {}
void R_CalcGammaRamp(GfxGammaRamp *ramp) {
    if (!ramp) return;
    for (int i = 0; i < 256; ++i) ramp->entries[i] = static_cast<uint16_t>(i << 8);
}
void R_GammaCorrect(uint8_t *, int) {}
void SetGfxConfig(const GfxConfiguration *config) { if (config) gfxCfg = *config; }

void R_InitThreads() { R_InitRenderThread(); }
static int g_remoteScreenUpdateNesting = 0;






void R_ShutdownMaterialUsage() {}

void R_ShutdownDirect3D() {
    if (g_gfxBackend) g_gfxBackend->Shutdown();
    delete dx.device;
    dx.device = nullptr;
    dx.d3d9 = nullptr;
}

void R_ReleaseForShutdownOrReset() {}
void R_UnloadWorld() {}
void R_BeginRegistration(vidConfig_t *out) {
    iassert(!rg.registered);
    R_Init();
    iassert(rg.registered);
    if (out)
        *out = vidConfig;
    s_registered = true;
}
extern void Switch_LogRaw(const char *msg);

void R_Init() {
    // Match the original renderer bootstrap order: renderer dvars/commands must
    // exist before R_InitImages()->R_SetPicmip() accesses them.
    Switch_LogRaw("[SWITCH RINIT TRACE] before R_Register\n");
    R_Register();
    Switch_LogRaw("[SWITCH RINIT TRACE] after R_Register\n");

    Switch_LogRaw("[SWITCH RINIT TRACE] before R_InitGlobalStructs\n");
    R_InitGlobalStructs();
    Switch_LogRaw("[SWITCH RINIT TRACE] after R_InitGlobalStructs\n");

    Switch_LogRaw("[SWITCH RINIT TRACE] before R_InitDrawMethod\n");
    R_InitDrawMethod();
    Switch_LogRaw("[SWITCH RINIT TRACE] after R_InitDrawMethod\n");

    Switch_LogRaw("[SWITCH RINIT TRACE] before R_InitGraphicsApi\n");
    R_InitGraphicsApi();
    Switch_LogRaw("[SWITCH RINIT TRACE] after R_InitGraphicsApi\n");

    Switch_LogRaw("[SWITCH RINIT TRACE] before R_InitSystems\n");
    R_InitSystems();
    Switch_LogRaw("[SWITCH RINIT TRACE] after R_InitSystems\n");
}
char R_InitRendererForWindow(HWND) { R_Init(); return 1; }
HWND R_CreateSwapChains(int, GfxWindowParms *, int) { return nullptr; }
char R_BeginRegistration_R_InitHardware(GfxWindowParms *wnd) { return R_InitHardware(wnd); }
char R_TestDevice() { return 1; }
void R_SetupTargetWindow(int) {}
void R_InitEditor() {}
char R_SetupRendertarget_CheckDevice(HWND__ *) { return 1; }
bool R_IsRegisteredRenderWindow(HWND__ *) { return s_registered; }
void R_CheckTargetWindow(HWND__ *) {}
void R_SortMaterials() {}
void R_Hwnd_Resize(HWND__ *, int width, int height) {
    if (g_gfxBackend) g_gfxBackend->SetViewport(0, 0, width, height);
}

void R_InitGraphicsApi() {
    Switch_LogRaw("[SWITCH RINIT TRACE] R_InitGraphicsApi: before CreateOpenGLBackend\n");
    if (!g_gfxBackend)
        g_gfxBackend = CreateOpenGLBackend();
    Switch_LogRaw("[SWITCH RINIT TRACE] R_InitGraphicsApi: after CreateOpenGLBackend\n");

    if (!g_gfxBackend)
        R_FatalInitError("CreateOpenGLBackend failed");

    Switch_LogRaw("[SWITCH RINIT TRACE] R_InitGraphicsApi: before backend Init\n");
    if (!g_gfxBackend->Init(nullptr))
        R_FatalInitError(g_gfxBackend->GetLastError());
    Switch_LogRaw("[SWITCH RINIT TRACE] R_InitGraphicsApi: after backend Init\n");

    if (!dx.device) dx.device = new IDirect3DDevice9;
    vidConfig.sceneWidth = 1280;
    vidConfig.sceneHeight = 720;
    vidConfig.displayWidth = 1280;
    vidConfig.displayHeight = 720;
    vidConfig.displayFrequency = 60;
    vidConfig.aspectRatioWindow = 1280.0f / 720.0f;
    vidConfig.aspectRatioScenePixel = 1.0f;
    vidConfig.aspectRatioDisplayPixel = 1.0f;
    vidConfig.maxTextureSize = 4096;
    vidConfig.maxTextureMaps = 16;
    vidConfig.deviceSupportsGamma = false;
    dx.depthStencilFormat = D3DFMT_D24S8;
    dx.multiSampleType = D3DMULTISAMPLE_NONE;
    dx.multiSampleQuality = 0;
}
void R_InitSystems() {
    Switch_LogRaw("[SWITCH RSYS TRACE] before R_InitImages\n");
    R_InitImages();
    Switch_LogRaw("[SWITCH RSYS TRACE] after R_InitImages\n");

    Switch_LogRaw("[SWITCH RSYS TRACE] before Material_Init\n");
    Material_Init();
    Switch_LogRaw("[SWITCH RSYS TRACE] after Material_Init\n");

    Switch_LogRaw("[SWITCH RSYS TRACE] before R_InitFonts\n");
    R_InitFonts();
    Switch_LogRaw("[SWITCH RSYS TRACE] after R_InitFonts\n");

    Switch_LogRaw("[SWITCH RSYS TRACE] before R_InitLoadWater\n");
    R_InitLoadWater();
    Switch_LogRaw("[SWITCH RSYS TRACE] after R_InitLoadWater\n");

    Switch_LogRaw("[SWITCH RSYS TRACE] before R_InitLightDefs\n");
    R_InitLightDefs();
    Switch_LogRaw("[SWITCH RSYS TRACE] after R_InitLightDefs\n");

    Switch_LogRaw("[SWITCH RSYS TRACE] before R_ClearFogs\n");
    R_ClearFogs();
    Switch_LogRaw("[SWITCH RSYS TRACE] after R_ClearFogs\n");

    Switch_LogRaw("[SWITCH RSYS TRACE] before R_InitDebug\n");
    R_InitDebug();
    Switch_LogRaw("[SWITCH RSYS TRACE] after R_InitDebug\n");

    rg.registered = 1;
    Switch_LogRaw("[SWITCH RSYS TRACE] rg.registered=1\n");
}
char R_PreCreateWindow() { return 1; }
void R_StoreDirect3DCaps(uint32_t) {}
void R_GetDirect3DCaps(uint32_t, _D3DCAPS9 *) {}
void R_SetShadowmapFormats_DX(uint32_t) {
    gfxMetrics.shadowmapFormatPrimary = 0;
    gfxMetrics.shadowmapFormatSecondary = 0;
}
uint32_t R_ChooseAdapter() { return 0; }
void Sys_HideSplashWindow() {}
char R_CreateGameWindow(GfxWindowParms *wnd) { return R_InitHardware(wnd); }

char R_InitHardware(const GfxWindowParms *wnd) {
    if (!g_gfxBackend) R_InitGraphicsApi();
    if (wnd) {
        vidConfig.sceneWidth = wnd->sceneWidth;
        vidConfig.sceneHeight = wnd->sceneHeight;
        vidConfig.displayWidth = wnd->displayWidth;
        vidConfig.displayHeight = wnd->displayHeight;
    }
    R_InitGamma();
    R_InitScene();
    R_InitSystems();
    return 1;
}
void R_StoreWindowSettings(const GfxWindowParms *) {}
void R_InitGamma() {}
char R_CreateForInitOrReset() { return 1; }

IDirect3DQuery9 *RB_HW_AllocOcclusionQuery() { return nullptr; }
char R_CreateDevice(const GfxWindowParms *) { return 1; }
void R_SetD3DPresentParameters(_D3DPRESENT_PARAMETERS_ *, const GfxWindowParms *) {}
void R_SetupAntiAliasing(const GfxWindowParms *) {}
HRESULT R_CreateDeviceInternal(HWND__ *, uint32_t, _D3DPRESENT_PARAMETERS_ *) { return S_OK; }
int R_GetDeviceType() { return 0; }
void R_SetWndParms(GfxWindowParms *wnd) {
    if (!wnd) return;
    wnd->sceneWidth = vidConfig.sceneWidth;
    wnd->sceneHeight = vidConfig.sceneHeight;
    wnd->displayWidth = vidConfig.displayWidth;
    wnd->displayHeight = vidConfig.displayHeight;
    wnd->hz = 60;
}
void R_Register() {
    R_RegisterDvars();
    R_RegisterCmds();
}
void R_InitGlobalStructs() {
    vidConfig = {};
    gfxMetrics = {};
    gfxMetrics.canMipCubemaps = true;
    g_disableRendering = 0;
}
void R_EndRegistration() {}
void R_TrackStatistics(trStatistics_t *) {}
void R_UpdateTeamColors(int, const float *, const float *) {}
void R_ConfigureRenderer(const GfxConfiguration *config) { SetGfxConfig(config); }
void R_ComErrorCleanup() {}
bool R_CheckLostDevice() { return false; }
void R_MakeDedicated(const GfxConfiguration *config) { SetGfxConfig(config); }
void R_UpdateGpuSyncType() {}
int R_IsHiDef() { return 1; }

// r_texturemem.cpp is intentionally excluded from the Switch build because its
// implementation depends on Windows DirectDraw. The renderer only needs a texture
// memory budget for picmip selection during startup, so keep a conservative Switch
// budget here instead of probing nonexistent D3D9/DirectDraw resources.
uint32_t __cdecl R_AvailableTextureMemory()
{
    return 2048;
}

uint32_t __cdecl R_DetectCurrentTextureMemory()
{
    return R_AvailableTextureMemory();
}

void R_ShutdownStreams() {}

void R_Shutdown(int destroyWindow) {
    (void)destroyWindow;
    R_ShutdownStreams();
    R_ShutdownMaterialUsage();
    if (s_registered) {
        R_ShutdownImages();
        s_registered = false;
    }
    R_ShutdownDirect3D();
}

#endif

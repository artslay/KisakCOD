#include <universal/q_shared.h>
#include "r_image.h"
#include <qcommon/threads.h>
#include <qcommon/mem_track.h>
#include <qcommon/qcommon.h>
#include <universal/com_memory.h>
#include <qcommon/cmd.h>
#include <database/database.h>
#include "r_init.h"
#include "r_dvars.h"
#include <universal/com_files.h>
#include <universal/q_parse.h>
#include "rb_logfile.h"
#include <universal/profile.h>
#include "r_pixelcost_load_obj.h"
#include "r_utils.h"
#include "r_texturemem.h"
#include "rb_state.h"
#include "r_state.h"
#include "r_outdoor.h"

#include <algorithm>

#ifdef __SWITCH__
extern void __cdecl Sys_Print(const char *msg);
#endif

#ifdef __SWITCH__
#include <unistd.h>
#include <cstring>
static inline void R_SwitchPicmipTrace(const char *msg)
{
    if (msg)
        (void)::write(STDERR_FILENO, msg, std::strlen(msg));
}
#endif

#ifdef __SWITCH__
static bool R_GLImageFormat(_D3DFORMAT f, GLenum &i, GLenum &u, GLenum &t, bool &compressed)
{
    compressed = false;
    switch (f) {
    case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: i=GL_RGBA8; u=GL_BGRA; t=GL_UNSIGNED_BYTE; return true;
    case D3DFMT_A8: case D3DFMT_L8: i=GL_R8; u=GL_RED; t=GL_UNSIGNED_BYTE; return true;
    case D3DFMT_A8L8: i=GL_RG8; u=GL_RG; t=GL_UNSIGNED_BYTE; return true;
    case D3DFMT_R32F: i=GL_R32F; u=GL_RED; t=GL_FLOAT; return true;
    case D3DFMT_G16R16F: i=GL_RG16F; u=GL_RG; t=GL_HALF_FLOAT; return true;
    case D3DFMT_D16: i=GL_DEPTH_COMPONENT16; u=GL_DEPTH_COMPONENT; t=GL_UNSIGNED_SHORT; return true;
    case D3DFMT_D24S8: i=GL_DEPTH24_STENCIL8; u=GL_DEPTH_STENCIL; t=GL_UNSIGNED_INT_24_8; return true;
    case D3DFMT_D24X8: i=GL_DEPTH_COMPONENT24; u=GL_DEPTH_COMPONENT; t=GL_UNSIGNED_INT; return true;
    case D3DFMT_DXT1: i=GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; compressed=true; return true;
    case D3DFMT_DXT3: i=GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; compressed=true; return true;
    case D3DFMT_DXT5: i=GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; compressed=true; return true;
    default: return false;
    }
}
static uint32_t R_GLFullMipCount(uint32_t w,uint32_t h,uint32_t d){uint32_t n=1;while(w>1||h>1||d>1){w=std::max(1u,w>>1);h=std::max(1u,h>>1);d=std::max(1u,d>>1);++n;}return n;}
static void R_GLAllocTexture(KisakGLTexture *x,GLenum target,uint32_t w,uint32_t h,uint32_t d,uint32_t levels,_D3DFORMAT f)
{
    GLenum i,u,t; bool c; if(!R_GLImageFormat(f,i,u,t,c)) return;
    x->target=target;x->sourceFormat=f;x->internalFormat=i;x->uploadFormat=u;x->uploadType=t;x->width=w;x->height=h;x->depth=d;x->mipLevels=levels;
    glGenTextures(1,&x->object); glBindTexture(target,x->object);
    glTexParameteri(target,GL_TEXTURE_MIN_FILTER,levels>1?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR);
    glTexParameteri(target,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(target,GL_TEXTURE_WRAP_S,GL_REPEAT); glTexParameteri(target,GL_TEXTURE_WRAP_T,GL_REPEAT);
    if(target==GL_TEXTURE_3D||target==GL_TEXTURE_CUBE_MAP) glTexParameteri(target,GL_TEXTURE_WRAP_R,GL_REPEAT);
    for(uint32_t l=0;l<levels;++l){uint32_t lw=std::max(1u,w>>l),lh=std::max(1u,h>>l),ld=std::max(1u,d>>l);
        if(target==GL_TEXTURE_3D) glTexImage3D(target,l,(GLint)i,lw,lh,ld,0,u,t,nullptr);
        else if(target==GL_TEXTURE_CUBE_MAP) for(uint32_t fce=0;fce<6;++fce) glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+fce,l,(GLint)i,lw,lh,0,u,t,nullptr);
        else glTexImage2D(target,l,(GLint)i,lw,lh,0,u,t,nullptr);
    }
}
static void R_GLUploadTexture(const GfxImage *image,_D3DFORMAT f,_D3DCUBEMAP_FACES face,uint32_t l,const uint8_t *src)
{
    auto *x=image->texture.basemap;if(!x||!src)return;GLenum i,u,t;bool c;if(!R_GLImageFormat(f,i,u,t,c))return;glBindTexture(x->target,x->object);
    uint32_t w=std::max(1u,(uint32_t)image->width>>l),h=std::max(1u,(uint32_t)image->height>>l),d=std::max(1u,(uint32_t)image->depth>>l);
    if(c){uint32_t b=f==D3DFMT_DXT1?8:16,s=((w+3)/4)*((h+3)/4)*b*d;
        if(x->target==GL_TEXTURE_CUBE_MAP)glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,l,0,0,w,h,i,s,src);
        else if(x->target==GL_TEXTURE_3D)glCompressedTexSubImage3D(GL_TEXTURE_3D,l,0,0,0,w,h,d,i,s,src);
        else glCompressedTexSubImage2D(GL_TEXTURE_2D,l,0,0,w,h,i,s,src);
    } else if(x->target==GL_TEXTURE_3D)glTexSubImage3D(GL_TEXTURE_3D,l,0,0,0,w,h,d,u,t,src);
    else if(x->target==GL_TEXTURE_CUBE_MAP)glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,l,0,0,w,h,u,t,src);
    else glTexSubImage2D(GL_TEXTURE_2D,l,0,0,w,h,u,t,src);
}
#endif


static const char *g_imageProgNames[14] =
{
  "$shadow_cookie",
  "$shadow_cookie_blur",
  "$shadowmap_sun",
  "$shadowmap_spot",
  "$floatz",
  "$post_effect_0",
  "$post_effect_1",
  "$pingpong_0",
  "$pingpong_1",
  "$resolved_scene",
  "$savedscreen",
  "$raw",
  "$model_lighting",
  "$model_lighting1"
}; // idb

static const char *imageTypeName[IMAGE_TRACK_COUNT] =
{
    "misc",
    "debug",
    "$tex+?",
    "ui",
    "lmap",
    "light",
    "f/x",
    "hud",
    "model",
    "world"
};

static const char *g_platform_name[2] =
{
    "current",
    "min_pc"
};

//ImgGlobals imageGlobals; // LWSS: moved to db_registry for DEDICATED
GfxImage g_imageProgs[14];

struct BuiltinImageConstructorTable // sizeof=0x8
{                                       // ...
    const char *name;                   // ...
    void(__cdecl *LoadCallback)(GfxImage *); // ...
};
const BuiltinImageConstructorTable constructorTable[8] =
{
    {"$white", Image_LoadWhite},
    {"$black", Image_LoadBlack},
    {"$black_3d", Image_LoadBlack3D},
    {"$black_cube", Image_LoadBlackCube},
    {"$gray", Image_LoadGray},
    {"$identitynormalmap", Image_LoadIdentityNormalMap},
    {"$outdoor", R_GenerateOutdoorImage},
    {"$pixelcostcolorcode", Image_LoadPixelCostColorCode}
};

void __cdecl TRACK_r_image()
{
    track_static_alloc_internal(g_imageProgs, 504, "g_imageProgs", 18);
    track_static_alloc_internal(imageTypeName, 40, "imageTypeName", 18);
}

void __cdecl R_DelayLoadImage(XAssetHeader header)
{
    GfxImage *image = header.image;
    if (image->delayLoadPixels)
    {
        image->delayLoadPixels = false;
        int externalDataSize = image->cardMemory.platform[0];
        image->cardMemory.platform[0] = 0;
        image->cardMemory.platform[1] = 0;
        if (r_loadForRenderer->current.enabled && !dx.deviceLost)
        {
            if (!Image_LoadFromFile(image))
                Image_AssignDefaultTexture(image);
            if (!image->texture.basemap)
            {
                HRESULT hr = dx.device->TestCooperativeLevel();
                if (hr != 0x88760868 && hr != 0x88760869)
                    Com_Error(ERR_DROP, "Couldn't load image '%s'\n", image->name);
            }
        }
        DB_LoadedExternalData(externalDataSize);
    }
}

void __cdecl R_GetImageList(ImageList *imageList)
{
    iassert( imageList );
    imageList->count = 0;
    DB_EnumXAssets(ASSET_TYPE_IMAGE, (void(__cdecl *)(XAssetHeader, void *))R_AddImageToList, imageList, 1);
}

void __cdecl R_AddImageToList(XAssetHeader header, ImageList* imageList)
{
    iassert( imageList->count < ARRAY_COUNT( imageList->image ) );
    imageList->image[imageList->count++] = header.image;
}

void __cdecl R_SumOfUsedImages(Image_MemUsage *usage)
{
    const char *v1; // eax
    GfxImage *image; // [esp+0h] [ebp-2040h]
    uint32_t v3[4]; // [esp+4h] [ebp-203Ch] BYREF
    int v4; // [esp+14h] [ebp-202Ch]
    int v5; // [esp+18h] [ebp-2028h]
    int v6; // [esp+1Ch] [ebp-2024h]
    int v7; // [esp+20h] [ebp-2020h]
    int v8; // [esp+24h] [ebp-201Ch]
    int v9; // [esp+28h] [ebp-2018h]
    int v10; // [esp+2Ch] [ebp-2014h]
    uint32_t i; // [esp+30h] [ebp-2010h]
    int v12; // [esp+34h] [ebp-200Ch]
    ImageList imageList; // [esp+38h] [ebp-2008h] BYREF

    iassert( usage );
    R_GetImageList(&imageList);
    memset(v3, 0, sizeof(v3));
    v4 = 0;
    v5 = 0;
    v6 = 0;
    v7 = 0;
    v8 = 0;
    v9 = 0;
    v12 = 0;
    for (i = 0; i < imageList.count; ++i)
    {
        image = imageList.image[i];
        iassert( image );
        v10 = image->cardMemory.platform[0];
        v3[image->track] += v10;
        if (!Image_IsCodeImage(image->track))
            v12 += v10;
    }
    usage->total = v12;
    usage->lightmap = v4;
    if (!dx.deviceLost && usage->total != imageGlobals.totalMemory.platform[0])
    {
        v1 = va("%i != %i", usage->total, imageGlobals.totalMemory.platform[0]);
        MyAssertHandler(
            ".\\r_image.cpp",
            223,
            0,
            "%s\n\t%s",
            "dx.deviceLost || usage->total == imageGlobals.totalMemory.platform[PICMIP_PLATFORM_USED]",
            v1);
    }
    usage->minspec = imageGlobals.totalMemory.platform[1];
}

void __cdecl Image_Release(GfxImage *image)
{
    int platform; // [esp+0h] [ebp-4h]

    iassert( image );
    if (!Image_IsCodeImage(image->track))
    {
        for (platform = 0; platform < 2; ++platform)
            imageGlobals.totalMemory.platform[platform] -= image->cardMemory.platform[platform];
    }
    if (image->texture.basemap)
    {
        //image->texture.basemap->Release(image->texture.basemap);
        image->texture.basemap->Release();
        image->texture.basemap = 0;
        image->cardMemory.platform[0] = 0;
        image->cardMemory.platform[1] = 0;
    }
    else if (r_loadForRenderer->current.enabled)
    {
        iassert( !image->cardMemory.platform[PICMIP_PLATFORM_USED] );
    }
}

GfxImage *__cdecl Image_AllocProg(int imageProgType, uint8_t category, uint8_t semantic)
{
    GfxImage *image; // [esp+0h] [ebp-Ch]
    const char *name; // [esp+4h] [ebp-8h]

    image = &g_imageProgs[imageProgType];
    iassert(image);
    name = g_imageProgNames[imageProgType];
    image->name = name;
    iassert(category != IMG_CATEGORY_UNKNOWN);
    image->category = category;
    image->semantic = semantic;
    image->track = IMAGE_TRACK_MISC;
    imageGlobals.imageHashTable[Image_GetAvailableHashLocation(name)] = image;
    return &g_imageProgs[imageProgType];
}

void __cdecl Image_SetupAndLoad(
    GfxImage *image,
    int width,
    int height,
    int depth,
    int imageFlags,
    _D3DFORMAT imageFormat)
{
    Image_Setup(image, width, height, depth, imageFlags, imageFormat);
}

void __cdecl R_ShutdownImages()
{
    GfxImage *image; // [esp+0h] [ebp-2014h]
    int numBackups; // [esp+4h] [ebp-2010h]
    uint32_t i; // [esp+8h] [ebp-200Ch]
    GfxImage* backupImages[IMAGE_HASH_TABLE_SIZE]; // [esp+Ch] [ebp-2008h]
    int j; // [esp+2010h] [ebp-4h]

    RB_UnbindAllImages();
    numBackups = 0;
    for (i = 0; i < IMAGE_HASH_TABLE_SIZE; ++i)
    {
        image = imageGlobals.imageHashTable[i];
        if (image)
        {
            if (Image_IsProg(image))
                backupImages[numBackups++] = image;
            else
                Image_Free(imageGlobals.imageHashTable[i]);
        }
    }

    memset(imageGlobals.imageHashTable, 0, sizeof(imageGlobals.imageHashTable));

    // Restore Images that were deleted in the memset above
    for (j = 0; j < numBackups; ++j)
    {
        image = backupImages[j];
        imageGlobals.imageHashTable[Image_GetAvailableHashLocation(image->name)] = image;
    }
}

void __cdecl Image_SetupRenderTarget(
    GfxImage *image,
    uint16_t width,
    uint16_t height,
    _D3DFORMAT imageFormat)
{
    iassert(image);
    iassert(image->semantic == TS_2D);
    Image_SetupAndLoad(image, width, height, 1, IMG_FLAG_NOPICMIP | IMG_FLAG_NOMIPMAPS | IMG_FLAG_RENDER_TARGET, imageFormat);
}

void __cdecl Load_Texture(GfxTexture *remoteLoadDef, GfxImage *image)
{
    uint32_t mipDepth; // [esp+0h] [ebp-60h]
    uint32_t mipHeight; // [esp+4h] [ebp-5Ch]
    uint32_t mipWidth; // [esp+8h] [ebp-58h]
    _D3DCUBEMAP_FACES v5; // [esp+Ch] [ebp-54h]
    uint16_t v6; // [esp+14h] [ebp-4Ch]
    uint16_t v7; // [esp+18h] [ebp-48h]
    GfxImageLoadDef *loadDef; // [esp+34h] [ebp-2Ch]
    LONG externalDataSize; // [esp+38h] [ebp-28h]
    signed int mipCount; // [esp+3Ch] [ebp-24h]
    unsigned char *data; // [esp+40h] [ebp-20h]
    int faceCount; // [esp+50h] [ebp-10h]
    signed int faceIndex; // [esp+54h] [ebp-Ch]
    _D3DFORMAT imageFormat; // [esp+58h] [ebp-8h]
    signed int mipLevel; // [esp+5Ch] [ebp-4h]

    loadDef = remoteLoadDef->loadDef;
    iassert(loadDef == image->texture.loadDef);

    image->texture.basemap = 0;
    if (r_loadForRenderer->current.enabled)
    {
        imageFormat = loadDef->format;
        if (loadDef->resourceSize)
        {
            image->delayLoadPixels = 0;
            if (image->mapType == MAPTYPE_2D)
            {
                Image_Create2DTexture_PC(
                    image,
                    loadDef->dimensions[0],
                    loadDef->dimensions[1],
                    loadDef->levelCount,
                    0,
                    imageFormat);
                faceCount = 1;
            }
            else if (image->mapType == MAPTYPE_3D)
            {
                Image_Create3DTexture_PC(
                    image,
                    loadDef->dimensions[0],
                    loadDef->dimensions[1],
                    loadDef->dimensions[2],
                    loadDef->levelCount,
                    0,
                    imageFormat);
                faceCount = 1;
            }
            else
            {
                iassert(image->mapType == MAPTYPE_CUBE);
                Image_CreateCubeTexture_PC(image, loadDef->dimensions[0], loadDef->levelCount, imageFormat);
                faceCount = 6;
            }
            data = &loadDef->data[0];
            mipCount = Image_CountMipmaps(loadDef->flags, image->width, image->height, image->depth);
            for (faceIndex = 0; faceIndex < faceCount; ++faceIndex)
            {
                if (faceCount == 1)
                    v5 = D3DCUBEMAP_FACE_POSITIVE_X;
                else
                    v5 = (D3DCUBEMAP_FACES)Image_CubemapFace(faceIndex);
                for (mipLevel = 0; mipLevel < mipCount; ++mipLevel)
                {
                    Image_UploadData(image, imageFormat, v5, mipLevel, data);
                    if (image->width >> mipLevel > 1)
                        mipWidth = image->width >> mipLevel;
                    else
                        mipWidth = 1;
                    if (image->height >> mipLevel > 1)
                        mipHeight = image->height >> mipLevel;
                    else
                        mipHeight = 1;
                    if (image->depth >> mipLevel > 1)
                        mipDepth = image->depth >> mipLevel;
                    else
                        mipDepth = 1;
                    data += Image_GetCardMemoryAmountForMipLevel(imageFormat, mipWidth, mipHeight, mipDepth);                }
            }
            iassert(data == &loadDef->data[loadDef->resourceSize]);
        }
        else if (image->category == IMG_CATEGORY_WATER)
        {
            image->delayLoadPixels = 0;
            if (loadDef->dimensions[0] >> r_picmip_water->current.integer < 4)
                v7 = 4;
            else
                v7 = loadDef->dimensions[0] >> r_picmip_water->current.integer;
            if (loadDef->dimensions[1] >> r_picmip_water->current.integer < 4)
                v6 = 4;
            else
                v6 = loadDef->dimensions[1] >> r_picmip_water->current.integer;
            image->cardMemory.platform[0] = 0;
            image->cardMemory.platform[1] = 0;
            Image_Create2DTexture_PC(image, v7, v6, loadDef->levelCount, 0x10000, imageFormat);
        }
        else
        {
            if (image->cardMemory.platform[0] != Image_GetCardMemoryAmount(
                loadDef->flags,
                loadDef->format,
                loadDef->dimensions[0],
                loadDef->dimensions[1],
                loadDef->dimensions[2]))
                MyAssertHandler(
                    ".\\r_image.cpp",
                    788,
                    1,
                    "%s\n\t(image->name) = %s",
                    "(static_cast< uint >( image->cardMemory.platform[PICMIP_PLATFORM_USED] ) == Image_GetCardMemoryAmount( loadDef"
                    "->flags, static_cast< GfxPixelFormat >( loadDef->format ), loadDef->dimensions[0], loadDef->dimensions[1], loa"
                    "dDef->dimensions[2] ))",
                    image->name);
            if (image->texture.basemap)
                MyAssertHandler(
                    ".\\r_image.cpp",
                    789,
                    1,
                    "%s\n\t(image->name) = %s",
                    "(image->texture.basemap == 0)",
                    image->name);
            if (!image->delayLoadPixels)
            {
                externalDataSize = image->cardMemory.platform[0];
                image->cardMemory.platform[0] = 0;
                image->cardMemory.platform[1] = 0;
                if (!Image_LoadFromFile(image))
                    Com_Error(ERR_DROP, "Couldn't load image '%s'\n", image->name);
                DB_LoadedExternalData(externalDataSize);
            }
        }
    }
}

GfxImage *__cdecl Image_FindExisting(const char *name)
{
    if (IsFastFileLoad())
        return Image_FindExisting_FastFile(name);
    else
        return Image_FindExisting_LoadObj(name);
}

GfxImage *__cdecl Image_FindExisting_FastFile(const char *name)
{
    return DB_FindXAssetHeader(ASSET_TYPE_IMAGE, name).image;
}

GfxImage *__cdecl Image_Register(const char *imageName, uint8_t semantic, int imageTrack)
{
    if (IsFastFileLoad())
        return (GfxImage *)Image_Register_FastFile(imageName);
    else
        return Image_Register_LoadObj((char*)imageName, semantic, imageTrack);
}

GfxImage *__cdecl Image_Register_FastFile(const char *imageName)
{
    return Image_FindExisting(imageName);
}

char __cdecl Image_LoadFromFile(GfxImage *image)
{
    return Image_LoadFromFileWithReader(image, FS_FOpenFileReadDatabase);
}

char __cdecl Image_ValidateHeader(GfxImageFileHeader *imageFile, const char *filepath)
{
    if (imageFile->tag[0] == 73 && imageFile->tag[1] == 87 && imageFile->tag[2] == 105)
    {
        if (imageFile->version == 6)
        {
            return 1;
        }
        else
        {
            Com_PrintError(CON_CHANNEL_GFX, "ERROR: image '%s' is version %i but should be version %i\n", filepath, imageFile->version, 6);
            return 0;
        }
    }
    else
    {
        Com_PrintError(CON_CHANNEL_GFX, "ERROR: image '%s' is not an IW image\n", filepath);
        return 0;
    }
}

uint32_t __cdecl Image_CountMipmaps(char imageFlags, uint32_t width, uint32_t height, uint32_t depth)
{
    uint32_t mipRes; // [esp+0h] [ebp-8h]
    uint32_t mipCount; // [esp+4h] [ebp-4h]

    if ((imageFlags & IMG_FLAG_NOMIPMAPS) != 0)
        return 1;
    mipCount = 1;
    for (mipRes = 1; mipRes < width || mipRes < height || mipRes < depth; mipRes *= 2)
        ++mipCount;
    return mipCount;
}
uint32_t __cdecl Image_CountMipmapsForFile(const GfxImageFileHeader *fileHeader)
{
    return Image_CountMipmaps(
        fileHeader->flags,
        fileHeader->dimensions[0],
        fileHeader->dimensions[1],
        fileHeader->dimensions[2]);
}

void __cdecl Image_UploadData(const GfxImage *image,_D3DFORMAT format,_D3DCUBEMAP_FACES face,uint32_t mipLevel,uint8_t *src)
{
#ifdef __SWITCH__
    if(image->mapType!=MAPTYPE_CUBE||!mipLevel||gfxMetrics.canMipCubemaps) R_GLUploadTexture(image,format,face,mipLevel,src);
#else
    if(image->mapType!=MAPTYPE_CUBE||!mipLevel||gfxMetrics.canMipCubemaps){if(image->mapType==MAPTYPE_3D)Image_Upload3D_CopyData_PC(image,format,mipLevel,src);else Image_Upload2D_CopyData_PC(image,format,face,mipLevel,src);}
#endif
}

void __cdecl Image_LoadWhite(GfxImage *image)
{
    Image_LoadSolid(image, 0xFFu, 0xFFu, 0xFFu, 0xFFu);
}

void __cdecl Image_LoadSolid(
    GfxImage *image,
    uint8_t r,
    uint8_t g,
    uint8_t b,
    uint8_t a)
{
    uint8_t pic[4]; // [esp+4h] [ebp-4h] BYREF

    *(uint32_t *)pic = (a << 24) | b | (g << 8) | (r << 16);
    Image_Generate2D(image, pic, 1, 1, D3DFMT_A8R8G8B8);
}

void __cdecl Image_LoadBlack(GfxImage *image)
{
    Image_LoadSolid(image, 0, 0, 0, 0xFFu);
}

void __cdecl Image_LoadGray(GfxImage *image)
{
    Image_LoadSolid(image, 0x80u, 0x80u, 0x80u, 0x80u);
}

void __cdecl Image_LoadIdentityNormalMap(GfxImage *image)
{
    Image_LoadSolid(image, 0x80u, 0x80u, 0xFFu, 0x80u);
}

void __cdecl Image_LoadBlack3D(GfxImage *image)
{
    uint8_t pic[4]; // [esp+4h] [ebp-4h] BYREF

    *(uint32_t *)pic = -16777216;
    Image_Generate3D(image, pic, 1, 1, 1, D3DFMT_A8R8G8B8);
}

void __cdecl Image_LoadBlackCube(GfxImage *image)
{
    const uint8_t *pic[6][15]; // [esp+4h] [ebp-170h] BYREF
    uint8_t pixel[4]; // [esp+170h] [ebp-4h] BYREF

    *(uint32_t *)pixel = -16777216;
    pic[0][0] = pixel;
    pic[1][0] = pixel;
    pic[2][0] = pixel;
    pic[3][0] = pixel;
    pic[4][0] = pixel;
    pic[5][0] = pixel;
    Image_GenerateCube(image, pic, 1, D3DFMT_A8R8G8B8, 1u);
}

void __cdecl Image_LoadPixelCostColorCode(GfxImage *image)
{
    uint8_t pic[257][4]; // [esp+0h] [ebp-408h] BYREF

    RB_PixelCost_BuildColorCodeMap(pic, 256);
    Image_Generate2D(image, pic[0], 256, 1, D3DFMT_X8R8G8B8);
}

GfxImage *__cdecl Image_LoadBuiltin(char *name, uint8_t semantic, uint8_t imageTrack)
{
    GfxImage *image; // [esp+14h] [ebp-8h]
    uint32_t tableIndex; // [esp+18h] [ebp-4h]

    for (tableIndex = 0; ; ++tableIndex)
    {
        if (tableIndex >= 8)
        {
            Com_PrintError(CON_CHANNEL_GFX, "ERROR: Unknown built-in image '%s'", name);
            return 0;
        }
        if (!strcmp(constructorTable[tableIndex].name, name))
            break;
    }

    image = Image_Alloc(name, IMG_CATEGORY_AUTO_GENERATED, semantic, imageTrack);
    iassert(image);
    constructorTable[tableIndex].LoadCallback(image);
    return image;
}

void __cdecl Image_Construct(
    char *name,
    int nameSize,
    uint8_t category,
    uint8_t semantic,
    uint8_t imageTrack,
    GfxImage *image)
{
    iassert(name);
    iassert(nameSize > 0);
    iassert(image);
    {
        PROF_SCOPED("R_memcpy");
        memcpy((uint8_t *)image->name, (uint8_t *)name, nameSize);
    }
    iassert(category != IMG_CATEGORY_UNKNOWN);
    image->category = category;
    image->semantic = semantic;
    iassert(image->noPicmip == false);
    iassert(image->picmip.platform[PICMIP_PLATFORM_USED] == 0);
    iassert(image->picmip.platform[PICMIP_PLATFORM_MINSPEC] == 0);
    image->track = imageTrack;
}
int __cdecl Image_GetAvailableHashLocation(const char *name)
{
    int hashIndex; // [esp+0h] [ebp-4h]

    // idb Image_Alloc @0x5128b0: `& 0x7FFF` (editor 32768-slot table). See IMAGE_HASH_TABLE_MASK.
    for (hashIndex = R_HashAssetName(name) & IMAGE_HASH_TABLE_MASK;
        imageGlobals.imageHashTable[hashIndex];
        hashIndex = ((_WORD)hashIndex + 1) & IMAGE_HASH_TABLE_MASK)
    {
        ;
    }
    return hashIndex;
}
GfxImage *__cdecl Image_Alloc(
    char *name,
    uint8_t category,
    uint8_t semantic,
    uint8_t imageTrack)
{
    uint32_t v5; // [esp+0h] [ebp-20h]
    GfxImage *image; // [esp+10h] [ebp-10h]

    iassert( name );
    v5 = strlen(name);
    image = (GfxImage *)Hunk_Alloc(v5 + 37, "Image_Alloc", 22);
    iassert( image );
    image->name = (const char *)&image[1];
    Image_Construct(name, v5 + 1, category, semantic, imageTrack, image);
    imageGlobals.imageHashTable[Image_GetAvailableHashLocation(name)] = image;
    return image;
}
void __cdecl Image_Free(GfxImage *image)
{
    Image_Release(image);
}

IDirect3DSurface9 *__cdecl Image_GetSurface(GfxImage *image)
{
    iassert(image&&image->mapType==MAPTYPE_2D&&image->texture.map);
#ifdef __SWITCH__
    auto *s = new IDirect3DSurface9;
    s->texture = image->texture.map;
    s->texture->AddRef();
    s->level = 0;
    return s;
#else
    IDirect3DSurface9 *s=nullptr;HRESULT hr=image->texture.map->GetSurfaceLevel(0,&s);if(hr<0)Com_Error(ERR_FATAL,"GetSurfaceLevel failed: %s",R_ErrorDescription(hr));return s;
#endif
}

void __cdecl R_SetPicmip()
{
#ifdef __SWITCH__
    R_SwitchPicmipTrace("[SWITCH PICMIP] ENTER\n");
#endif
    uint32_t texMemInMegs; // [esp+0h] [ebp-10h]
    uint32_t sysMemInMegs; // [esp+4h] [ebp-Ch]
    bool cappedPicmip; // [esp+Bh] [ebp-5h]
    int minPicmip; // [esp+Ch] [ebp-4h]

#ifdef __SWITCH__
    R_SwitchPicmipTrace("[SWITCH PICMIP] BEFORE DX ASSERT\n");
#endif
    iassert( dx.device );
#ifdef __SWITCH__
    R_SwitchPicmipTrace("[SWITCH PICMIP] AFTER DX ASSERT\n");
#endif
    texMemInMegs = R_AvailableTextureMemory();
#ifdef __SWITCH__
    R_SwitchPicmipTrace("[SWITCH PICMIP] AFTER TEXMEM\n");
#endif
#ifdef __SWITCH__
    // sys_sysMB is not registered by the Switch port. Do not enter the shared
    // dvar read lock here; use the same 2048 MB budget as the Switch texture budget.
    sysMemInMegs = texMemInMegs;
#else
    sysMemInMegs = Dvar_GetInt("sys_sysMB");
#endif
#ifdef __SWITCH__
    R_SwitchPicmipTrace("[SWITCH PICMIP] BEFORE REFLECTION ASSERT\n");
#endif
    iassert( r_reflectionProbeGenerate );
#ifdef __SWITCH__
    R_SwitchPicmipTrace("[SWITCH PICMIP] AFTER REFLECTION ASSERT\n");
#endif
    if (r_reflectionProbeGenerate->current.enabled)
    {
#ifdef __SWITCH__
        R_SwitchPicmipTrace("[SWITCH PICMIP] REFLECTION ENABLED\n");
#endif
        Com_Printf(CON_CHANNEL_GFX, "Picmip is set to lowest quality for generating reflections.\n");
        imageGlobals.picmip = 2;
        imageGlobals.picmipBump = 2;
        imageGlobals.picmipSpec = 2;
    }
    else
    {
        if (r_picmip_manual->current.enabled)
        {
            Com_Printf(CON_CHANNEL_GFX, "Picmip is set manually.\n");
            imageGlobals.picmip = r_picmip->current.integer;
            imageGlobals.picmipBump = r_picmip_bump->current.integer;
            imageGlobals.picmipSpec = r_picmip_spec->current.integer;
        }
        else
        {
            Com_Printf(CON_CHANNEL_GFX, "Texture detail is set automatically.\n");
#ifdef __SWITCH__
            R_SwitchPicmipTrace("[SWITCH PICMIP] AFTER DETAIL LOG\n");
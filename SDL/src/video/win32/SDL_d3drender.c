/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2006 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

#if SDL_VIDEO_RENDER_D3D

#include "SDL_win32video.h"
#include "../SDL_yuv_sw_c.h"

/* Direct3D renderer implementation */

static SDL_Renderer *SDL_D3D_CreateRenderer(SDL_Window * window,
                                            Uint32 flags);
static int SDL_D3D_CreateTexture(SDL_Renderer * renderer,
                                 SDL_Texture * texture);
static int SDL_D3D_QueryTexturePixels(SDL_Renderer * renderer,
                                      SDL_Texture * texture, void **pixels,
                                      int *pitch);
static int SDL_D3D_SetTexturePalette(SDL_Renderer * renderer,
                                     SDL_Texture * texture,
                                     const SDL_Color * colors, int firstcolor,
                                     int ncolors);
static int SDL_D3D_GetTexturePalette(SDL_Renderer * renderer,
                                     SDL_Texture * texture,
                                     SDL_Color * colors, int firstcolor,
                                     int ncolors);
static int SDL_D3D_UpdateTexture(SDL_Renderer * renderer,
                                 SDL_Texture * texture, const SDL_Rect * rect,
                                 const void *pixels, int pitch);
static int SDL_D3D_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                               const SDL_Rect * rect, int markDirty,
                               void **pixels, int *pitch);
static void SDL_D3D_UnlockTexture(SDL_Renderer * renderer,
                                  SDL_Texture * texture);
static void SDL_D3D_DirtyTexture(SDL_Renderer * renderer,
                                 SDL_Texture * texture, int numrects,
                                 const SDL_Rect * rects);
static void SDL_D3D_SelectRenderTexture(SDL_Renderer * renderer,
                                        SDL_Texture * texture);
static int SDL_D3D_RenderFill(SDL_Renderer * renderer, const SDL_Rect * rect,
                              Uint32 color);
static int SDL_D3D_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
                              const SDL_Rect * srcrect,
                              const SDL_Rect * dstrect, int blendMode,
                              int scaleMode);
static int SDL_D3D_RenderReadPixels(SDL_Renderer * renderer,
                                    const SDL_Rect * rect, void *pixels,
                                    int pitch);
static int SDL_D3D_RenderWritePixels(SDL_Renderer * renderer,
                                     const SDL_Rect * rect,
                                     const void *pixels, int pitch);
static void SDL_D3D_RenderPresent(SDL_Renderer * renderer);
static void SDL_D3D_DestroyTexture(SDL_Renderer * renderer,
                                   SDL_Texture * texture);
static void SDL_D3D_DestroyRenderer(SDL_Renderer * renderer);


SDL_RenderDriver SDL_D3D_RenderDriver = {
    SDL_D3D_CreateRenderer,
    {
     "d3d",
     (                          //SDL_Renderer_Minimal |
         SDL_Renderer_SingleBuffer | SDL_Renderer_PresentCopy |
         SDL_Renderer_PresentFlip2 | SDL_Renderer_PresentFlip3 |
         SDL_Renderer_PresentDiscard | SDL_Renderer_RenderTarget),
     (SDL_TextureBlendMode_None |
      SDL_TextureBlendMode_Mask | SDL_TextureBlendMode_Blend),
     (SDL_TextureScaleMode_None | SDL_TextureScaleMode_Fast),
     11,
     {
      SDL_PixelFormat_Index8,
      SDL_PixelFormat_RGB555,
      SDL_PixelFormat_RGB565,
      SDL_PixelFormat_RGB888,
      SDL_PixelFormat_BGR888,
      SDL_PixelFormat_ARGB8888,
      SDL_PixelFormat_RGBA8888,
      SDL_PixelFormat_ABGR8888,
      SDL_PixelFormat_BGRA8888,
      SDL_PixelFormat_YUY2,
      SDL_PixelFormat_UYVY},
     0,
     0}
};

typedef struct
{
    IDirect3DDevice9 *device;
    SDL_bool beginScene;
} SDL_D3D_RenderData;

typedef struct
{
    SDL_SW_YUVTexture *yuv;
} SDL_D3D_TextureData;

static void
D3D_SetError(const char *prefix, HRESULT result)
{
    const char *error;

    switch (result) {
    case D3DERR_WRONGTEXTUREFORMAT:
        error = "WRONGTEXTUREFORMAT";
        break;
    case D3DERR_UNSUPPORTEDCOLOROPERATION:
        error = "UNSUPPORTEDCOLOROPERATION";
        break;
    case D3DERR_UNSUPPORTEDCOLORARG:
        error = "UNSUPPORTEDCOLORARG";
        break;
    case D3DERR_UNSUPPORTEDALPHAOPERATION:
        error = "UNSUPPORTEDALPHAOPERATION";
        break;
    case D3DERR_UNSUPPORTEDALPHAARG:
        error = "UNSUPPORTEDALPHAARG";
        break;
    case D3DERR_TOOMANYOPERATIONS:
        error = "TOOMANYOPERATIONS";
        break;
    case D3DERR_CONFLICTINGTEXTUREFILTER:
        error = "CONFLICTINGTEXTUREFILTER";
        break;
    case D3DERR_UNSUPPORTEDFACTORVALUE:
        error = "UNSUPPORTEDFACTORVALUE";
        break;
    case D3DERR_CONFLICTINGRENDERSTATE:
        error = "CONFLICTINGRENDERSTATE";
        break;
    case D3DERR_UNSUPPORTEDTEXTUREFILTER:
        error = "UNSUPPORTEDTEXTUREFILTER";
        break;
    case D3DERR_CONFLICTINGTEXTUREPALETTE:
        error = "CONFLICTINGTEXTUREPALETTE";
        break;
    case D3DERR_DRIVERINTERNALERROR:
        error = "DRIVERINTERNALERROR";
        break;
    case D3DERR_NOTFOUND:
        error = "NOTFOUND";
        break;
    case D3DERR_MOREDATA:
        error = "MOREDATA";
        break;
    case D3DERR_DEVICELOST:
        error = "DEVICELOST";
        break;
    case D3DERR_DEVICENOTRESET:
        error = "DEVICENOTRESET";
        break;
    case D3DERR_NOTAVAILABLE:
        error = "NOTAVAILABLE";
        break;
    case D3DERR_OUTOFVIDEOMEMORY:
        error = "OUTOFVIDEOMEMORY";
        break;
    case D3DERR_INVALIDDEVICE:
        error = "INVALIDDEVICE";
        break;
    case D3DERR_INVALIDCALL:
        error = "INVALIDCALL";
        break;
    case D3DERR_DRIVERINVALIDCALL:
        error = "DRIVERINVALIDCALL";
        break;
    case D3DERR_WASSTILLDRAWING:
        error = "WASSTILLDRAWING";
        break;
    default:
        error = "UNKNOWN";
        break;
    }
    SDL_SetError("%s: %s", prefix, error);
}

static void
UpdateYUVTextureData(SDL_Texture * texture)
{
    SDL_D3D_TextureData *data = (SDL_D3D_TextureData *) texture->driverdata;
    SDL_Rect rect;

    rect.x = 0;
    rect.y = 0;
    rect.w = texture->w;
    rect.h = texture->h;
    //SDL_SW_CopyYUVToRGB(data->yuv, &rect, data->format, texture->w,
    //                    texture->h, data->pixels, data->pitch);
}

void
D3D_AddRenderDriver(_THIS)
{
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;

    if (data->d3d) {
        SDL_AddRenderDriver(0, &SDL_D3D_RenderDriver);
    }
}

SDL_Renderer *
SDL_D3D_CreateRenderer(SDL_Window * window, Uint32 flags)
{
    SDL_VideoDisplay *display = SDL_GetDisplayFromWindow(window);
    SDL_VideoData *videodata = (SDL_VideoData *) display->device->driverdata;
    SDL_WindowData *windowdata = (SDL_WindowData *) window->driverdata;
    SDL_Renderer *renderer;
    SDL_D3D_RenderData *data;
    HRESULT result;
    D3DPRESENT_PARAMETERS pparams;

    renderer = (SDL_Renderer *) SDL_malloc(sizeof(*renderer));
    if (!renderer) {
        SDL_OutOfMemory();
        return NULL;
    }
    SDL_zerop(renderer);

    data = (SDL_D3D_RenderData *) SDL_malloc(sizeof(*data));
    if (!data) {
        SDL_D3D_DestroyRenderer(renderer);
        SDL_OutOfMemory();
        return NULL;
    }
    SDL_zerop(data);

    renderer->CreateTexture = SDL_D3D_CreateTexture;
    renderer->QueryTexturePixels = SDL_D3D_QueryTexturePixels;
    renderer->SetTexturePalette = SDL_D3D_SetTexturePalette;
    renderer->GetTexturePalette = SDL_D3D_GetTexturePalette;
    renderer->UpdateTexture = SDL_D3D_UpdateTexture;
    renderer->LockTexture = SDL_D3D_LockTexture;
    renderer->UnlockTexture = SDL_D3D_UnlockTexture;
    renderer->DirtyTexture = SDL_D3D_DirtyTexture;
    renderer->SelectRenderTexture = SDL_D3D_SelectRenderTexture;
    renderer->RenderFill = SDL_D3D_RenderFill;
    renderer->RenderCopy = SDL_D3D_RenderCopy;
    renderer->RenderReadPixels = SDL_D3D_RenderReadPixels;
    renderer->RenderWritePixels = SDL_D3D_RenderWritePixels;
    renderer->RenderPresent = SDL_D3D_RenderPresent;
    renderer->DestroyTexture = SDL_D3D_DestroyTexture;
    renderer->DestroyRenderer = SDL_D3D_DestroyRenderer;
    renderer->info = SDL_D3D_RenderDriver.info;
    renderer->window = window->id;
    renderer->driverdata = data;

    renderer->info.flags = SDL_Renderer_RenderTarget;

    SDL_zero(pparams);
    pparams.BackBufferWidth = window->w;
    pparams.BackBufferHeight = window->h;
    pparams.BackBufferFormat = D3DFMT_UNKNOWN;  /* FIXME */
    if (flags & SDL_Renderer_PresentFlip2) {
        pparams.BackBufferCount = 2;
        pparams.SwapEffect = D3DSWAPEFFECT_FLIP;
    } else if (flags & SDL_Renderer_PresentFlip3) {
        pparams.BackBufferCount = 3;
        pparams.SwapEffect = D3DSWAPEFFECT_FLIP;
    } else if (flags & SDL_Renderer_PresentCopy) {
        pparams.BackBufferCount = 1;
        pparams.SwapEffect = D3DSWAPEFFECT_COPY;
    } else {
        pparams.BackBufferCount = 1;
        pparams.SwapEffect = D3DSWAPEFFECT_DISCARD;
    }
    if (window->flags & SDL_WINDOW_FULLSCREEN) {
        pparams.Windowed = FALSE;
    } else {
        pparams.Windowed = TRUE;
    }
    pparams.FullScreen_RefreshRateInHz = 0;     /* FIXME */

    result = IDirect3D9_CreateDevice(videodata->d3d, D3DADAPTER_DEFAULT,        /* FIXME */
                                     D3DDEVTYPE_HAL,
                                     windowdata->hwnd,
                                     D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                     &pparams, &data->device);
    if (FAILED(result)) {
        SDL_D3D_DestroyRenderer(renderer);
        D3D_SetError("CreateDevice()", result);
        return NULL;
    }
    data->beginScene = SDL_TRUE;

    return renderer;
}

static int
SDL_D3D_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    SDL_D3D_RenderData *renderdata =
        (SDL_D3D_RenderData *) renderer->driverdata;
    SDL_Window *window = SDL_GetWindowFromID(renderer->window);
    SDL_VideoDisplay *display = SDL_GetDisplayFromWindow(window);
    SDL_D3D_TextureData *data;

    data = (SDL_D3D_TextureData *) SDL_malloc(sizeof(*data));
    if (!data) {
        SDL_OutOfMemory();
        return -1;
    }
    SDL_zerop(data);

    texture->driverdata = data;

    return 0;
}

static int
SDL_D3D_QueryTexturePixels(SDL_Renderer * renderer, SDL_Texture * texture,
                           void **pixels, int *pitch)
{
    SDL_D3D_TextureData *data = (SDL_D3D_TextureData *) texture->driverdata;

    if (data->yuv) {
        return SDL_SW_QueryYUVTexturePixels(data->yuv, pixels, pitch);
    } else {
        return 0;
    }
}

static int
SDL_D3D_SetTexturePalette(SDL_Renderer * renderer, SDL_Texture * texture,
                          const SDL_Color * colors, int firstcolor,
                          int ncolors)
{
    SDL_D3D_RenderData *renderdata =
        (SDL_D3D_RenderData *) renderer->driverdata;
    SDL_D3D_TextureData *data = (SDL_D3D_TextureData *) texture->driverdata;

    if (data->yuv) {
        SDL_SetError("YUV textures don't have a palette");
        return -1;
    } else {
        return 0;
    }
}

static int
SDL_D3D_GetTexturePalette(SDL_Renderer * renderer, SDL_Texture * texture,
                          SDL_Color * colors, int firstcolor, int ncolors)
{
    SDL_D3D_TextureData *data = (SDL_D3D_TextureData *) texture->driverdata;

    if (data->yuv) {
        SDL_SetError("YUV textures don't have a palette");
        return -1;
    } else {
        return 0;
    }
}

static int
SDL_D3D_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                      const SDL_Rect * rect, const void *pixels, int pitch)
{
    SDL_D3D_TextureData *data = (SDL_D3D_TextureData *) texture->driverdata;

    if (data->yuv) {
        if (SDL_SW_UpdateYUVTexture(data->yuv, rect, pixels, pitch) < 0) {
            return -1;
        }
        UpdateYUVTextureData(texture);
        return 0;
    } else {
        SDL_D3D_RenderData *renderdata =
            (SDL_D3D_RenderData *) renderer->driverdata;

        return 0;
    }
}

static int
SDL_D3D_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                    const SDL_Rect * rect, int markDirty, void **pixels,
                    int *pitch)
{
    SDL_D3D_TextureData *data = (SDL_D3D_TextureData *) texture->driverdata;

    if (data->yuv) {
        return SDL_SW_LockYUVTexture(data->yuv, rect, markDirty, pixels,
                                     pitch);
    } else {
        return -1;
    }
}

static void
SDL_D3D_UnlockTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    SDL_D3D_TextureData *data = (SDL_D3D_TextureData *) texture->driverdata;

    if (data->yuv) {
        SDL_SW_UnlockYUVTexture(data->yuv);
        UpdateYUVTextureData(texture);
    }
}

static void
SDL_D3D_DirtyTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                     int numrects, const SDL_Rect * rects)
{
}

static void
SDL_D3D_SelectRenderTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    SDL_D3D_RenderData *data = (SDL_D3D_RenderData *) renderer->driverdata;
}

static int
SDL_D3D_RenderFill(SDL_Renderer * renderer, const SDL_Rect * rect,
                   Uint32 color)
{
    SDL_D3D_RenderData *data = (SDL_D3D_RenderData *) renderer->driverdata;
    HRESULT result;

    if (data->beginScene) {
        IDirect3DDevice9_BeginScene(data->device);
        data->beginScene = SDL_FALSE;
    }

    result =
        IDirect3DDevice9_Clear(data->device, 0, NULL, D3DCLEAR_TARGET,
                               (D3DCOLOR) color, 1.0f, 0);
    if (FAILED(result)) {
        D3D_SetError("Clear()", result);
        return -1;
    }
    return 0;
}

static int
SDL_D3D_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
                   const SDL_Rect * srcrect, const SDL_Rect * dstrect,
                   int blendMode, int scaleMode)
{
    SDL_D3D_RenderData *data = (SDL_D3D_RenderData *) renderer->driverdata;
    SDL_D3D_TextureData *texturedata =
        (SDL_D3D_TextureData *) texture->driverdata;

    if (data->beginScene) {
        IDirect3DDevice9_BeginScene(data->device);
        data->beginScene = SDL_FALSE;
    }
    return 0;
}

static int
SDL_D3D_RenderReadPixels(SDL_Renderer * renderer, const SDL_Rect * rect,
                         void *pixels, int pitch)
{
    SDL_D3D_RenderData *data = (SDL_D3D_RenderData *) renderer->driverdata;

    return 0;
}

static int
SDL_D3D_RenderWritePixels(SDL_Renderer * renderer, const SDL_Rect * rect,
                          const void *pixels, int pitch)
{
    SDL_D3D_RenderData *data = (SDL_D3D_RenderData *) renderer->driverdata;

    return 0;
}

static void
SDL_D3D_RenderPresent(SDL_Renderer * renderer)
{
    SDL_D3D_RenderData *data = (SDL_D3D_RenderData *) renderer->driverdata;
    HRESULT result;

    if (!data->beginScene) {
        IDirect3DDevice9_EndScene(data->device);
        data->beginScene = SDL_TRUE;
    }

    result = IDirect3DDevice9_Present(data->device, NULL, NULL, NULL, NULL);
    if (FAILED(result)) {
        D3D_SetError("Present()", result);
    }
}

static void
SDL_D3D_DestroyTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    SDL_D3D_TextureData *data = (SDL_D3D_TextureData *) texture->driverdata;

    if (!data) {
        return;
    }
    SDL_free(data);
    texture->driverdata = NULL;
}

void
SDL_D3D_DestroyRenderer(SDL_Renderer * renderer)
{
    SDL_D3D_RenderData *data = (SDL_D3D_RenderData *) renderer->driverdata;

    if (data) {
        if (data->device) {
            IDirect3DDevice9_Release(data->device);
        }
        SDL_free(data);
    }
    SDL_free(renderer);
}

#endif /* SDL_VIDEO_RENDER_D3D */

/* vi: set ts=4 sw=4 expandtab: */

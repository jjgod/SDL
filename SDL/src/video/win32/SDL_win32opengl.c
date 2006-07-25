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

#include "SDL_win32video.h"

/* WGL implementation of SDL OpenGL support */

#if SDL_VIDEO_OPENGL
#include "SDL_opengl.h"

#define DEFAULT_OPENGL_PATH "OPENGL32.DLL"


int
WIN_GL_LoadLibrary(_THIS, const char *path)
{
    LPTSTR wpath;
    HANDLE handle;

    if (_this->gl_config.driver_loaded) {
        if (path) {
            SDL_SetError("OpenGL library already loaded");
            return -1;
        } else {
            ++_this->gl_config.driver_loaded;
            return 0;
        }
    }
    if (path == NULL) {
        path = DEFAULT_OPENGL_PATH;
    }
    wpath = WIN_UTF8ToString(path);
    handle = LoadLibrary(wpath);
    SDL_free(wpath);
    if (!handle) {
        char message[1024];
        SDL_snprintf(message, SDL_arraysize(message), "LoadLibrary(\"%s\")",
                     path);
        WIN_SetError(message);
        return -1;
    }

    /* Load function pointers */
    _this->gl_data->wglGetProcAddress = (void *(WINAPI *) (const char *))
        GetProcAddress(handle, "wglGetProcAddress");
    _this->gl_data->wglCreateContext = (HGLRC(WINAPI *) (HDC))
        GetProcAddress(handle, "wglCreateContext");
    _this->gl_data->wglDeleteContext = (BOOL(WINAPI *) (HGLRC))
        GetProcAddress(handle, "wglDeleteContext");
    _this->gl_data->wglMakeCurrent = (BOOL(WINAPI *) (HDC, HGLRC))
        GetProcAddress(handle, "wglMakeCurrent");
    _this->gl_data->wglSwapIntervalEXT = (void (WINAPI *) (int))
        GetProcAddress(handle, "wglSwapIntervalEXT");
    _this->gl_data->wglGetSwapIntervalEXT = (int (WINAPI *) (void))
        GetProcAddress(handle, "wglGetSwapIntervalEXT");

    if (!_this->gl_data->wglGetProcAddress ||
        !_this->gl_data->wglCreateContext ||
        !_this->gl_data->wglDeleteContext ||
        !_this->gl_data->wglMakeCurrent) {
        SDL_SetError("Could not retrieve OpenGL functions");
        FreeLibrary(handle);
        return -1;
    }

    _this->gl_config.dll_handle = handle;
    SDL_strlcpy(_this->gl_config.driver_path, path,
                SDL_arraysize(_this->gl_config.driver_path));
    _this->gl_config.driver_loaded = 1;
    return 0;
}

void *
WIN_GL_GetProcAddress(_THIS, const char *proc)
{
    void *func;

    /* This is to pick up extensions */
    func = _this->gl_data->wglGetProcAddress(proc);
    if (!func) {
        /* This is probably a normal GL function */
        func = GetProcAddress(_this->gl_config.dll_handle, proc);
    }
    return func;
}

static void
WIN_GL_UnloadLibrary(_THIS)
{
    if (_this->gl_config.driver_loaded > 0) {
        if (--_this->gl_config.driver_loaded > 0) {
            return;
        }
        FreeLibrary((HMODULE) _this->gl_config.dll_handle);
        _this->gl_config.dll_handle = NULL;
    }
}

static void
WIN_GL_SetupPixelFormat(_THIS, PIXELFORMATDESCRIPTOR * pfd)
{
    SDL_zerop(pfd);
    pfd->nSize = sizeof(*pfd);
    pfd->nVersion = 1;
    pfd->dwFlags = (PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL);
    if (_this->gl_config.double_buffer) {
        pfd->dwFlags |= PFD_DOUBLEBUFFER;
    }
    if (_this->gl_config.stereo) {
        pfd->dwFlags |= PFD_STEREO;
    }
    pfd->iLayerType = PFD_MAIN_PLANE;
    pfd->iPixelType = PFD_TYPE_RGBA;
    pfd->cRedBits = _this->gl_config.red_size;
    pfd->cGreenBits = _this->gl_config.green_size;
    pfd->cBlueBits = _this->gl_config.blue_size;
    pfd->cAlphaBits = _this->gl_config.alpha_size;
    if (_this->gl_config.buffer_size) {
        pfd->cColorBits =
            _this->gl_config.buffer_size - _this->gl_config.alpha_size;
    } else {
        pfd->cColorBits = (pfd->cRedBits + pfd->cGreenBits + pfd->cBlueBits);
    }
    pfd->cAccumRedBits = _this->gl_config.accum_red_size;
    pfd->cAccumGreenBits = _this->gl_config.accum_green_size;
    pfd->cAccumBlueBits = _this->gl_config.accum_blue_size;
    pfd->cAccumAlphaBits = _this->gl_config.accum_alpha_size;
    pfd->cAccumBits =
        (pfd->cAccumRedBits + pfd->cAccumGreenBits + pfd->cAccumBlueBits +
         pfd->cAccumAlphaBits);
    pfd->cDepthBits = _this->gl_config.depth_size;
    pfd->cStencilBits = _this->gl_config.stencil_size;
}

static SDL_bool
HasExtension(const char *extension, const char *extensions)
{
    const char *start;
    const char *where, *terminator;

    /* Extension names should not have spaces. */
    where = SDL_strchr(extension, ' ');
    if (where || *extension == '\0')
        return SDL_FALSE;

    if (!extensions)
        return SDL_FALSE;

    /* It takes a bit of care to be fool-proof about parsing the
     * OpenGL extensions string. Don't be fooled by sub-strings,
     * etc. */

    start = extensions;

    for (;;) {
        where = SDL_strstr(start, extension);
        if (!where)
            break;

        terminator = where + SDL_strlen(extension);
        if (where == start || *(where - 1) == ' ')
            if (*terminator == ' ' || *terminator == '\0')
                return SDL_TRUE;

        start = terminator;
    }
    return SDL_FALSE;
}

static void
WIN_GL_InitExtensions(_THIS)
{
    HWND hwnd;
    HDC hdc;
    PIXELFORMATDESCRIPTOR pfd;
    int pixel_format;
    HGLRC hglrc;
    const char *(WINAPI * wglGetExtensionsStringARB) (HDC) = 0;
    const char *extensions;

    hwnd =
        CreateWindow(SDL_Appname, SDL_Appname, (WS_POPUP | WS_DISABLED), 0, 0,
                     10, 10, NULL, NULL, SDL_Instance, NULL);
    WIN_PumpEvents(_this);

    hdc = GetDC(hwnd);

    WIN_GL_SetupPixelFormat(_this, &pfd);
    pixel_format = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, pixel_format, &pfd);

    hglrc = _this->gl_data->wglCreateContext(hdc);
    if (hglrc) {
        _this->gl_data->wglMakeCurrent(hdc, hglrc);
    }

    wglGetExtensionsStringARB = (const char *(WINAPI *) (HDC))
        _this->gl_data->wglGetProcAddress("wglGetExtensionsStringARB");
    if (wglGetExtensionsStringARB) {
        extensions = wglGetExtensionsStringARB(hdc);
    } else {
        extensions = NULL;
    }

    /* Check for WGL_ARB_pixel_format */
    _this->gl_data->WGL_ARB_pixel_format = 0;
    if (HasExtension("WGL_ARB_pixel_format", extensions)) {
        _this->gl_data->wglChoosePixelFormatARB = (BOOL(WINAPI *)
                                                   (HDC, const int *,
                                                    const FLOAT *, UINT,
                                                    int *, UINT *))
            WIN_GL_GetProcAddress(_this, "wglChoosePixelFormatARB");
        _this->gl_data->wglGetPixelFormatAttribivARB =
            (BOOL(WINAPI *) (HDC, int, int, UINT, const int *, int *))
            WIN_GL_GetProcAddress(_this, "wglGetPixelFormatAttribivARB");

        if ((_this->gl_data->wglChoosePixelFormatARB != NULL) &&
            (_this->gl_data->wglGetPixelFormatAttribivARB != NULL)) {
            _this->gl_data->WGL_ARB_pixel_format = 1;
        }
    }

    /* Check for WGL_EXT_swap_control */
    if (HasExtension("WGL_EXT_swap_control", extensions)) {
        _this->gl_data->wglSwapIntervalEXT =
            WIN_GL_GetProcAddress(_this, "wglSwapIntervalEXT");
        _this->gl_data->wglGetSwapIntervalEXT =
            WIN_GL_GetProcAddress(_this, "wglGetSwapIntervalEXT");
    }

    if (hglrc) {
        _this->gl_data->wglMakeCurrent(NULL, NULL);
        _this->gl_data->wglDeleteContext(hglrc);
    }
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);
    WIN_PumpEvents(_this);
}

static void
WIN_GL_Shutdown(_THIS)
{
    if (!_this->gl_data || (--_this->gl_data->initialized > 0)) {
        return;
    }

    WIN_GL_UnloadLibrary(_this);

    SDL_free(_this->gl_data);
    _this->gl_data = NULL;
}

static int
WIN_GL_Initialize(_THIS)
{
    if (_this->gl_data) {
        ++_this->gl_data->initialized;
        return 0;
    }

    _this->gl_data =
        (struct SDL_GLDriverData *) SDL_calloc(1,
                                               sizeof(struct
                                                      SDL_GLDriverData));
    if (!_this->gl_data) {
        SDL_OutOfMemory();
        return -1;
    }
    _this->gl_data->initialized = 1;

    if (WIN_GL_LoadLibrary(_this, NULL) < 0) {
        return -1;
    }

    /* Initialize extensions */
    WIN_GL_InitExtensions(_this);

    return 0;
}

int
WIN_GL_SetupWindow(_THIS, SDL_Window * window)
{
    HDC hdc = ((SDL_WindowData *) window->driverdata)->hdc;
    PIXELFORMATDESCRIPTOR pfd;
    int pixel_format;
    unsigned int matching;
    int iAttribs[64];
    int *iAttr;
    float fAttribs[1] = { 0 };

    if (WIN_GL_Initialize(_this) < 0) {
        return -1;
    }

    WIN_GL_SetupPixelFormat(_this, &pfd);

    /* setup WGL_ARB_pixel_format attribs */
    iAttr = &iAttribs[0];

    *iAttr++ = WGL_DRAW_TO_WINDOW_ARB;
    *iAttr++ = GL_TRUE;
    *iAttr++ = WGL_ACCELERATION_ARB;
    *iAttr++ = WGL_FULL_ACCELERATION_ARB;
    *iAttr++ = WGL_RED_BITS_ARB;
    *iAttr++ = _this->gl_config.red_size;
    *iAttr++ = WGL_GREEN_BITS_ARB;
    *iAttr++ = _this->gl_config.green_size;
    *iAttr++ = WGL_BLUE_BITS_ARB;
    *iAttr++ = _this->gl_config.blue_size;

    if (_this->gl_config.alpha_size) {
        *iAttr++ = WGL_ALPHA_BITS_ARB;
        *iAttr++ = _this->gl_config.alpha_size;
    }

    *iAttr++ = WGL_DOUBLE_BUFFER_ARB;
    *iAttr++ = _this->gl_config.double_buffer;

    *iAttr++ = WGL_DEPTH_BITS_ARB;
    *iAttr++ = _this->gl_config.depth_size;

    if (_this->gl_config.stencil_size) {
        *iAttr++ = WGL_STENCIL_BITS_ARB;
        *iAttr++ = _this->gl_config.stencil_size;
    }

    if (_this->gl_config.accum_red_size) {
        *iAttr++ = WGL_ACCUM_RED_BITS_ARB;
        *iAttr++ = _this->gl_config.accum_red_size;
    }

    if (_this->gl_config.accum_green_size) {
        *iAttr++ = WGL_ACCUM_GREEN_BITS_ARB;
        *iAttr++ = _this->gl_config.accum_green_size;
    }

    if (_this->gl_config.accum_blue_size) {
        *iAttr++ = WGL_ACCUM_BLUE_BITS_ARB;
        *iAttr++ = _this->gl_config.accum_blue_size;
    }

    if (_this->gl_config.accum_alpha_size) {
        *iAttr++ = WGL_ACCUM_ALPHA_BITS_ARB;
        *iAttr++ = _this->gl_config.accum_alpha_size;
    }

    if (_this->gl_config.stereo) {
        *iAttr++ = WGL_STEREO_ARB;
        *iAttr++ = GL_TRUE;
    }

    if (_this->gl_config.multisamplebuffers) {
        *iAttr++ = WGL_SAMPLE_BUFFERS_ARB;
        *iAttr++ = _this->gl_config.multisamplebuffers;
    }

    if (_this->gl_config.multisamplesamples) {
        *iAttr++ = WGL_SAMPLES_ARB;
        *iAttr++ = _this->gl_config.multisamplesamples;
    }

    if (_this->gl_config.accelerated >= 0) {
        *iAttr++ = WGL_ACCELERATION_ARB;
        *iAttr++ =
            (_this->gl_config.
             accelerated ? WGL_GENERIC_ACCELERATION_ARB :
             WGL_NO_ACCELERATION_ARB);
    }

    *iAttr = 0;

    /* Choose and set the closest available pixel format */
    if (!_this->gl_data->WGL_ARB_pixel_format
        || !_this->gl_data->wglChoosePixelFormatARB(hdc, iAttribs, fAttribs,
                                                    1, &pixel_format,
                                                    &matching) || !matching) {
        pixel_format = ChoosePixelFormat(hdc, &pfd);
    }
    if (!pixel_format) {
        SDL_SetError("No matching GL pixel format available");
        return -1;
    }
    if (!SetPixelFormat(hdc, pixel_format, &pfd)) {
        WIN_SetError("SetPixelFormat()");
        return (-1);
    }
    return 0;
}

void
WIN_GL_CleanupWindow(_THIS, SDL_Window * window)
{
    WIN_GL_Shutdown(_this);
}

SDL_GLContext
WIN_GL_CreateContext(_THIS, SDL_Window * window)
{
    HDC hdc = ((SDL_WindowData *) window->driverdata)->hdc;

    return _this->gl_data->wglCreateContext(hdc);
}

int
WIN_GL_MakeCurrent(_THIS, SDL_Window * window, SDL_GLContext context)
{
    HDC hdc;
    int status;

    if (window) {
        hdc = ((SDL_WindowData *) window->driverdata)->hdc;
    } else {
        hdc = NULL;
    }
    if (!_this->gl_data->wglMakeCurrent(hdc, (HGLRC) context)) {
        WIN_SetError("wglMakeCurrent()");
        status = -1;
    } else {
        status = 0;
    }
    return status;
}

int
WIN_GL_SetSwapInterval(_THIS, int interval)
{
    if (_this->gl_data->wglSwapIntervalEXT) {
        _this->gl_data->wglSwapIntervalEXT(interval);
        return 0;
    } else {
        SDL_Unsupported();
        return -1;
    }
}

int
WIN_GL_GetSwapInterval(_THIS)
{
    if (_this->gl_data->wglGetSwapIntervalEXT) {
        return _this->gl_data->wglGetSwapIntervalEXT();
    } else {
        SDL_Unsupported();
        return -1;
    }
}

void
WIN_GL_SwapWindow(_THIS, SDL_Window * window)
{
    HDC hdc = ((SDL_WindowData *) window->driverdata)->hdc;

    SwapBuffers(hdc);
}

void
WIN_GL_DeleteContext(_THIS, SDL_GLContext context)
{
    _this->gl_data->wglDeleteContext((HGLRC) context);
}

#endif /* SDL_VIDEO_OPENGL */


/* vi: set ts=4 sw=4 expandtab: */

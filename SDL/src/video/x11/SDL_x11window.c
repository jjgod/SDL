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

#include "SDL_syswm.h"
#include "../SDL_sysvideo.h"
#include "../../events/SDL_keyboard_c.h"

#include "SDL_x11video.h"


static int
SetupWindowData(_THIS, SDL_Window * window, Window w, BOOL created)
{
    SDL_VideoData *videodata = (SDL_VideoData *) _this->driverdata;
    SDL_WindowData *data;
    int numwindows = videodata->numwindows;
    SDL_WindowData **windowlist = videodata->windowlist;

    /* Allocate the window data */
    data = (SDL_WindowData *) SDL_malloc(sizeof(*data));
    if (!data) {
        SDL_OutOfMemory();
        return -1;
    }
    data->windowID = window->id;
    data->window = w;
#ifdef X_HAVE_UTF8_STRING
    if (SDL_X11_HAVE_UTF8) {
        data->ic =
            pXCreateIC(videodata->im, XNClientWindow, w, XNFocusWindow, w,
                       XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                       XNResourceName, videodata->classname, XNResourceClass,
                       videodata->classname, NULL);
    }
#endif
    data->created = created;
    data->videodata = videodata;

    /* Associate the data with the window */
    windowlist =
        (SDL_WindowData **) SDL_realloc(windowlist,
                                        (numwindows +
                                         1) * sizeof(*windowlist));
    if (!windowlist) {
        SDL_OutOfMemory();
        SDL_free(data);
        return -1;
    }
    windowlist[numwindows++] = data;
    videodata->numwindows = numwindows;
    videodata->windowlist = windowlist;

    /* Fill in the SDL window with the window data */
    {
        XWindowAttributes attrib;

        XGetWindowAttributes(data->videodata->display, w, &attrib);
        window->x = attrib.x;
        window->y = attrib.y;
        window->w = attrib.width;
        window->h = attrib.height;
        if (attrib.map_state != IsUnmapped) {
            window->flags |= SDL_WINDOW_SHOWN;
        } else {
            window->flags &= ~SDL_WINDOW_SHOWN;
        }
    }
    /* FIXME: How can I tell?
       {
       DWORD style = GetWindowLong(hwnd, GWL_STYLE);
       if (style & WS_VISIBLE) {
       if (style & (WS_BORDER | WS_THICKFRAME)) {
       window->flags &= ~SDL_WINDOW_BORDERLESS;
       } else {
       window->flags |= SDL_WINDOW_BORDERLESS;
       }
       if (style & WS_THICKFRAME) {
       window->flags |= SDL_WINDOW_RESIZABLE;
       } else {
       window->flags &= ~SDL_WINDOW_RESIZABLE;
       }
       if (style & WS_MAXIMIZE) {
       window->flags |= SDL_WINDOW_MAXIMIZED;
       } else {
       window->flags &= ~SDL_WINDOW_MAXIMIZED;
       }
       if (style & WS_MINIMIZE) {
       window->flags |= SDL_WINDOW_MINIMIZED;
       } else {
       window->flags &= ~SDL_WINDOW_MINIMIZED;
       }
       }
       if (GetFocus() == hwnd) {
       int index = data->videodata->keyboard;
       window->flags |= SDL_WINDOW_INPUT_FOCUS;
       SDL_SetKeyboardFocus(index, data->windowID);

       if (window->flags & SDL_WINDOW_INPUT_GRABBED) {
       RECT rect;
       GetClientRect(hwnd, &rect);
       ClientToScreen(hwnd, (LPPOINT) & rect);
       ClientToScreen(hwnd, (LPPOINT) & rect + 1);
       ClipCursor(&rect);
       }
       }
     */

    /* All done! */
    window->driverdata = data;
    return 0;
}

int
X11_CreateWindow(_THIS, SDL_Window * window)
{
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;
    SDL_DisplayData *displaydata =
        (SDL_DisplayData *) SDL_CurrentDisplay.driverdata;
    Visual *visual;
    int depth;
    XSetWindowAttributes xattr;
    int x, y;
    Window w;
    XSizeHints *sizehints;
    XWMHints *wmhints;
    XClassHint *classhints;

#if SDL_VIDEO_DRIVER_X11_XINERAMA
/* FIXME
    if ( use_xinerama ) {
        x = xinerama_info.x_org;
        y = xinerama_info.y_org;
    }
*/
#endif
    if (window->flags & SDL_WINDOW_OPENGL) {
        /* FIXME: get the glx visual */
    } else {
        visual = displaydata->visual;
        depth = displaydata->depth;
    }

    if (window->flags & SDL_WINDOW_FULLSCREEN) {
        xattr.override_redirect = True;
    } else {
        xattr.override_redirect = False;
    }
    xattr.background_pixel = 0;
    xattr.border_pixel = 0;
    if (visual->class == PseudoColor || visual->class == DirectColor) {
        xattr.colormap =
            XCreateColormap(data->display,
                            RootWindow(data->display, displaydata->screen),
                            visual, AllocAll);
    } else {
        xattr.colormap =
            XCreateColormap(data->display,
                            RootWindow(data->display, displaydata->screen),
                            visual, AllocNone);
    }

    if ((window->flags & SDL_WINDOW_FULLSCREEN) ||
        window->x == SDL_WINDOWPOS_CENTERED) {
        x = (DisplayWidth(data->display, displaydata->screen) -
             window->w) / 2;
    } else if (window->x == SDL_WINDOWPOS_UNDEFINED) {
        x = 0;
    } else {
        x = window->x;
    }
    if ((window->flags & SDL_WINDOW_FULLSCREEN) ||
        window->y == SDL_WINDOWPOS_CENTERED) {
        y = (DisplayHeight(data->display, displaydata->screen) -
             window->h) / 2;
    } else if (window->y == SDL_WINDOWPOS_UNDEFINED) {
        y = 0;
    } else {
        y = window->y;
    }

    w = XCreateWindow(data->display,
                      RootWindow(data->display, displaydata->screen), x, y,
                      window->w, window->h, 0, depth, InputOutput, visual,
                      (CWOverrideRedirect | CWBackPixel | CWBorderPixel |
                       CWColormap), &xattr);

    sizehints = XAllocSizeHints();
    if (sizehints) {
        if (window->flags & SDL_WINDOW_RESIZABLE) {
            sizehints->min_width = 32;
            sizehints->min_height = 32;
            sizehints->max_height = 4096;
            sizehints->max_width = 4096;
        } else {
            sizehints->min_width = sizehints->max_width = window->w;
            sizehints->min_height = sizehints->max_height = window->h;
        }
        sizehints->flags = PMaxSize | PMinSize;
        if (!(window->flags & SDL_WINDOW_FULLSCREEN)
            && window->x != SDL_WINDOWPOS_UNDEFINED
            && window->y != SDL_WINDOWPOS_UNDEFINED) {
            sizehints->x = x;
            sizehints->y = y;
            sizehints->flags |= USPosition;
        }
        XSetWMNormalHints(data->display, w, sizehints);
        XFree(sizehints);
    }

    if (window->flags & SDL_WINDOW_BORDERLESS) {
        SDL_bool set;
        Atom WM_HINTS;

        /* We haven't modified the window manager hints yet */
        set = SDL_FALSE;

        /* First try to set MWM hints */
        WM_HINTS = XInternAtom(data->display, "_MOTIF_WM_HINTS", True);
        if (WM_HINTS != None) {
            /* Hints used by Motif compliant window managers */
            struct
            {
                unsigned long flags;
                unsigned long functions;
                unsigned long decorations;
                long input_mode;
                unsigned long status;
            } MWMHints = {
            (1L << 1), 0, 0, 0, 0};

            XChangeProperty(data->display, w, WM_HINTS, WM_HINTS, 32,
                            PropModeReplace, (unsigned char *) &MWMHints,
                            sizeof(MWMHints) / sizeof(long));
            set = SDL_TRUE;
        }
        /* Now try to set KWM hints */
        WM_HINTS = XInternAtom(data->display, "KWM_WIN_DECORATION", True);
        if (WM_HINTS != None) {
            long KWMHints = 0;

            XChangeProperty(data->display, w,
                            WM_HINTS, WM_HINTS, 32,
                            PropModeReplace,
                            (unsigned char *) &KWMHints,
                            sizeof(KWMHints) / sizeof(long));
            set = SDL_TRUE;
        }
        /* Now try to set GNOME hints */
        WM_HINTS = XInternAtom(data->display, "_WIN_HINTS", True);
        if (WM_HINTS != None) {
            long GNOMEHints = 0;

            XChangeProperty(data->display, w,
                            WM_HINTS, WM_HINTS, 32,
                            PropModeReplace,
                            (unsigned char *) &GNOMEHints,
                            sizeof(GNOMEHints) / sizeof(long));
            set = SDL_TRUE;
        }
        /* Finally set the transient hints if necessary */
        if (!set) {
            XSetTransientForHint(data->display, w,
                                 RootWindow(data->display,
                                            displaydata->screen));
        }
    } else {
        SDL_bool set;
        Atom WM_HINTS;

        /* We haven't modified the window manager hints yet */
        set = SDL_FALSE;

        /* First try to unset MWM hints */
        WM_HINTS = XInternAtom(data->display, "_MOTIF_WM_HINTS", True);
        if (WM_HINTS != None) {
            XDeleteProperty(data->display, w, WM_HINTS);
            set = SDL_TRUE;
        }
        /* Now try to unset KWM hints */
        WM_HINTS = XInternAtom(data->display, "KWM_WIN_DECORATION", True);
        if (WM_HINTS != None) {
            XDeleteProperty(data->display, w, WM_HINTS);
            set = SDL_TRUE;
        }
        /* Now try to unset GNOME hints */
        WM_HINTS = XInternAtom(data->display, "_WIN_HINTS", True);
        if (WM_HINTS != None) {
            XDeleteProperty(data->display, w, WM_HINTS);
            set = SDL_TRUE;
        }
        /* Finally unset the transient hints if necessary */
        if (!set) {
            /* NOTE: Does this work? */
            XSetTransientForHint(data->display, w, None);
        }
    }

    /* Tell KDE to keep fullscreen windows on top */
    if (window->flags & SDL_WINDOW_FULLSCREEN) {
        XEvent ev;
        long mask;

        SDL_zero(ev);
        ev.xclient.type = ClientMessage;
        ev.xclient.window = RootWindow(data->display, displaydata->screen);
        ev.xclient.message_type =
            XInternAtom(data->display, "KWM_KEEP_ON_TOP", False);
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = w;
        ev.xclient.data.l[1] = CurrentTime;
        XSendEvent(data->display,
                   RootWindow(data->display, displaydata->screen), False,
                   SubstructureRedirectMask, &ev);
    }

    /* Set the input hints so we get keyboard input */
    wmhints = XAllocWMHints();
    if (wmhints) {
        wmhints->input = True;
        if (window->flags & SDL_WINDOW_MINIMIZED) {
            wmhints->initial_state = IconicState;
        } else if (window->flags & SDL_WINDOW_SHOWN) {
            wmhints->initial_state = NormalState;
        } else {
            wmhints->initial_state = WithdrawnState;
        }
        wmhints->flags = InputHint | StateHint;
        XSetWMHints(data->display, w, wmhints);
        XFree(wmhints);
    }

    XSelectInput(data->display, w,
                 (FocusChangeMask | EnterWindowMask | LeaveWindowMask |
                  ExposureMask | ButtonPressMask | ButtonReleaseMask |
                  PointerMotionMask | KeyPressMask | KeyReleaseMask |
                  PropertyChangeMask | StructureNotifyMask |
                  KeymapStateMask));

    /* Set the class hints so we can get an icon (AfterStep) */
    classhints = XAllocClassHint();
    if (classhints != NULL) {
        classhints->res_name = data->classname;
        classhints->res_class = data->classname;
        XSetClassHint(data->display, w, classhints);
        XFree(classhints);
    }

    /* Allow the window to be deleted by the window manager */
    XSetWMProtocols(data->display, w, &data->WM_DELETE_WINDOW, 1);

    /* Finally, show the window */
    if (window->flags & SDL_WINDOW_SHOWN) {
        XMapRaised(data->display, w);
    }
    XSync(data->display, False);

    if (SetupWindowData(_this, window, w, SDL_TRUE) < 0) {
        XDestroyWindow(data->display, w);
        return -1;
    }

    X11_SetWindowTitle(_this, window);

#ifdef SDL_VIDEO_OPENGL
    /*
       if (window->flags & SDL_WINDOW_OPENGL) {
       if (X11_GL_SetupWindow(_this, window) < 0) {
       X11_DestroyWindow(_this, window);
       return -1;
       }
       }
     */
#endif
    return 0;
}

int
X11_CreateWindowFrom(_THIS, SDL_Window * window, const void *data)
{
    Window w = (Window) data;

    /* FIXME: Query the title from the existing window */

    if (SetupWindowData(_this, window, w, SDL_FALSE) < 0) {
        return -1;
    }
    return 0;
}

void
X11_SetWindowTitle(_THIS, SDL_Window * window)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    Display *display = data->videodata->display;
    XTextProperty titleprop, iconprop;
    Status status;
    const char *title = window->title;
    const char *icon = NULL;

#ifdef X_HAVE_UTF8_STRING
    Atom _NET_WM_NAME = 0;
    Atom _NET_WM_ICON_NAME = 0;

    /* Look up some useful Atoms */
    if (SDL_X11_HAVE_UTF8) {
        _NET_WM_NAME = XInternAtom(display, "_NET_WM_NAME", False);
        _NET_WM_ICON_NAME = XInternAtom(display, "_NET_WM_ICON_NAME", False);
    }
#endif

    if (title != NULL) {
        char *title_latin1 = SDL_iconv_utf8_latin1((char *) title);
        if (!title_latin1) {
            SDL_OutOfMemory();
            return;
        }
        status = XStringListToTextProperty(&title_latin1, 1, &titleprop);
        SDL_free(title_latin1);
        if (status) {
            XSetTextProperty(display, data->window, &titleprop, XA_WM_NAME);
            XFree(titleprop.value);
        }
#ifdef X_HAVE_UTF8_STRING
        if (SDL_X11_HAVE_UTF8) {
            status =
                Xutf8TextListToTextProperty(display, (char **) &title, 1,
                                            XUTF8StringStyle, &titleprop);
            if (status == Success) {
                XSetTextProperty(display, data->window, &titleprop,
                                 _NET_WM_NAME);
                XFree(titleprop.value);
            }
        }
#endif
    }
    if (icon != NULL) {
        char *icon_latin1 = SDL_iconv_utf8_latin1((char *) icon);
        if (!icon_latin1) {
            SDL_OutOfMemory();
            return;
        }
        status = XStringListToTextProperty(&icon_latin1, 1, &iconprop);
        SDL_free(icon_latin1);
        if (status) {
            XSetTextProperty(display, data->window, &iconprop,
                             XA_WM_ICON_NAME);
            XFree(iconprop.value);
        }
#ifdef X_HAVE_UTF8_STRING
        if (SDL_X11_HAVE_UTF8) {
            status =
                Xutf8TextListToTextProperty(display, (char **) &icon, 1,
                                            XUTF8StringStyle, &iconprop);
            if (status == Success) {
                XSetTextProperty(display, data->window, &iconprop,
                                 _NET_WM_ICON_NAME);
                XFree(iconprop.value);
            }
        }
#endif
    }
}

void
X11_SetWindowPosition(_THIS, SDL_Window * window)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    SDL_DisplayData *displaydata =
        (SDL_DisplayData *) SDL_CurrentDisplay.driverdata;
    Display *display = data->videodata->display;
    int x, y;

    if ((window->flags & SDL_WINDOW_FULLSCREEN) ||
        window->x == SDL_WINDOWPOS_CENTERED) {
        x = (DisplayWidth(display, displaydata->screen) - window->w) / 2;
    } else if (window->x == SDL_WINDOWPOS_UNDEFINED) {
        x = 0;
    } else {
        x = window->x;
    }
    if ((window->flags & SDL_WINDOW_FULLSCREEN) ||
        window->y == SDL_WINDOWPOS_CENTERED) {
        y = (DisplayHeight(display, displaydata->screen) - window->h) / 2;
    } else if (window->y == SDL_WINDOWPOS_UNDEFINED) {
        y = 0;
    } else {
        y = window->y;
    }
    XMoveWindow(display, data->window, x, y);
}

void
X11_SetWindowSize(_THIS, SDL_Window * window)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    Display *display = data->videodata->display;

    XMoveWindow(display, data->window, window->w, window->h);
}

void
X11_ShowWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    Display *display = data->videodata->display;

    XMapRaised(display, data->window);
}

void
X11_HideWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    Display *display = data->videodata->display;

    XUnmapWindow(display, data->window);
}

void
X11_RaiseWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    Display *display = data->videodata->display;

    XRaiseWindow(display, data->window);
}

void
X11_MaximizeWindow(_THIS, SDL_Window * window)
{
    /* FIXME: is this even possible? */
}

void
X11_MinimizeWindow(_THIS, SDL_Window * window)
{
    X11_HideWindow(_this, window);
}

void
X11_RestoreWindow(_THIS, SDL_Window * window)
{
    X11_ShowWindow(_this, window);
}

void
X11_SetWindowGrab(_THIS, SDL_Window * window)
{
    /* FIXME */
}

void
X11_DestroyWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;

    if (data) {
        Display *display = data->videodata->display;
#ifdef SDL_VIDEO_OPENGL
        /*
           if (window->flags & SDL_WINDOW_OPENGL) {
           X11_GL_CleanupWindow(_this, window);
           }
         */
#endif
#ifdef X_HAVE_UTF8_STRING
        if (data->ic) {
            XDestroyIC(data->ic);
        }
#endif
        if (data->created) {
            XDestroyWindow(display, data->window);
        }
        SDL_free(data);
    }
}

SDL_bool
X11_GetWindowWMInfo(_THIS, SDL_Window * window, SDL_SysWMinfo * info)
{
    if (info->version.major <= SDL_MAJOR_VERSION) {
        /* FIXME! */
        return SDL_TRUE;
    } else {
        SDL_SetError("Application not compiled with SDL %d.%d\n",
                     SDL_MAJOR_VERSION, SDL_MINOR_VERSION);
        return SDL_FALSE;
    }
}

/* vi: set ts=4 sw=4 expandtab: */

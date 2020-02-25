/*-
 * Copyright (c) 2007,2008 Igor Serikov
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation. The copyright holders make no representations about the 
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <unistd.h>
#include <stdlib.h>
#include <sys/select.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xfuncs.h>
#include <X11/keysym.h>

#include "util.h"
#include "graphics.h"

int root_width, root_height, root_depth;

static Display * dpy = NULL;
static Drawable win, pix, out;
static Atom wm_delete_window;
static COLOR * bkc;
static int win_height, win_width;
static MAP * colors;

struct COLOR {
    unsigned long Pixel;
    GC Gc;
};

void InitGraphics (char * Display) {
    Drawable root_win;
    int root_x, root_y, root_bw;

    dpy = XOpenDisplay (Display);
    if (!dpy)
	Errx ("cannot open display \"%s\"", XDisplayName (Display));

    XGetGeometry (
      dpy, DefaultRootWindow (dpy), &root_win, 
      &root_x, &root_y, &root_width, &root_height, &root_bw,
      &root_depth
    );

    colors = CreateMap (1);
}

COLOR * AllocColor (char * Name) {
    XGCValues gcv;
    XColor sc, ec;
    COLOR * color;

    color = (COLOR *) LookupNamed (colors, Name);
    if (color)
	return color;

    color = (COLOR *) Alloc (sizeof (COLOR));
    AddNamed (colors, Name, color);

    if (
      XAllocNamedColor (
	dpy, DefaultColormap (dpy, DefaultScreen (dpy)), Name, &sc, &ec
      ) == 0
    )
	Errx ("cannot alloc color '%s'", Name);

    color->Pixel = sc.pixel;

    gcv.function = GXcopy;
    gcv.foreground = color->Pixel;
    color->Gc = XCreateGC (dpy, DefaultRootWindow (dpy), GCFunction | GCForeground, &gcv);
    return color;
}

COLOR * InvertColor (COLOR * OrigColor) {
    XGCValues gcv;
    COLOR * color;

    color = (COLOR *) Alloc (sizeof (COLOR));
    color->Pixel = OrigColor->Pixel ^ 0xFFFFFF;

    gcv.function = GXcopy;
    gcv.foreground = color->Pixel;
    color->Gc = XCreateGC (dpy, DefaultRootWindow (dpy), GCFunction | GCForeground, &gcv);
    return color;
}

void CreateWindow (COLOR * Background, COLOR * Border, int Width, int Height) {
    XSetWindowAttributes attrs;

    win_width = Width;
    win_height = Height;
    bkc = Background;

    attrs.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;
    attrs.background_pixel = bkc->Pixel;
    attrs.border_pixel = Border->Pixel;

    win = XCreateWindow (
      dpy, DefaultRootWindow (dpy), 0, 0, Width, Height, 1, 
      root_depth, InputOutput, DefaultVisual (dpy, DefaultScreen (dpy)), 
      CWEventMask | CWBackPixel | CWBorderPixel, &attrs
    );

    out = win;

    wm_delete_window = XInternAtom (dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols (dpy, win, &wm_delete_window, 1);

    XChangeProperty (
      dpy, win, XA_WM_NAME, XA_STRING, 8, PropModeReplace, GetProgName (), 
      strlen (GetProgName ())
    );

    XMapWindow (dpy, win);
}

void Bell (void) {
    struct timeval tv = { 0, 200000 };

    XBell (dpy, 0);
    XFlush (dpy);
    select (0, NULL, NULL, NULL, &tv);
    XBell (dpy, 0);
    XFlush (dpy);
}

void DrawPoint (COLOR* Color, int x, int y) {
    XDrawPoint (dpy, out, Color->Gc, x, win_height - y - 1);
}

void FillRectangle (COLOR* Color, int x, int y, int width, int height) {
    XFillRectangle (dpy, out, Color->Gc, x, win_height - y - height, width, height);
}

void HoldOutput (void) {
    pix = XCreatePixmap (dpy, win, win_width, win_height, root_depth);
    out = pix;
}

void ResumeOutput (void) {
    XCopyArea (dpy, pix, win, bkc->Gc, 0, 0, win_width, win_height, 0, 0);
    XFreePixmap (dpy, pix);
    out = win;
}

void DestroyWindow (void) {
    XDestroyWindow (dpy, win);
    XFlush (dpy);
}

void DeinitGraphics (void) {
    XCloseDisplay (dpy);
    FreeMap (colors, 1);
}

int ProcessEvents (int Wait) {
    XEvent xev;
    KeySym ksym;
    char buf [5];
    int r, tr, c;

    r = g_none;
    
    for (;;) {
	if (Wait)
	    XNextEvent (dpy, &xev);
	else if (!XCheckMaskEvent (dpy, -1, &xev))
	    return r;
	switch (xev.type) {
	  case ConfigureNotify:
	    if (xev.xconfigure.window != win)
		break;
	    win_width = xev.xconfigure.width;
	    win_height = xev.xconfigure.height;
	    WindowSize (xev.xconfigure.width, xev.xconfigure.height);
	    break;
	  case Expose:
	    if (xev.xexpose.window != win)
		break;
	    Redraw (
	      xev.xexpose.x, win_height - xev.xexpose.y - xev.xexpose.height, 
	      xev.xexpose.width, xev.xexpose.height
	    );
	    break;
	  case KeyPress: 
	    if (xev.xkey.window != win)
		break;
	    if (XLookupString (&xev.xkey, buf, sizeof buf, &ksym, NULL) == 1) {
		c = (unsigned char) buf [0];
		goto sc;
	    }
	    switch (ksym) {
	      case XK_Left:
		c = g_left;
		goto sc;
	      case XK_Right:
		c = g_right;
		goto sc;
	      case XK_Up:
		c = g_up;
		goto sc;
	      case XK_Down:
		c = g_down;
		goto sc;
	    }
	    break;
	sc:
	    tr = Key (c);
	    if (tr == g_none)
		break;
	    if (tr == g_quit) {
		Wait = 0;
		r = g_quit;
		break;
	    }
	    return tr;
	  case ClientMessage:
	    if (xev.xclient.window != win)
		break;
	    if ((unsigned) xev.xclient.data.l [0] == wm_delete_window) {
		XDestroyWindow (dpy, win);
		XFlush (dpy);
		return g_destroy;
	    }
	    break;
	}
    }
}

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

#include <stdlib.h>

#include "graphics.h"
#include "util.h"

#include "view.h"

struct VCOLOR {
    COLOR * Orig;
    COLOR * Inv;
};

/* (w_x,w_y) is a position of the window center on the data field */
static int f_width, f_height, w_x, w_y, w_width, w_height, mag = 1;
static COLOR * bkc;

#define MaxMag 32

VCOLOR * AllocVColor (char * Name) {
    VCOLOR * vcolor;

    vcolor = (VCOLOR *) Alloc (sizeof (VCOLOR));
    vcolor->Orig = AllocColor (Name);
    vcolor->Inv = InvertColor (vcolor->Orig);
    return vcolor;
}

void InitView (VCOLOR * Background, VCOLOR * Border, int FieldWidth, int FieldHeight) {
    bkc = Background->Orig;
    f_width = FieldWidth;
    f_height = FieldHeight;
    w_width = FieldWidth;
    w_height = FieldHeight;
    w_x = f_width / 2;
    w_y = f_height / 2;
    if (w_width >= root_width)
	w_width = root_width;
    if (w_height >= root_height)
	w_height = root_height;
    CreateWindow (Background->Orig, Border->Orig, w_width, w_height);
}

static void DrawRect (COLOR * Color, int xs, int ys, int w, int h) {
    int xe = xs + w;
    int ye = ys + h;
    if (xs < 0)
	xs = 0;
    if (ys < 0)
	ys = 0;
    if (xe > w_width)
	xe = w_width;
    if (ye > w_height)
	ye = w_height;
    if (xs < xe && ys < ye)
	FillRectangle (Color, xs, ys, xe - xs, ye - ys);
}

void ViewDrawPoint (VCOLOR * Color, int x, int y, int dir) {
    int xp, yp, xs, ys;
    if (mag == 1) {
	xp = w_width  / 2 + x - w_x;
	yp = w_height / 2 + y - w_y;
	if (xp >= 0 && yp >= 0 && xp < w_width && yp < w_height)
	    DrawPoint (Color->Orig, xp, yp);
	return;
    }
    xs = w_width  / 2 + (x - w_x) * mag;
    ys = w_height / 2 + (y - w_y) * mag;
    if (mag < 16) {
	DrawRect (Color->Orig, xs, ys, mag, mag);
	return;
    }
    switch (dir) {
      case dir_up:
	DrawRect (Color->Orig, xs, ys, mag / 2 - 1, mag);
	DrawRect (Color->Orig, xs + mag / 2 - 1, ys, 2, mag - 2 );
	DrawRect (Color->Inv, xs + mag / 2 - 1, ys + mag - 2, 2, 2);
	DrawRect (Color->Orig, xs + mag / 2 + 1, ys, mag / 2 - 1, mag);
	break; 
      case dir_right:
	DrawRect (Color->Orig, xs, ys + mag / 2 + 1, mag, mag / 2 - 1);
	DrawRect (Color->Orig, xs, ys + mag / 2 - 1, mag - 2, 2);
	DrawRect (Color->Inv, xs + mag - 2, ys + mag / 2 - 1, 2, 2);
	DrawRect (Color->Orig, xs, ys, mag, mag / 2 - 1);
	break; 
      case dir_down:
	DrawRect (Color->Orig, xs + mag / 2 + 1, ys, mag / 2 - 1, mag);
	DrawRect (Color->Orig, xs + mag / 2 - 1, ys + 2, 2, mag - 2);
	DrawRect (Color->Inv, xs + mag / 2 - 1, ys, 2, 2);
	DrawRect (Color->Orig, xs + 0, ys, mag / 2 - 1, mag);
	break; 
      case dir_left:
	DrawRect (Color->Orig, xs + 0, ys, mag, mag / 2 - 1);
	DrawRect (Color->Orig, xs + 2,ys + mag / 2 - 1, mag - 2, 2);
	DrawRect (Color->Inv, xs + 0, ys + mag / 2 - 1, 2, 2);
	DrawRect (Color->Orig, xs + 0, ys + mag / 2 + 1, mag, mag / 2 - 1);
	break; 
  default:
	DrawRect (Color->Orig, xs, ys, mag, mag);
	break;
    }
}

void Redraw (int x, int y, int width, int height) {
    int xs = w_x + (x - w_width  / 2) / mag;
    int ys = w_y + (y - w_height / 2) / mag;
    int xe = w_x + (x + width  + mag - 1 - w_width  / 2) / mag;
    int ye = w_y + (y + height + mag - 1 - w_height / 2) / mag;
    if (xs < 0)
	xs = 0;
    if (ys < 0)
	ys = 0;
    if (xe > f_width)
	xe = f_width;
    if (ye > f_height)
	ye = f_height;
    if (xs < xe && ys < ye)
	AppRedraw (xs, ys, xe - xs, ye - ys);
}

static void Repaint (void) {
    HoldOutput ();
    FillRectangle (bkc, 0, 0, w_width, w_height);
    Redraw (0, 0, w_width, w_height);
    ResumeOutput ();
}

void WindowSize (int Width, int Height) {
    w_width = Width;
    w_height = Height;
    Repaint ();
}

int Key (int c) {
    int step = MaxMag / mag;
    switch (c) {
      case '-':
	if (mag == 1)
	    return g_none;
	mag /= 2;
	break;
      case '+':
	if (mag >= MaxMag)
	    return g_none;
	mag *= 2;
	break;
      case g_left:
	if (w_x <= 0)
	    return g_none;
	w_x -= step;
	if (w_x < 0)
	    w_x = 0;
	break;
      case g_right:
	if (w_x >= f_width - 1)
	    return g_none;
	w_x += step;
	if (w_x > f_width - 1)
	    w_x = f_width - 1;
	break;
      case g_down:
	if (w_y <= 0)
	    return g_none;
	w_y -=  step;
	if (w_y < 0)
	    w_y = 0;
	break;
      case g_up:
	if (w_y >= f_height - 1)
	    return g_none;
	w_y += step;
	if (w_y > f_height - 1)
	    w_y = f_height - 1;
	break;
      default:
	return AppKey (c);
    } 
    Repaint ();
    return g_none;
}

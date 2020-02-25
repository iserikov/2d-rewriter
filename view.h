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

enum {
    dir_up = 0,
    dir_right,
    dir_down,
    dir_left,
    dir_none
};

typedef struct VCOLOR VCOLOR;

VCOLOR * AllocVColor (char * Name);
void InitView (VCOLOR * Background, VCOLOR * Border, int FieldWidth, int FieldHeight);
void ViewDrawPoint (VCOLOR* Color, int x, int y, int dir);

void  AppRedraw (int x, int y, int width, int height);
int AppKey (int c);

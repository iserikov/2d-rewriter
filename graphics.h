/*-
 * Copyright (c) 2007,2008 Igor Serikov iserikov@acm.org
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

typedef struct COLOR COLOR;

enum { g_none = 256, g_quit, g_destroy, g_left, g_right, g_up, g_down, g_app };

extern int root_width, root_height, root_depth;

void InitGraphics (char * Display);
COLOR * AllocColor (char* Name);
COLOR * InvertColor (COLOR * OrigColor);
void CreateWindow (COLOR * Background, COLOR * Border, int Width, int Height);
void DestroyWindow (void);
void Bell (void);
void DrawPoint (COLOR * Color, int x, int y);
void HoldOutput (void);
void ResumeOutput (void);
void FillRectangle (COLOR* Color, int x, int y, int width, int height);
void DeinitGraphics (void);
int ProcessEvents (int Wait);

void Redraw (int x, int y, int width, int height);
int Key (int c);
void WindowSize (int Width, int Height);

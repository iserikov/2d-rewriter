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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

#include "util.h"
#include "scanner.h"
#include "graphics.h"
#include "view.h"

#define VERSION "1.6"

#define ProcessEventsThreshold 1000 

/*
 * MaskBits is a limit for the number of tuples in a set. Sorry;-)
 */
typedef unsigned long MaskType;
#define MaskAllOnes ULONG_MAX
#define MaskBits (CHAR_BIT * sizeof (MaskType))

/* 
 * We want:
 * 1) A * B       to fit to positive integer
 * 2) A * B * 64  to fit to size_t
 * Yet, we rely upon compiler' ability to convert correctly integer array indexes to 
 * data addresses on platforms where addresses are longer than integers (like amd64).
 */
#define SIZE_T_BITS (CHAR_BIT * sizeof (size_t) - 6)
#define POSITIVE_INT_BITS (CHAR_BIT * sizeof (int) - 1)
#define PRODUCT_BITS (SIZE_T_BITS < POSITIVE_INT_BITS ? SIZE_T_BITS : POSITIVE_INT_BITS) 
#define SAFE_MUL (UINT_MAX >> ( CHAR_BIT * sizeof (int) - PRODUCT_BITS / 2 ))

typedef struct {
    VCOLOR * color;
    int Index;
} OBJECT;

typedef struct {
    int IndexBoundary;
    int PosCount;
    int TupleCount;
    int * Objects;    /* Tuple       * Position -> ObjectIndex  */
    MaskType * Masks; /* ObjectIndex * Position -> tuples_bitmap  */
} SET;

#define SET_TUPLE(s,o,p,t) \
do { \
  SET * _s = (s); \
  _s->Masks [(o) * _s->PosCount + (p)] |= (MaskType)1 << (t); \
} while (0)

#define GET_MASK(s,o,p) \
  ({ \
    SET * _s = (s); \
    int _o = (o); \
    _o >= _s->IndexBoundary ? (MaskType) 0 : _s->Masks [_o * _s->PosCount + (p)]; \
  })

typedef struct OBJ_POS OBJ_POS;

struct OBJ_POS {
    OBJ_POS * Next;
    int Index;
    int Pos;
};

DECLARE_LIST (OBJ_POS)

typedef struct OBJ_DIR OBJ_DIR;

struct OBJ_DIR {
    OBJ_DIR * Next;
    int Dir;
    int Pos;
};

DECLARE_LIST (OBJ_DIR)

typedef struct SET_POS_POS SET_POS_POS;

struct SET_POS_POS {
    SET_POS_POS * Next;
    int Seq;
    int Pos;
};

typedef struct SET_POS SET_POS;

struct SET_POS {
    SET_POS * Next;
    SET * Set;
    SET_POS_POS * Positions;
};

DECLARE_LIST (SET_POS)

typedef struct RULE RULE;

struct RULE {
    RULE * Next;
    OBJ_DIR * ObjDir;
    OBJ_POS * ObjPos;
    SET_POS * SetPos;
    SET_POS * ResSetPos;   
    int ResIndexOrSeq; 
    int Dir;
};

DECLARE_LIST (RULE)

typedef struct CELL CELL;

struct CELL {
    CELL * Next;
    int Dir;
    int Index;
    int NewDir;
    int NewIndex;
};

DECLARE_LIST (CELL)

typedef struct {
    SET * Set;
    SET_POS * SetPos;
    SET_POS_POS ** LastSetPos;
}  VARIABLE;

enum { 
    w_dot = w_app, 
    w_left_round_brace, 
    w_right_round_brace, 
    w_left_curly_brace, 
    w_right_curly_brace, 
    w_slash, 
    w_colon,
    w_star,
    w_use,
    w_dim,
    w_obj,
    w_set,
    w_init, 
    w_rule,
    w_up, 
    w_right, 
    w_down, 
    w_left
};

char * SymbolTable [] = {
    ".", "(", ")", "{", "}", "/", ":", "*", "use", "dimensions", "object",
    "set", "init", "rule", "up", "right", "down", "left", NULL
};

enum { g_exit = g_app };

static OBJECT * ground, * border;
static int wx, wy, wxp1, wxp2, wxp3, wyp1, wyp2, stepping = 0;
static CELL * cells = NULL;
static VCOLOR ** colors;

void  AppRedraw (int x, int y, int width, int height) {
    CELL* c = cells + x + y * wxp2;
    int lx = x + width, ly = y + height, d = wxp2 - width;

    int ix;

    for (; y < ly; y ++, c += d)
        for (ix = x; ix < lx; ix ++, c ++)
	    ViewDrawPoint (colors [c->Index], ix, y, c->Dir);
}

int AppKey (int c) {
    switch (c) {
      case 'q':
      case 'Q':
	return g_exit;
      case 's':
      case 'S':
	stepping = 1;
	return g_quit;
      case 'r':
      case 'R':
	stepping = 0;
	return g_quit;
    }
    return g_none;
}

static void Usage (FILE * out) {
    fprintf (
      out,
      "\nUsage:  %s [-s] filename\n"
      "        %s -h\n"
      "\n"
      "-h - show this help\n"
      "-s - start in stepping mode\n"
      , GetProgName (), GetProgName ()
    );
}

static int ParseDir (void) {
    if (Expect (w_up))
	return dir_up;
    if (Expect (w_right))
	return dir_right;
    if (Expect (w_down))
	return dir_down;
    if (Expect (w_left))
	return dir_left;
    Unexpected ();
}

int main (int argc, char * argv []) {
    int nobjects, count;
    MAP * objects, * sets;
    RULE_LIST rules;
    CELL_LIST curcells;

    SetProgName (argv [0]);

    nobjects = 0;
    objects = CreateMap (0);
    sets = CreateMap (0);

    INIT_LIST (&rules);

    {
	int opt;
	while ( (opt = getopt (argc, argv, "hs")) != -1)
	    switch (opt) {
	      case 's':
		stepping = 1;
		break;
	      case 'h':
		printf ("%s v%s\n", GetProgName (), VERSION);
		Usage (stdout);
		return 0;
	      default:
		Usage (stderr);
		return 1;
	    }
    }

    if (optind + 1 != argc) {
	fprintf (stderr, "No input file specified\n");
	Usage (stderr);
	return 1;
    }

    InitGraphics (NULL);

    OpenScanner (argv [optind]);

    while (!Expect (w_eof)) {
	if (Expect (w_use)) {
	    MustBe (w_string);
	    OpenScanner (Token);
	    continue;
	}
	if (!cells && Expect (w_dim)) {
	    MustBe (w_number);

	    if (Number > (int) (SAFE_MUL - 3))
		Syntax ("X is too big");

	    wx = Number;

	    wxp1 = wx + 1;
	    wxp2 = wx + 2;
	    wxp3 = wx + 3;

	    MustBe (w_number);

	    if (Number > (int)(SAFE_MUL - 3))
		Syntax ("Y is too big");

	    wy = Number;
	    wyp1 = wy + 1;
	    wyp2 = wy + 2;

	    cells = (CELL *) Alloc (wxp2 * wyp2 * sizeof (CELL));
	    continue;
	}
	if (Expect (w_obj)) {
	    OBJECT * o;

	    if (nobjects >= (int) SAFE_MUL)
		Syntax ("too many objects");

	    MustBe (w_name);

	    if (LookupNamed (objects, Token) || LookupNamed (sets, Token))
		Syntax ("name %s is already used", Token);

	    o = (OBJECT *) Alloc (sizeof (OBJECT));

	    AddNamed (objects, Token, o);

	    o->Index = nobjects ++;

	    MustBe (w_name);

	    o->color = AllocVColor (Token);
	    continue;
	}
	if (cells && Expect (w_init)) {
	    OBJECT * o;
	    CELL * c;
	    int x, dir;

	    MustBe (w_name);

	    o = (OBJECT *) LookupNamed (objects, Token);
	    if (!o)
		Syntax ("name %s is not an object", Token);

	    dir = dir_up;
	    if (Expect (w_slash))
		dir = ParseDir ();

	    MustBe (w_number);

	    if (Number >= wx)
		Syntax ("invalid horizontal position");

	    x = Number;

	    MustBe (w_number);

	    if (Number >= wy)
		Syntax ("invalid vertical position");

	    c = &cells [((size_t)Number + 1) * wxp2 + x + 1];

	    if (c->NewIndex)
		Syntax ("position is occupied");

	    c->Index = o->Index;
	    c->Dir = dir;
	    c->NewIndex = 1;
	    continue;
	}
	if (Expect (w_set)) {
	    SET * s;
	    OBJECT * o;
	    INT_LIST * data;
	    int i, j, n, * dp;

	    MustBe (w_name);

	    if (LookupNamed (objects, Token) || LookupNamed (sets, Token))
		Syntax ("name %s is already used", Token);

	    s = (SET *) Alloc (sizeof (SET));

	    AddNamed (sets, Token, s);

	    s->IndexBoundary = nobjects;

	    MustBe (w_left_curly_brace);

	    data = CreateIntList ();
	    
	    if (Expect (w_name)) {
		s->PosCount = 1;
		for (;;) {
		    o = (OBJECT *) LookupNamed (objects, Token);
		    if (!o)
			Syntax ("object %s does not exist", Token);
		    AddInt (data, o->Index);
		    s->TupleCount ++;
		    if (Expect (w_right_curly_brace))
			break;
		    if (s->TupleCount >= (int) MaskBits)
			Syntax ("set contains too many members");
	    	    MustBe (w_name);
		}
	    } else {
		MustBe (w_left_round_brace);
		MustBe (w_name);
		for (;;) {
		    o = (OBJECT *) LookupNamed (objects, Token);
		    if (!o)
			Syntax ("object %s does not exist", Token);
		    AddInt (data, o->Index);
		    s->PosCount ++;
		    if (Expect (w_right_round_brace))
			break;
		    if (s->PosCount >= (int) SAFE_MUL)
			Syntax ("too many members in a tuple");
	    	    MustBe (w_name);
		}
		s->TupleCount = 1;
		while (!Expect (w_right_curly_brace)) {
	    	    MustBe (w_left_round_brace);
		    if (s->TupleCount >= (int) MaskBits)
			Syntax ("set contains too many members");
		    s->TupleCount ++;
		    for (n = 0; n < s->PosCount; n ++) {
			if (!Expect (w_name))
			    Unexpected ();
			o = (OBJECT *) LookupNamed (objects, Token);
			if (!o)
			    Syntax ("object %s does not exist", Token);
			AddInt (data, o->Index);
		    }
	    	    MustBe (w_right_round_brace);
		}
	    }
	    s->Objects = IntListToArray (data);
	    FreeIntList (data);
	    s->Masks = (MaskType *) Alloc (nobjects * s->PosCount * sizeof (MaskType));
	    dp = s->Objects;
	    for (i = 0; i < s->TupleCount; i ++)
		for (j = 0; j < s->PosCount; j ++, dp ++)
		    SET_TUPLE (s, *dp, j, i);
	    continue;
	}
	if (Expect (w_rule)) {
	    MAP * vars = CreateMap (0);
	    RULE * r = (RULE *) Alloc (sizeof (RULE));
	    OBJ_DIR ** objdirptr = &r->ObjDir;
	    OBJ_POS ** objposptr = &r->ObjPos;
	    SET_POS ** setposptr = &r->SetPos;

	    OBJ_DIR * od;
	    OBJ_POS * op;
	    SET_POS * sp;
	    SET_POS_POS * spp;
	    VARIABLE * v;
	    OBJECT * o;
	    SET * s;
	    int i;

	    ADD_TO_LIST (&rules, r);

	    if (Expect (w_left_round_brace)) {
		MustBe (w_name);
		for (;;) {
    		    if (LookupNamed (vars, Token))
			Syntax ("variable %s is already defined", Token);
		    if (LookupNamed (objects, Token) || LookupNamed (sets, Token))
			Syntax ("name %s is already used for an object or a set", Token);
		    v = (VARIABLE *) Alloc (sizeof (VARIABLE));
		    AddNamed (vars, Token, v);
		    MustBe (w_colon);
		    MustBe (w_name);
    		    s = (SET *) LookupNamed (sets, Token);
    		    if (!s)
			Syntax ("set %s does not exist", Token);
		    v->Set = s;
		    if (Expect (w_right_round_brace))
			break;
		    MustBe (w_name);
		}
	    }
	    for (i = 0; i < 9; i ++) {
		if (Expect (w_star))
		    goto get_pos;
		MustBe (w_name);
		o = (OBJECT *) LookupNamed (objects, Token);
		if (o) {
		    op = (OBJ_POS *) Alloc (sizeof (OBJ_POS));
		    ADD_TO_LIST_P (&objposptr, op);
		    op->Index = o->Index;
		    op->Pos = i;
		    goto get_pos;
		}
		s = (SET *) LookupNamed (sets, Token);
		if (s) {
		    sp = (SET_POS *) Alloc (sizeof (SET_POS));
		    ADD_TO_LIST_P (&setposptr, sp);
		    sp->Set = s;
		    spp = (SET_POS_POS *) Alloc (sizeof (SET_POS_POS));
		    sp->Positions = spp;
		    spp->Pos = i;
		    if (Expect (w_dot)) {
			MustBe (w_number);
			if (Number >= s->PosCount)
			    Syntax ("wrong position");
			spp->Seq = Number;
		    }
		    goto get_pos;
		}
		v = (VARIABLE *) LookupNamed (vars, Token);
		if (v) {
		    s = v->Set;
		    sp = v->SetPos;
		    if (!sp) {
			sp = (SET_POS *) Alloc (sizeof (SET_POS));
			ADD_TO_LIST_P (&setposptr, sp);
			sp->Set = s;
			v->SetPos = sp;
			v->LastSetPos = &sp->Positions;
		    }
		    spp = (SET_POS_POS *) Alloc (sizeof (SET_POS_POS));
		    spp->Pos = i;
		    ADD_TO_LIST_P (&v->LastSetPos, spp);
		    if (Expect (w_dot)) {
			MustBe (w_number);
			if (Number >= s->PosCount)
			    Syntax ("wrong position");
			spp->Seq = Number;
		    }
		    goto get_pos;
		}
		Syntax ("name %s is unknown", Token);
	    get_pos:
		if (Expect (w_slash)) {
 		    od = (OBJ_DIR *) Alloc (sizeof (OBJ_DIR));
		    ADD_TO_LIST_P (&objdirptr, od);
		    od->Dir = ParseDir ();
		    od->Pos = i;
		}
	    }
	    MustBe (w_name);
	    o = (OBJECT *) LookupNamed (objects, Token);
	    if (o) 
		r->ResIndexOrSeq = o->Index;
	    else {
		v = (VARIABLE *) LookupNamed (vars, Token);
		if (v) {
		    sp = v->SetPos;
		    if (!sp)
			Syntax ("variable %s is used unitialized in the result", Token);
		    r->ResSetPos = sp;
		    s = v->Set;
		    if (Expect (w_dot)) {
			MustBe (w_number);
			if (Number >= s->PosCount)
			    Syntax ("wrong position");
			r->ResIndexOrSeq = Number;
		    } 
		} else
		    Syntax ("name %s is not a name of an object or a variable", Token);
	    }
	    r->Dir = Expect (w_slash) ? ParseDir () : dir_up;
	    FreeMap (vars, 1);
	    continue;
	}

	Unexpected ();
    }

    if (!cells)
	Errx ("missing 'dimensions'");

    ground = (OBJECT *) LookupNamed (objects, "ground");
    if (!ground)
	Errx ("object 'ground' was not defined");

    border = (OBJECT *) LookupNamed (objects, "border");
    if (!border)
	Errx ("object 'border' was not defined");

    {
	NAMED * m;
	OBJECT * o;
	colors = (VCOLOR **) Alloc (sizeof (VCOLOR *) * nobjects);
	for (m = objects->First; m; m = m->Next) {
	    o = (OBJECT *) m->Object;
	    colors [o->Index] = o->color;
	}
    }

    /* 
     * There is a CELL for every pixel including ones belonging to borders.
     * Border CELLs are immutable. Every cell that is free (can be linked to a list)
     * has "Next == Cells". It is unambiguous because cells [0] is a border CELL and
     * would never be placed on a list. For all border CELLs "Next == NULL" is always true
     * and so they would never be placed on a list. 
     */

    INIT_LIST (&curcells);

    {
	int i, j;
	CELL * c;

	for (i = 0, c = cells; i < wxp2; i ++) {
	    c->Index = border->Index;
	    c->Dir = dir_up;
	    c ++;
	}

	for (i = 0; i < wy; i ++) {
	    c->Index = border->Index;
	    c->Dir = dir_up;
	    c ++;
	    for (j = 0; j < wx; j ++, c ++) {
		if (!c->NewIndex) {
		    c->Index = ground->Index;
		    c->Dir = dir_up;
		}
		ADD_TO_LIST (&curcells, c);
	    }
	    c->Index = border->Index;
	    c->Dir = dir_up;
	    c ++;
	}

	for (i = 0; i < wxp2; i ++) {
	    c->Index = border->Index;
	    c->Dir = dir_up;
	    c ++;
	}
    }

    InitView (ground->color, border->color, wxp2, wyp2);

    count = 0;

    {
	int mx [4] [9] = {
	    {
		wxp1,  wxp2,  wxp3,
		-1,     0,     1,
		-wxp3, -wxp2, -wxp1
	    },
	    {
		wxp3,     1, -wxp1,
		wxp2,     0, -wxp2,
		wxp1,    -1, -wxp3
	    },
	    {
		-wxp1, -wxp2, -wxp3,
		1,        0,    -1,
		wxp3,  wxp2,  wxp1
	    },
	    {
		-wxp3,    -1,  wxp1,
		-wxp2,     0,  wxp2,
		-wxp1,     1,  wxp3
	    }
	};

	do {
	    CELL * c, * tc;

	    if (stepping || count >= ProcessEventsThreshold) {
		switch (ProcessEvents (stepping)) {
		  case g_destroy:
		  case g_exit:
		    goto done;
		}
		count = 0;
	    }

	    c = curcells.First;
	    INIT_LIST (&curcells);

	    while (c) {
		RULE * r;
		OBJ_DIR * od;
		OBJ_POS * op;
		SET_POS * sp;
		SET_POS_POS * spp;
		int d;

		tc = c->Next;
		c->Next = cells;

		for (r = rules.First; r; r = r->Next) {
		    for (d = dir_up; d < dir_none; d ++) {
			for (od = r->ObjDir; od; od = od->Next)
			    if ( ((od->Dir + d) & (dir_none - 1)) !=  (c + mx [d] [od->Pos])->Dir ) 
				goto rot;
			for (op = r->ObjPos; op; op = op->Next)
			    if ((c + mx [d] [op->Pos])->Index != op->Index)
				goto rot;
			MaskType rm = 0;
			SET * rs = NULL;
			for (sp = r->SetPos; sp; sp = sp->Next) {
			    MaskType m = MaskAllOnes;
			    for (spp = sp->Positions; spp; spp = spp->Next)
				m &= GET_MASK (
				  sp->Set, (c + mx [d] [spp->Pos])->Index, spp->Seq
				);
			    if (m == 0)
				goto rot;
			    if (r->ResSetPos == sp) {
				rs = sp->Set;
				rm = m;
			    }
			}
			if (!rs)
			    c->NewIndex = r->ResIndexOrSeq;
			else {
			    static int map [16] = {
				-1, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0 
			    };
			    int n = 0, nr;
			    goto chkb;
			    do {
				rm = rm >> 4;
				n += 4;
			    chkb:
				nr = map [(int) (rm & 0x0f)];
			    } while (nr == -1);
			    n += nr;
			    c->NewIndex = rs->Objects [n * rs->PosCount + r->ResIndexOrSeq];
			}
			c->NewDir = (d + r->Dir) & (dir_none - 1);
			ADD_TO_LIST (&curcells, c);
			goto rok;
		    rot:;
		    }
		}
	    rok:
		c = tc;
	    }
	    c = curcells.First;
	    INIT_LIST (&curcells);
	    while (c) {
		size_t l = c - cells;
		tc = c->Next;
		c->Index = c->NewIndex;
		c->Dir = c->NewDir;
		ViewDrawPoint (colors [c->Index], (int) (l % wxp2), (int)(l / wxp2), c->Dir);
		/*
		 * Every cells that has "Next != cells" is either:
		 * 1) Belongs to a border.
		 * 2) Is on the old list and will be handled in a future.
		 * 3) Is on the new list already.
		 */
#define ENQ(x) \
do { \
  CELL * _x = x; \
  if (_x->Next == cells) \
    ADD_TO_LIST (&curcells, _x); \
} while (0)
		ENQ (c + wxp1);
		ENQ (c + wxp2);
		ENQ (c + wxp3);
		ENQ (c - 1);
		ADD_TO_LIST (&curcells, c);
		ENQ (c + 1);
		ENQ (c - wxp3);
		ENQ (c - wxp2);
		ENQ (c - wxp1);
		count ++;
#undef ENQ
		c = tc;
	    }

	} while (curcells.First);
    }

    Bell ();

    for (;;)
	switch (ProcessEvents (1)) {
	  case g_destroy:
	  case g_exit:
	    goto done;
	}

 done:
    DeinitGraphics ();
    return 0;
}

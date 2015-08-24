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

#define DECLARE_LIST(t) typedef struct { t * First; t ** Ptr; } t ## _LIST;

#define INIT_LIST(l) \
do { \
    typeof (l) _l = l; \
    _l->First = NULL; \
    _l->Ptr = &_l->First; \
} while (0)

#define ADD_TO_LIST(l,e) \
do { \
    typeof (l) _l = l; \
    typeof (e) _e = e; \
    _e->Next = NULL; \
    *_l->Ptr = _e; \
    _l->Ptr = &_e->Next; \
} while (0)

#define ADD_TO_LIST_P(p,e) \
do { \
    typeof (p) _p = p; \
    typeof (e) _e = e; \
    _e->Next = NULL; \
    **_p = _e; \
    *_p = &_e->Next; \
} while (0)

typedef struct NAMED NAMED;

struct NAMED {
    NAMED * Next;
    char * Name;
    void * Object;
};

typedef struct {
    NAMED * First;
    NAMED ** Ptr;
    int (*StrCmp) (const char *s1, const char *s2);
} MAP;

typedef struct ONE_INT ONE_INT;

struct ONE_INT {
    ONE_INT * Next;
    int Value;
};

typedef struct INT_LIST INT_LIST;

struct INT_LIST {
    ONE_INT * First;
    ONE_INT ** Ptr;
    size_t Count;
};

void SetProgName (const char * progname);
const char * GetProgName (void);
void * Alloc (size_t sz);
char * DupStr (const char * Str);
void __attribute__((__noreturn__)) Err (char * Str, ...);
void __attribute__((__noreturn__)) Errx (char * Str, ...);
MAP * CreateMap (int ci);
void * LookupNamed (MAP * map, char * Name);
void AddNamed (MAP * map, char * Name, void * Object);
void FreeMap (MAP * map, int FreeObjs);
INT_LIST * CreateIntList (void);
void AddInt (INT_LIST * List, int Value);
int * IntListToArray (INT_LIST * List);
void FreeIntList (INT_LIST * List);

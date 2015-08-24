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

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "util.h"

static const char * ProgName;

void SetProgName (const char * progname) {
    ProgName = progname;
}

const char * GetProgName (void) {
    return ProgName;
}

void  __attribute__((__noreturn__)) Err (char * Str, ...) {
    int e = errno;

    va_list args;

    fprintf (stderr, "%s: ", GetProgName ());

    va_start (args, Str);
    vfprintf (stderr, Str, args);

    fprintf (stderr, ": %s\n", strerror (e));
    
    exit (1);
}

void  __attribute__((__noreturn__)) Errx (char * Str, ...) {
    va_list args;

    fprintf (stderr, "%s: ", GetProgName ());

    va_start (args, Str);
    vfprintf (stderr, Str, args);

    fputc ('\n', stderr);
    
    exit (1);
}

void * Alloc (size_t sz) {
    void* p = malloc (sz);
    if (!sz)
	Err ("Out of memory");
    memset (p, 0, sz);
    return p;
}

char * DupStr (const char * Str) {
    size_t l = strlen (Str) + 1;
    char * s = (char *) Alloc (l);
    memcpy (s, Str, l);
    return s;
}

MAP * CreateMap (int ci) {
    MAP * map = (MAP *) Alloc (sizeof (MAP));
    INIT_LIST (map);
    map->StrCmp = ci ? strcasecmp : strcmp;
    return map;
}

void * LookupNamed (MAP * map, char * Name) {
    NAMED * m;
    for (m = map->First; m; m = m->Next)
	if (map->StrCmp (m->Name, Name) == 0)
	    return m->Object;
    return NULL;
}

void AddNamed (MAP * map, char * Name, void * Object) {
    NAMED * m = (NAMED *) Alloc (sizeof (NAMED));
    m->Name = DupStr (Name);
    m->Object = Object;
    ADD_TO_LIST (map, m);
}

void FreeMap (MAP * map, int FreeObjs) {
    NAMED * m, * tm;

    m = map->First;
    free (map);
    while (m) {
	tm = m->Next;
	free (m->Name);
	if (FreeObjs)
	    free (m->Object);
	free (m);
	m = tm;
    }
}

INT_LIST * CreateIntList (void) {
    INT_LIST * l = (INT_LIST *) Alloc (sizeof (INT_LIST));
    INIT_LIST (l);
    return l;
}

void AddInt (INT_LIST * List, int Value) {
    ONE_INT * ie = (ONE_INT *) Alloc (sizeof (ONE_INT));
    ie->Value = Value;
    ADD_TO_LIST (List, ie);
    List->Count ++;
}

int * IntListToArray (INT_LIST * List) {
    int * a, * pa;
    ONE_INT * ie;

    a = (int *) Alloc (List->Count * sizeof (int));

    for (pa = a, ie = List->First; ie; pa ++, ie = ie->Next)
	*pa = ie->Value;

    return a;
}

void FreeIntList (INT_LIST * List) {
    ONE_INT * ie, * tie;

    ie = List->First;
    free (List);
    while (ie) {
	tie = ie->Next;
	free (ie);
	ie = tie;
    }
}

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/param.h>

#include "scanner.h"
#include "util.h"

char Token [MAXPATHLEN + 1];
int Number;

typedef struct SOURCE SOURCE;

struct SOURCE {
    SOURCE* Prev;
    FILE* File;
    char * FileName;
    int Line;
};

static SOURCE * Src = NULL;
static int TokenType, Expected = 0, Reinit = 0;

/*  Mind prefetched token. */
void OpenScanner (char * Fn) {
    SOURCE * tsrc;

    if (!Reinit) {
	tsrc = (SOURCE*) Alloc (sizeof (SOURCE));
	tsrc->Prev = Src;
	Src = tsrc;
    } else {
	Reinit = 0;
	Expected = 0;
	free (Src->FileName);
    }
    Src->FileName = DupStr (Fn);
    Src->Line = 1;
    Src->File = fopen (Src->FileName, "r");
    if (!Src->File)
	Err ("cannot open %s", Src->FileName);
}

static inline void GetToken (void) {
    int i, c;
    char ** wa;
    SOURCE * tsrc;

    do {
    again:
	c = fgetc (Src->File);
	if (ferror (Src->File))
	    have_error:	Err ("error reading %s", Src->FileName);
	if (c == -1) {
	have_eof:
	    fclose (Src->File);
	    if (Src->Prev) {
		free (Src->FileName);
		tsrc = Src;
		Src = Src->Prev;
		free (tsrc);
		goto again;
	    }
	    Reinit = 1;
	    TokenType = w_eof;
	    return;
	}
	if (c == '\n') {
	    Src->Line ++;
	    goto again;
	}
	if (c == '#') {
	    do {
		c = fgetc (Src->File);
		if (ferror (Src->File))
		    goto have_error;
		if (c == -1)
		    goto have_eof;
	    } while (c != '\n');
	    Src->Line ++;
	    goto again;
	}
    } while (isspace (c));

    if (c >= '0' && c <= '9') {
	Number = c - '0';
	for (;;) {
	    c = fgetc (Src->File);
	    if (ferror (Src->File))
		goto have_error;
	    if (c < '0' || c > '9') {
		if (c != -1 && (c == '_' || isalpha (c)))
		    Syntax ("invalid integer");
		ungetc (c, Src->File);
		TokenType = w_number;
		return;
	    }
	    if (Number > INT_MAX / 10)
		iover: Syntax ("integer overflow");
	    Number *= 10;
	    c -= '0';
	    Number += c;
	    if (Number < 0)
		goto iover;
	}
    }

    if (c == '"') {
	i = 0;
        for (;;) {
	    c = fgetc (Src->File);
	    if (ferror (Src->File))
		goto have_error;
	    if (c == -1 || c == '\n')
		have_unterm: Syntax ("unterminated quoted string");
	    if (c == '"') {
		Token [i] = 0;
		TokenType = w_string;
		return;
	    }
	    if (i > (int) sizeof Token - 1)
		Syntax ("too long quoted string");
	    if (c == '\\') {
		c = fgetc (Src->File);
		if (ferror (Src->File))
		    goto have_error;
		switch (c) {
		  case -1:
		  case '\n':
		    goto have_unterm;
		  case '\\':
		    c = '\\';
		    break;
		  case '"':
		    c = '"';
		    break;
		  case 't':
		    c = '\t';
		    break;
		  case 'r':
		    c = '\r';
		    break;
		  case 'n':
		    c = '\n';
		    break;
		  case 'b':
		    c = '\b';
		    break;
		  default:
		    Syntax ("invalid escape sequence");
		}
	    }
	    Token [i] = (char) c;
	    i ++;
	}	
    }

    if (c != '_' && !isalpha (c)) {
	for (wa = SymbolTable, TokenType = w_app; *wa; wa ++, TokenType ++)
	    if ((*wa) [0] == c)
		return;
	Syntax ("invalid character '%c'", c);
    }

    i = 0;
    
    do {
	if (i > (int) sizeof Token - 1)
	    Syntax ("too long token");
	Token [i] = (char) c;
	i ++;
	c = fgetc (Src->File);
	if (ferror (Src->File))
	    goto have_error;
    } while (c == '_' || (c >= '0' && c <= '9') || isalpha (c));

    ungetc (c, Src->File);

    Token [i] = 0;

    for (wa = SymbolTable, TokenType = w_app; *wa; wa ++, TokenType ++)
	if (strcmp (*wa, Token) == 0)
	    return;

    TokenType = w_name;
}

int Expect (int Type) {
    if (Expected == 0)
	GetToken ();
    if (Type != TokenType) {
	Expected |= (int) 1 << Type;
	return 0;
    }
    Expected = 0;
    return 1;
}

void MustBe (int Type) {
    if (!Expect (Type))
	Unexpected ();
}

void __attribute__((__noreturn__)) Syntax (char * Str, ...) {
    va_list args;

    fprintf (stderr, "%s: %s(%u): ", GetProgName (), Src->FileName, Src->Line);

    va_start (args, Str);
    vfprintf (stderr, Str, args);

    fputc ('\n', stderr);

    exit (1);
}

void __attribute__((__noreturn__)) Unexpected (void) {
    static char * strs [] = { "end of file", "numeral", "quoted string", "name"};

    int nf, n;
    char c;

    fprintf (stderr, "%s: %s(%u): expected: ", GetProgName (), Src->FileName, Src->Line);

    nf = n = 0;
    for (n = 0; Expected; n ++, Expected >>= 1)
	if (Expected & 1) {
	    if (nf)
		fputc (' ', stderr);
	    nf = 1;
	    if (n < w_app)
		fprintf (stderr, "%s", strs [n]);
	    else {
		c = SymbolTable [n - w_app] [0];
		if (c != '_' && !isalpha (c)) 
		    fprintf (stderr, "'%c'", c);
		else
		    fprintf (stderr, "\"%s\"", SymbolTable [n - w_app]);
	    }
	}

    fputc ('\n', stderr);

    exit (1);
}

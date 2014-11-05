/***************************************************************************
  Copyright (C) 2014 Christoph Reichenbach


 This program may be modified and copied freely according to the terms of
 the GNU general public license (GPL), as long as the above copyright
 notice and the licensing information contained herein are preserved.

 Please refer to www.gnu.org for licensing details.

 This work is provided AS IS, without warranty of any kind, expressed or
 implied, including but not limited to the warranties of merchantability,
 noninfringement, and fitness for a specific purpose. The author will not
 be held liable for any damage caused by this work or derivatives of it.

 By using this source code, you agree to the licensing terms as stated
 above.


 Please contact the maintainer for bug reports or inquiries.

 Current Maintainer:

    Christoph Reichenbach (CR) <creichen@gmail.com>

***************************************************************************/

#ifndef _ATTOL_LEXER_SUPPORT_H
#define _ATTOL_LEXER_SUPPORT_H

#include "chash.h"

/**
 * Translates an escaped character into the matching control code
 * (e.g., 'n' into a newline '\n')
 *
 * @param c Character to translate
 * @return Matching control character, if applicable, otherwise `c'
 */
char
unescape_char(char c);

/**
 * Translates a string with escape characters into a mallocd unescaped string
 *
 * @param text String to unescape
 * @param yyerror Function pointer to error message handler
 * @return unescaped string (freshly mallocd)
 */
char*
unescape_string(char *text, void (*yyerror)(const char *msg));

/**
 * Maps a string into a globally (heap-)unique string
 *
 * @param id Any string
 * @return A string with exactly identical contents but possibly a
 * different (unique, for these contents) memory address
 */
char*
mk_unique_string(char *id);

#endif // !defined(_ATTOL_LEXER_SUPPORT_H)

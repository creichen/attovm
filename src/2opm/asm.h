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

// Principal declarations and definitions for the assembler

#ifndef _ATTOL_2OPM_ASM_H
#define _ATTOL_2OPM_ASM_H

typedef union {
	signed long int num;
	char  *str;
} yylval_t;

extern yylval_t yylval;

// Lexer/Tokeniser outputs

#define T_ID		0x101
#define T_INT		0x102
#define T_STR		0x103

#define T_S_DATA	0x110
#define T_S_TEXT	0x111
#define T_S_BYTES	0x112
#define T_S_WORDS	0x113
#define T_S_ASCIIZ	0x114

#endif // !defined(_ATTOL_2OPM_ASM_H)

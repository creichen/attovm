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

#ifndef REGISTERS_H_
#define REGISTERS_H_

#define REGISTERS_NR 16

// Special registers
#define REGISTER_V0 0 // return value
#define REGISTER_SP 4
#define REGISTER_FP 5
#define REGISTER_GP 11

#define REGISTERS_CALLEE_SAVED_NR 6
#define REGISTERS_ARGUMENT_NR 6

typedef struct {
	char *intel; // real name
	char *mips;  // pseudo-name for students more familiar with MIPS naming
} regname_t;

extern regname_t register_names[REGISTERS_NR];
extern int registers_callee_saved[REGISTERS_CALLEE_SAVED_NR];
extern int registers_argument_nr[REGISTERS_ARGUMENT_NR];


#endif // !defined(REGISTERS_H_)

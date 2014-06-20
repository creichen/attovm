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

#include "registers.h"

regname_t register_names[REGISTERS_NR] = {
	{"%rax", "$v0"}, // 0
	{"%rcx", "$a3"},
	{"%rdx", "$a2"},
	{"%rbx", "$s0"},
	{"%rsp", "$sp"}, // 4
	{"%rbp", "$fp"},
	{"%rsi", "$a1"},
	{"%rdi", "$a0"},
	{"%r8",  "$a4"}, // 8
	{"%r9",  "$a5"},
	{"%r10", "$t0"}, 
	{"%r11", "$t1"},
	{"%r12", "$s1"}, // 12
	{"%r13", "$s2"},
	{"%r14", "$s3"},
	{"%r15", "$gp"}};

int registers_callee_saved[REGISTERS_CALLEE_SAVED_NR] = {
	3, 12, 13, 14
};

int registers_temp[REGISTERS_TEMP_NR] = {
	10, 11
};

int registers_argument[REGISTERS_ARGUMENT_NR] = {
	7, 6, 2, 1, 8, 9
};

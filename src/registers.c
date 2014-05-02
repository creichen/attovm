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
	{"%rax", "$v0"},
	{"%rcx", "$a3"},
	{"%rdx", "$a2"},
	{"%rbx", "$s0"},
	{"%rsp", "$sp"},
	{"%rbp", "$fp"},
	{"%rsi", "$a1"},
	{"%rdi", "$a0"},
	{"%r8",  "$a4"},
	{"%r9",  "$a5"},
	{"%r10", "$s1"}, 
	{"%r11", "$gp"},
	{"%r12", "$s2"},
	{"%r13", "$s3"},
	{"%r14", "$s4"},
	{"%r15", "$s5"}};

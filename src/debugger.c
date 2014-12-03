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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdbool.h>
#include <signal.h>

#ifdef __MACH__
#  define _DARWIN_C_SOURCE // for sys/types.h
#  include <mach/mach_init.h>
#  include <mach/mach_vm.h>
#  include <mach/mach.h>
#  include <mach/machine/thread_state.h>
#  include <mach/machine/thread_status.h>
void
debug(unsigned char *asm_start, unsigned char *asm_end,
	      void (*entry_point)())
{
	fprintf(stderr, "<Debugging not supported on OS X>\n");
	entry_point();
}
#else

#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <unistd.h>

#include "address-store.h"
#include "registers.h"
#include "assembler.h"
#include "error.h"
#include "debugger.h"

typedef unsigned char byte;

#ifdef __MACH__
#  define PTRACE_ATTACH		PT_ATTACH
#  define PTRACE_SINGLESTEP	PT_STEP
// FIXME: this one's not working on Mach.  Have to use software breakpoints (0xcc) instead.
#endif

#define PTRACE(a, b, c, d) if (-1 == ptrace(a, b, c, d)) { fprintf(stderr, "ptrace failure in %d\n", __LINE__); perror(#a); return; }

static struct machine_state {
	bool running;
	bool failure;
	int pid;
	debugger_config_t *config;
	signed long long registers[REGISTERS_NR];
	byte *pc;
	byte *initial_stack;
} status = {
	.running = false,
	.failure = false
};

#define MAX_INSN_SIZE 32

#define DEBUG_COMMAND_STEP	1
#define DEBUG_COMMAND_QUIT	2

static int multistep_count = 0;
static byte *breakpoint = NULL;

#define MESSAGE_FORMAT "\033[33m"
#define END_FORMAT "\033[0m"

static void
start_message(void)
{
	fprintf(stdout, MESSAGE_FORMAT);
}

static void
start_disasm(void)
{
	fprintf(stdout, "\033[32m");
}

static void
stop_message(void)
{
	fprintf(stdout, END_FORMAT);
}

static void
message(const char *fmt, ...)
{
	va_list args;
	start_message();
	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	stop_message();
	printf("\n");
	va_end(args);
}

static void
print_memmap(int pid)
{
	char s[1024];
	start_message();
	sprintf(s, "cat /proc/%d/maps", pid);
	system(s);
	stop_message();
}

#ifdef __MACH__
void
getdata(pid_t child, unsigned long long addr, byte *str, int len)
{
        vm_size_t read_bytes;
	if (KERN_SUCCESS != vm_read_overwrite(child, addr, len, (vm_address_t) str, &read_bytes)) {
                perror("vm_read_overwrite");
                return;
        }

        if (read_bytes != len) {
                status.config->error("vm_read_overwrite() read %d instead of %d bytes",
				     len, read_bytes);
        }
}

void
putdata(pid_t child, unsigned long long addr, byte *str, int len)
{
        if (KERN_SUCCESS != vm_write(child, addr, (vm_address_t) str, len)) {
                perror("vm_write");
                return;
        }
}

static void
get_registers_for_process(int process, struct machine_state *state)
{
	byte **pc = &state->pc;
	signed long long *regs = &(state->registers[0]);
	x86_thread_state64_t* x86_thread_state64;

	unsigned int count = MACHINE_THREAD_STATE_COUNT;

	thread_get_state(process, x86_THREAD_STATE64,
			 (thread_state_t)&x86_thread_state64,
			 &count);

	regs[0] = x86_thread_state64->__rax;
	regs[1] = x86_thread_state64->__rcx;
	regs[2] = x86_thread_state64->__rdx;
	regs[3] = x86_thread_state64->__rbx;
	regs[4] = x86_thread_state64->__rsp;
	regs[5] = x86_thread_state64->__rbp;
	regs[6] = x86_thread_state64->__rsi;
	regs[7] = x86_thread_state64->__rdi;
	regs[8] = x86_thread_state64->__r8;
	regs[9] = x86_thread_state64->__r9;
	regs[10] = x86_thread_state64->__r10;
	regs[11] = x86_thread_state64->__r11;
	regs[12] = x86_thread_state64->__r12;
	regs[13] = x86_thread_state64->__r13;
	regs[14] = x86_thread_state64->__r14;
	regs[15] = x86_thread_state64->__r15;

	*pc = (byte *) x86_thread_state64->__rip;
}

#else // not __MACH__

void
getdata(pid_t child, unsigned long long addr, byte *str, int len)
{
	while (len >= sizeof(long)) {
		long v = ptrace(PTRACE_PEEKDATA, child,
					addr, NULL);
		memcpy(str, &v, sizeof(long));
		addr += sizeof(long);
		str += sizeof(long);
		len -= sizeof(long);
	}
	if (len) {
		long v = ptrace(PTRACE_PEEKDATA, child,
					addr, NULL);
		memcpy(str, &v, len);
	}
}

void
putdata(pid_t child, unsigned long long addr, byte *str, int len)
{
	while (len >= sizeof(long)) {
		long v;
		memcpy(&v, str, sizeof(long));
		ptrace(PTRACE_POKEDATA, child, addr, v);
		addr += sizeof(long);
		str += sizeof(long);
		len -= sizeof(long);
	}
	if (len) {
		long v = ptrace(PTRACE_PEEKDATA, child, addr, v);
		memcpy(&v, str, len);
		ptrace(PTRACE_POKEDATA, child, addr, v);
	}
}

static void
get_registers_for_process(int process, struct machine_state *state)
{
	byte **pc = &state->pc;
	signed long long *regs = &(state->registers[0]);
	struct user_regs_struct uregs;
	PTRACE(PTRACE_GETREGS, process, &uregs, &uregs);

	*pc = (byte *) uregs.rip;
	regs[0] = uregs.rax;
	regs[1] = uregs.rcx;
	regs[2] = uregs.rdx;
	regs[3] = uregs.rbx;
	regs[4] = uregs.rsp;
	regs[5] = uregs.rbp;
	regs[6] = uregs.rsi;
	regs[7] = uregs.rdi;
	regs[8] = uregs.r8;
	regs[9] = uregs.r9;
	regs[10] = uregs.r10;
	regs[11] = uregs.r11;
	regs[12] = uregs.r12;
	regs[13] = uregs.r13;
	regs[14] = uregs.r14;
	regs[15] = uregs.r15;
}

#endif // not __MACH

#define MAX_INPUT_2OPMDEBUG 4095
static char last_command[MAX_INPUT_2OPMDEBUG + 1] = "";

static int
cmd_help(char *_);

static int
cmd_quit(char *_)
{
	return DEBUG_COMMAND_QUIT;
}

static int
cmd_step(char * s)
{
	int steps_nr = 1;
	if (s) {
		steps_nr = atoi(s);
	}

	if (steps_nr > 0) {
		multistep_count = steps_nr - 1;
	}

	return DEBUG_COMMAND_STEP;
}

static int
cmd_registers(char * s)
{
	// translate registers for naming scheme
	int register_table[REGISTERS_NR] = {
		0,			// $v0
		7, 6, 2, 1, 8, 9,	// $a0..$a5
		10, 11,			// $t0, $t1
		3, 12, 13, 14,		// $s0..$s3
		4,			// $sp
		5,			// $fp
		15			// $gp
	};
	for (int i = 0; i < REGISTERS_NR; i++) {
		int k = register_table[i];
		printf ("  %-5s  %016llx  %lld\n", register_names[k].mips, status.registers[k], status.registers[k]);
	}
	return 0;
}

static int
cmd_cont(char *s)
{
	multistep_count = -1;
	return DEBUG_COMMAND_STEP;
}

static int
cmd_break(char *s)
{
	if (!s) {
		message("Must specify breakpoint name or address");
		return 0;
	}
	if (status.config->name_lookup) {
		breakpoint = status.config->name_lookup(s);
	} else {
		breakpoint = NULL;
	}
	if (!breakpoint) {
		char *end;
		if (s[0] == '0' && s[1] == 'x') {
			s += 2;
		}
		breakpoint = (byte *) strtoull(s, &end, 16);
		if (*end || !breakpoint) {
			message("I don't understand address `%s', sorry", s);
			breakpoint = NULL;
			return 0;
		}
	}
	multistep_count = -1;
	message("==> %p", breakpoint);
	return DEBUG_COMMAND_STEP;
}

/**
 * Do any registers point to this address?  If so, print them to dest_buffer and return `true', otherwise `false'
 */
static bool
have_any_registers_at(char *dest_buffer, byte *addr)
{
	char *dest = dest_buffer;
	bool match = false;
	for (int i = 0; i < REGISTERS_NR; i++) {
		if ((byte *) status.registers[i] == addr) {
			if (match) {
				dest += sprintf(dest, ", ");
			}
			dest += sprintf(dest, "%s", register_names[i].mips);
			match = true;
		}
	}
	return match;
}

#define MAX_REGISTER_OBSERVATIONS 7

static int
cmd_static(char *s) {
	// hilight data if a register happens to point at it
	int registersets_observed = 0;
	char registerset_observations[MAX_REGISTER_OBSERVATIONS][256];

	byte *data = status.config->static_section;
	size_t data_section_size = status.config->static_section_size;

	char pre_buffer[1024], post_buffer[1024];
	char *pre = NULL, *post = NULL;
	for (int i = 0; i < data_section_size; i++) {
		if (!(i & 0xf)) {
			start_message();
			printf("[%p]\t", data);
			pre_buffer[0] = '\0';
			post_buffer[0] = '\0';
			pre = pre_buffer;
			post = post_buffer;
			stop_message();
		}

		getdata(status.pid, (unsigned long long) data, data, 1);

		bool must_reset_font = false;
		if (registersets_observed < MAX_REGISTER_OBSERVATIONS) {
			if (have_any_registers_at(registerset_observations[registersets_observed], data)) {
				++registersets_observed;
				must_reset_font = true;
				pre += sprintf(pre, "\033[1;4%dm", registersets_observed);
				post += sprintf(post, "\033[1;4%dm", registersets_observed);
			}
		}

		pre += sprintf(pre, "%02x", *data);
		post += sprintf(post, "%c", (*data >= ' ' && *data < 127) ? *data : '.');
		if (must_reset_font) {
			pre += sprintf(pre, END_FORMAT MESSAGE_FORMAT);
			post += sprintf(post, END_FORMAT MESSAGE_FORMAT);
		}
		pre += sprintf(pre, " ");

		if ((i & 0xf) == 7) {
			printf(" ");
		} else if ((i & 0xf) == 0xf) {
			message("%s [%s]", pre_buffer, post_buffer);
		}

		++data;
	}
	int missing_chars = data_section_size & 0xf;
	if (missing_chars) {
		int filler = missing_chars * 3 + ((missing_chars < 7) ? 1 : 0);
		message("%s%-*s [%s]", pre_buffer, filler, "", post_buffer);
	}

	for (int i = 0; i < registersets_observed; i++) {
		message(" - \033[1;4%dm%s" END_FORMAT,
		       i + 1,
		       registerset_observations[i]);
	}
	return 0;
}


#define MAX_STACK 256

static int
cmd_stack(char *s)
{
	unsigned long long *stack = (unsigned long long *) status.initial_stack;
	int stack_entries_nr;
	unsigned long long *stack_end = (unsigned long long *) status.registers[REGISTER_SP];

	if (stack_end > stack) {
		message("Stack has shrunk above its original start at %p, nothing to show", stack);
		return 0;
	} else if ((stack - stack_end) > MAX_STACK) {
		stack_entries_nr = 256;
	} else {
		stack_entries_nr = (stack - stack_end) + 1;
	}


	//fprintf(stderr, "%d stack entries (%p to %p)\n", stack_entries_nr, stack,>" stack_end);

	for (int i = 0; i < stack_entries_nr; i++) {
		unsigned long long data;
		getdata(status.pid, (unsigned long long) stack, (byte *) &data, sizeof (unsigned long long));

		start_message();
		printf("  %p: \033[1m %016llx  %-22lld", stack,
		       data, (signed long long) data);
		stop_message();

		char regs_here[1024] = "\0";
		if (have_any_registers_at(regs_here, (byte *) stack)) {
			start_message();
			printf(" <--- %s", regs_here);
		}
		stop_message();
		printf("\n");
		--stack;
	}
	return 0;
}

static struct {
	char *name;
	char *descr;
	int (*f)(char *); // returns nonzero DEBUG_COMMAND (optionally); takes optional args
} commands[] = {
	{ "help", "print help message", &cmd_help },
	{ "quit", "stop executing and exit", &cmd_quit },
	{ "s", "step forward a single step; \"s <n>\" steps forward n steps", &cmd_step },
	{ "r", "print registers", &cmd_registers },
	{ "regs", "print registers", &cmd_registers },
	{ "break", "continue until label or address (passed as argument)", &cmd_break },
	{ "cont", "continue executing", &cmd_cont },
	{ "stack", "print out the current stack", &cmd_stack },
	{ "static", "print out the contents of static memory", &cmd_static },
	//	{ "", "", &cmd_ },
	{ NULL, NULL, NULL }
};

static int
cmd_help(char *_)
{
	start_message();
	printf("Usage:\n");
	for (int i = 0; commands[i].name; i++) {
		printf("  %-12s  %s\n", commands[i].name, commands[i].descr);
	}
	printf("Empty input will repeat the previous command.\n");
	stop_message();
	return 0;
}

/**
 * Read user commands until the user instructs us to proceed
 */
static int
debug_command()
{
	breakpoint = NULL;
	static char command[MAX_INPUT_2OPMDEBUG + 1] = "";

	while (true) {
		// update instruction in local memory for disassembly
		int max_insn_size = MAX_INSN_SIZE;
		long long int bytes_until_pc_end = status.config->debug_region_end - status.pc;
		if (bytes_until_pc_end < max_insn_size) {
			max_insn_size = bytes_until_pc_end;
		}
		// update local (in-process) copy of instruction memory from child
		getdata(status.pid, (unsigned long long) status.pc, status.pc, max_insn_size);

		start_disasm();
		if (addrstore_get_prefix(status.pc)) {
			printf("%s:\n", addrstore_get_suffix(status.pc));
		}
		printf("\t");
		disassemble_one(stdout, status.pc, max_insn_size);
		printf("\n");
		start_message();
		printf("%p%s>", status.pc, status.running? "" : ":HALTED");
		stop_message();
		printf(" ");

		

		if (fgets(command, MAX_INPUT_2OPMDEBUG, stdin) == (char *) EOF) {
			// end-of-file
			return DEBUG_COMMAND_QUIT;
		}
		char *tail = command + strlen(command);
		// Remove trailing whitespace:
		while (tail > command && isspace(tail[-1])) {
			--tail;
			*tail = '\0';
		}

		if (!command[0]) { // empty? repeat last command
			strcpy(command, last_command);
		} else {
			strcpy(last_command, command);
		}

		char *args = strchr(command, ' ');

		if (args) {
			*args = '\0';
			++args;
			while (*args == ' ') {
				++args;
			}
		}

		bool match = false;
		for (int i = 0; commands[i].name; i++) {
			if (!strcasecmp(commands[i].name, command)) {
				match = true;
				int retcode = commands[i].f(args);
				if (retcode)
					return retcode;
				break;
			}
		}
		if (!match) {
			message("Unknown command `%s'", command);
		}
	}
}

void
debug(debugger_config_t *config, void (*entry_point)())
{
	status.config = config;
	static int wait_for_me = 0; // used to sync up child process
	
	int child;
	//	signal(SIGSTOP, &sighandler);

	status.initial_stack = (byte *) &child;
	while (((unsigned long long) status.initial_stack) & 0x7ll) {
		// align, just in case
		--status.initial_stack;
	}

	message("Starting up debugger; enter `help' for help.");

	if ((child = fork())) {
		int child_status;

		// parent
		PTRACE(PTRACE_ATTACH, child, NULL, 0);
		wait(&child_status);
		wait_for_me = 1;
		putdata(child, (unsigned long long) &wait_for_me, (byte *) &wait_for_me, sizeof(wait_for_me));
		do {
break;
			get_registers_for_process(child, &status);
			if ((void *) status.pc != entry_point) {
				break;
			}
			PTRACE(PTRACE_SINGLESTEP, child, NULL, 0);
#ifdef __MACH__
			// FIXME: no internal consistency check on Mach kernels
#else
			if (child_status >> 8 == (SIGTRAP | (PTRACE_EVENT_EXIT<<8))) {
				status.config->error("Unexpected early exit (internal error?)");
				return;
			}
#endif
			wait(&child_status);
		} while (true);

		status.pid = child;
		status.running = true;

		void *last_safe_pc = NULL;

		while (true) {
			if (WIFEXITED(child_status)) {
				message("Program finished executing.");
				status.running = false;
				return;
			} else if (WIFSTOPPED(child_status)
				   && WSTOPSIG(child_status) != SIGSTOP
				   && WSTOPSIG(child_status) != SIGTRAP) {
				message("Program halted on signal %d, %s.\n", WSTOPSIG(child_status), strsignal(WSTOPSIG(child_status)));
				status.running = false;
				status.failure = true;
			}
			get_registers_for_process(child, &status);

			if (status.pc == breakpoint) {
				multistep_count = 0;
				message("(breakpoint)");
			}

			if (status.failure) {
				// Restore to some PC that the programmer understands
				if (status.pc < status.config->debug_region_start || status.pc > status.config->debug_region_end) {
					message("$pc=%p at time of error (outside of your code)", status.pc);
					message("Setting $pc=%p (last instruction in your code)", last_safe_pc);
					message("NOTE: registers/memory may have changed since this instruction was executed!", last_safe_pc);
				}
				status.pc = last_safe_pc;
			}

			if (status.failure
			    || (status.pc >= status.config->debug_region_start
				&& status.pc <= status.config->debug_region_end
				&& status.config->is_instruction(status.pc))) {
				last_safe_pc = status.pc;
				if (status.failure || !multistep_count) {
					int command = debug_command();
					switch (command) {
					case DEBUG_COMMAND_QUIT:
						kill(9, child);
						//PTRACE(PTRACE_DETACH, child, NULL, NULL);
						return;
					default:
						;
					}
				} else {
					--multistep_count;
				}
			}

			PTRACE(PTRACE_SINGLESTEP, child, NULL, 0);
			wait(&child_status);
		}
	} else {
		// child
		while (!wait_for_me);
		entry_point();
	}
}
#endif

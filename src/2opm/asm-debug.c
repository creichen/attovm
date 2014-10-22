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
#include <string.h>

#include <sys/user.h>
#include <sys/wait.h>
#include <sys/ptrace.h>

#include "../registers.h"

#include "asm.h"

static void
print_memmap(int pid)
{
	char s[1024];
	sprintf(s, "cat /proc/%d/maps", pid);
	system(s);
}


static void
decode_regs(unsigned long long *regs, void **pc, struct user_regs_struct *uregs)
{
	*pc = (void *) uregs->rip;
	regs[0] = uregs->rax;
	regs[1] = uregs->rcx;
	regs[2] = uregs->rdx;
	regs[3] = uregs->rbx;
	regs[4] = uregs->rsp;
	regs[5] = uregs->rbp;
	regs[6] = uregs->rsi;
	regs[7] = uregs->rdi;
	regs[8] = uregs->r8;
	regs[9] = uregs->r9;
	regs[10] = uregs->r10;
	regs[11] = uregs->r11;
	regs[12] = uregs->r12;
	regs[13] = uregs->r13;
	regs[14] = uregs->r14;
	regs[15] = uregs->r15;
}

// putdata/getdata by Pradeep Padala
void
getdata(pid_t child, unsigned long long addr, char *str, int len)
{
	char *laddr;
	int i, j;
	union u {
		long val;
		char chars[sizeof(long)];
	}data;
	i = 0;
	j = len / sizeof(long);
	laddr = str;
	while(i < j) {
		data.val = ptrace(PTRACE_PEEKDATA, child,
				  addr + i * 4, NULL);
		memcpy(laddr, data.chars, sizeof(long));
		++i;
		laddr += sizeof(long);
	}
	j = len % sizeof(long);
	if(j != 0) {
		data.val = ptrace(PTRACE_PEEKDATA, child,
                          addr + i * 4, NULL);
		memcpy(laddr, data.chars, j);
	}
	str[len] = '\0';
}

void putdata(pid_t child, unsigned long long addr, char *str, int len)
{
	char *laddr;
	int i, j;
	union u {
		long val;
		char chars[sizeof(long)];
	}data;
	i = 0;
	j = len / sizeof(long);
	laddr = str;
	while(i < j) {
		memcpy(data.chars, laddr, sizeof(long));
		ptrace(PTRACE_POKEDATA, child,
		       addr + i * 4, data.val);
		++i;
		laddr += sizeof(long);
	}
	j = len % sizeof(long);
	if(j != 0) {
		memcpy(data.chars, laddr, j);
		ptrace(PTRACE_POKEDATA, child,
		       addr + i * 4, data.val);
	}
}

#define PTRACE(a, b, c, d) if (-1 == ptrace(a, b, c, d)) { fprintf(stderr, "ptrace failure in %d\n", __LINE__); perror(#a); return; }

void
debug(buffer_t *buffer, void (*entry_point)())
{
	unsigned long long debug_region_start = (unsigned long long) buffer_entrypoint(*buffer);
	unsigned long long debug_region_end = debug_region_start + buffer_size(*buffer);

	static int wait_for_me = 0; // used to sync up child process
	
	int child;
	//	signal(SIGSTOP, &sighandler);

	if ((child = fork())) {
		struct user_regs_struct regs_struct;
		unsigned long long regs[16];
		void *pc;
		int status;

		// parent
		PTRACE(PTRACE_ATTACH, child, NULL, NULL);
		wait(&status);
		ptrace(PTRACE_SETOPTIONS, child, NULL, PTRACE_O_TRACEEXIT);
		wait_for_me = 1;
		putdata(child, (unsigned long long) &wait_for_me, (char *) &wait_for_me, sizeof(wait_for_me));
		do {
			PTRACE(PTRACE_GETREGS, child, &regs_struct, &regs_struct);
			if (((void *)regs_struct.rip) != entry_point) {
				break;
			}
			PTRACE(PTRACE_SINGLESTEP, child, NULL, NULL);
			if (status >> 8 == (SIGTRAP | (PTRACE_EVENT_EXIT<<8))) {
				printf("Early exit.");
				return;
			}
			wait(&status);
		} while (true);

		while (true) {
			if (WIFEXITED(status)) {// >> 8 == (SIGTRAP | (PTRACE_EVENT_EXIT<<8))) {
				printf("===>> Normal exit: %x.\n", status);
				return;
			} else if (WIFSIGNALED(status)) {// >> 8 == (SIGTRAP | (PTRACE_EVENT_EXIT<<8))) {
				PTRACE(PTRACE_DETACH, child, NULL, NULL);
				printf("===>> SIGNAL received: %d.\n", WTERMSIG(status));
				return;
			} else if (WIFSTOPPED(status)
				   && WSTOPSIG(status) != SIGSTOP
				   && WSTOPSIG(status) != SIGTRAP) {// >> 8 == (SIGTRAP | (PTRACE_EVENT_EXIT<<8))) {
				PTRACE(PTRACE_DETACH, child, NULL, NULL);
				printf("===>> SIGNAL received: %d, %s.\n", WSTOPSIG(status), strsignal(WSTOPSIG(status)));
				return;
			}
			PTRACE(PTRACE_GETREGS, child, &regs_struct, &regs_struct);
			decode_regs(regs, &pc, &regs_struct);
			
			if ((unsigned long long) pc >= debug_region_start
			    && (unsigned long long) pc <= debug_region_end) {
				printf("%p : %llu\n", pc, regs[REGISTER_A0]);
			}
			PTRACE(PTRACE_SINGLESTEP, child, NULL, NULL);
			wait(&status);
		}
	} else {
		// child
		while (!wait_for_me);
		entry_point();
	}
}

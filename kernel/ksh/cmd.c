/*********************************************************************************/
/* Module Name:  cmd.c */
/* Project:      AurixOS */
/*                                                                               */
/* Copyright (c) 2024-2026 Jozef Nagy */
/*                                                                               */
/* This source is subject to the MIT License. */
/* See License.txt in the root of this repository. */
/* All other rights reserved. */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE */
/* SOFTWARE. */
/*********************************************************************************/

#include <ksh/cmd.h>

#include <arch/cpu/cpu.h>
#include <arch/sys/irqlock.h>

#include <aurix.h>
#include <lib/string.h>
#include <mm/pmm.h>
#include <sys/sched.h>
#include <time/time.h>
#include <util/kprintf.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern const char *aurix_banner;

typedef int (*ksh_cmd_fn)(int argc, char **argv);

typedef struct {
	const char *name;
	const char *usage;
	const char *desc;
	ksh_cmd_fn fn;
} ksh_command;

static int cmd_help(int argc, char **argv);
static int cmd_about(int argc, char **argv);
static int cmd_cpuinfo(int argc, char **argv);
static int cmd_threads(int argc, char **argv);
static int cmd_ps(int argc, char **argv);
static int cmd_uptime(int argc, char **argv);
static int cmd_whoami(int argc, char **argv);
static int cmd_hexdump(int argc, char **argv);
static int cmd_sched(int argc, char **argv);
static int cmd_free(int argc, char **argv);
static int cmd_clear(int argc, char **argv);
static int cmd_fetch(int argc, char **argv);

static const ksh_command ksh_commands[] = {
	{ "help", "help [cmd]", "list commands / show help for cmd", cmd_help },
	{ "about", "about", "print system banner", cmd_about },
	{ "cpuinfo", "cpuinfo", "show CPU vendor/name and scheduler stats",
	  cmd_cpuinfo },
	{ "threads", "threads [cpu]", "list threads (optionally for one CPU)",
	  cmd_threads },
	{ "ps", "ps", "list processes (derived from runnable threads)", cmd_ps },
	{ "uptime", "uptime", "show time since boot in ms", cmd_uptime },
	{ "whoami", "whoami", "show current thread/process/cpu", cmd_whoami },
	{ "free", "free", "show physical memory usage", cmd_free },
	{ "hexdump", "hexdump <addr> <len>", "dump memory (unsafe if unmapped)",
	  cmd_hexdump },
	{ "sched", "sched", "show scheduler enabled state", cmd_sched },
	{ "clear", "clear", "clears screen", cmd_clear },
	{ "fetch", "fetch", "show system information", cmd_fetch },
};

static bool ksh_is_idle_thread_on_cpu(tcb *t, struct cpu *cpu)
{
	if (!t || !cpu || !t->process)
		return false;
	// Idle threads are encoded as UINT32_MAX - cpu_id and belong to pid 0.
	return t->process->pid == 0 && t->tid == (uint32_t)(UINT32_MAX - cpu->id);
}

static bool ksh_is_space(char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static const ksh_command *ksh_find_cmd(const char *name)
{
	if (!name || !*name)
		return NULL;
	for (size_t i = 0; i < sizeof(ksh_commands) / sizeof(ksh_commands[0]);
		 i++) {
		if (streq(ksh_commands[i].name, name))
			return &ksh_commands[i];
	}
	return NULL;
}

static bool ksh_parse_u64(const char *s, uint64_t *out)
{
	if (!s || !*s || !out)
		return false;

	uint64_t base = 10;
	if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
		base = 16;
		s += 2;
		if (!*s)
			return false;
	}

	uint64_t val = 0;
	while (*s) {
		char c = *s++;
		uint64_t d;
		if (c >= '0' && c <= '9')
			d = (uint64_t)(c - '0');
		else if (base == 16 && c >= 'a' && c <= 'f')
			d = 10 + (uint64_t)(c - 'a');
		else if (base == 16 && c >= 'A' && c <= 'F')
			d = 10 + (uint64_t)(c - 'A');
		else
			return false;

		if (d >= base)
			return false;
		val = (val * base) + d;
	}

	*out = val;
	return true;
}

static int ksh_tokenize(char *line, char **argv, int argv_cap)
{
	if (!line || !argv || argv_cap <= 0)
		return 0;

	int argc = 0;
	char *save = NULL;
	for (char *tok = strtok_r(line, " \t", &save); tok;
		 tok = strtok_r(NULL, " \t", &save)) {
		if (argc >= argv_cap)
			break;
		argv[argc++] = tok;
	}
	return argc;
}

void ksh_exec_line(char *line)
{
	if (!line)
		return;

	while (*line && ksh_is_space(*line))
		line++;
	if (!*line)
		return;

	char *argv[16];
	int argc = ksh_tokenize(line, argv, (int)(sizeof(argv) / sizeof(argv[0])));
	if (argc <= 0)
		return;

	const ksh_command *cmd = ksh_find_cmd(argv[0]);
	if (!cmd) {
		kprintf("ksh: unknown command: %s (try 'help')\n", argv[0]);
		return;
	}

	(void)cmd->fn(argc, argv);
}

static int cmd_help(int argc, char **argv)
{
	if (argc >= 2) {
		const ksh_command *cmd = ksh_find_cmd(argv[1]);
		if (!cmd) {
			kprintf("ksh: no such command: %s\n", argv[1]);
			return 1;
		}
		kprintf("%s - %s\n", cmd->name, cmd->desc);
		kprintf("usage: %s\n", cmd->usage);
		return 0;
	}

	for (size_t i = 0; i < sizeof(ksh_commands) / sizeof(ksh_commands[0]);
		 i++) {
		kprintf("%-8s  %-24s  %s\n", ksh_commands[i].name,
				ksh_commands[i].usage, ksh_commands[i].desc);
	}
	return 0;
}

static int cmd_about(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	if (aurix_banner)
		kprintf("%s\n", aurix_banner);
	kprintf("Licensed under the MIT License (c) 2024-2026\n");
	return 0;
}

static int cmd_cpuinfo(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	kprintf("cpus: %zu\n", cpu_count);
	for (size_t i = 0; i < cpu_count; i++) {
		struct cpu *c = &cpuinfo[i];
		irqlock_acquire(&c->sched_lock);
		uint64_t tc = c->thread_count;
		struct tcb *head = c->thread_list;
		bool head_is_idle = ksh_is_idle_thread_on_cpu(head, c);
		irqlock_release(&c->sched_lock);

		if (head_is_idle) {
			kprintf("CPU%u: vendor='%s' name='%s' threads=%llu current=idle\n",
					c->id, c->vendor_str, c->name_ext, (unsigned long long)tc);
		} else {
			kprintf(
				"CPU%u: vendor='%s' name='%s' threads=%llu current_tid=%u\n",
				c->id, c->vendor_str, c->name_ext, (unsigned long long)tc,
				head ? head->tid : 0);
		}
		kprintf("  features: sse=%u sse2=%u apic=%u tsc=%u\n",
				c->cpuid.edx_bits.sse, c->cpuid.edx_bits.sse2,
				c->cpuid.edx_bits.apic, c->cpuid.edx_bits.tsc);
	}
	return 0;
}

static bool ksh_parse_cpu_id(const char *s, size_t *out)
{
	uint64_t v;
	if (!ksh_parse_u64(s, &v))
		return false;
	if (v >= cpu_count)
		return false;
	*out = (size_t)v;
	return true;
}

static int cmd_threads(int argc, char **argv)
{
	bool single = false;
	size_t cpu_id = 0;
	if (argc >= 2) {
		single = ksh_parse_cpu_id(argv[1], &cpu_id);
		if (!single) {
			kprintf("usage: threads [cpu]\n");
			return 1;
		}
	}

	for (size_t i = 0; i < cpu_count; i++) {
		if (single && i != cpu_id)
			continue;

		struct cpu *c = &cpuinfo[i];
		irqlock_acquire(&c->sched_lock);
		kprintf("CPU%u:\n", c->id);
		kprintf("  runnable_count=%llu\n", (unsigned long long)c->thread_count);

		tcb *t = c->thread_list;
		unsigned guard = 0;
		while (t && guard++ < 512) {
			pcb *p = t->process;
			uint32_t pid = p ? p->pid : 0;
			const char *pname = (p && p->name) ? p->name : "(unnamed)";
			if (ksh_is_idle_thread_on_cpu(t, c)) {
				kprintf("  tid=idle pid=%u cpu=%u slice=%u proc=%p name=%s\n",
						pid, c->id, t->time_slice, p, pname);
			} else {
				kprintf("  tid=%u pid=%u cpu=%u slice=%u proc=%p name=%s\n",
						t->tid, pid, c->id, t->time_slice, p, pname);
			}
			t = t->cpu_next;
		}
		if (guard >= 512)
			kprintf("  ... truncated (possible list corruption)\n");
		irqlock_release(&c->sched_lock);
	}

	return 0;
}

typedef struct {
	pcb *p;
	uint64_t threads_seen;
} ksh_ps_ent;

static int cmd_ps(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	ksh_ps_ent ents[64];
	size_t ent_count = 0;

	for (size_t i = 0; i < cpu_count; i++) {
		struct cpu *c = &cpuinfo[i];
		irqlock_acquire(&c->sched_lock);
		for (tcb *t = c->thread_list; t; t = t->cpu_next) {
			if (ksh_is_idle_thread_on_cpu(t, c))
				continue;
			pcb *p = t->process;
			if (!p)
				continue;
			size_t j;
			for (j = 0; j < ent_count; j++) {
				if (ents[j].p == p) {
					ents[j].threads_seen++;
					break;
				}
			}
			if (j == ent_count &&
				ent_count < (sizeof(ents) / sizeof(ents[0]))) {
				ents[ent_count].p = p;
				ents[ent_count].threads_seen = 1;
				ent_count++;
			}
		}
		irqlock_release(&c->sched_lock);
	}

	kprintf("pid  threads  name\n");
	for (size_t i = 0; i < ent_count; i++) {
		pcb *p = ents[i].p;
		const char *pname = (p && p->name) ? p->name : "(unnamed)";
		kprintf("%u    %llu        %s\n", p->pid,
				(unsigned long long)ents[i].threads_seen, pname);
	}
	if (ent_count == (sizeof(ents) / sizeof(ents[0])))
		kprintf("(warning: process list truncated)\n");
	return 0;
}

static int cmd_uptime(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	kprintf("uptime: %llu ms\n", (unsigned long long)get_ms());
	return 0;
}

static int cmd_whoami(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	tcb *t = thread_current();
	pcb *p = t ? t->process : NULL;
	struct cpu *c = cpu_get_current();
	const char *pname = (p && p->name) ? p->name : "(unnamed)";
	kprintf("tid=%u pid=%u cpu=%u proc=%p name=%s\n", t ? t->tid : 0,
			p ? p->pid : 0, c ? c->id : 0, p, pname);
	return 0;
}

static int cmd_hexdump(int argc, char **argv)
{
	if (argc < 3) {
		kprintf("usage: hexdump <addr> <len>\n");
		return 1;
	}

	uint64_t addr = 0;
	uint64_t len = 0;
	if (!ksh_parse_u64(argv[1], &addr) || !ksh_parse_u64(argv[2], &len)) {
		kprintf("ksh: invalid number\n");
		return 1;
	}
	if (len == 0) {
		kprintf("ksh: len must be > 0\n");
		return 1;
	}
	if (len > 4096) {
		kprintf("ksh: capping len to 4096\n");
		len = 4096;
	}

	kprintf("hexdump: addr=%p len=%llu\n", (void *)(uintptr_t)addr,
			(unsigned long long)len);
	hexdump((const void *)(uintptr_t)addr, (size_t)len);
	return 0;
}

static int cmd_sched(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	kprintf("sched: %s\n", sched_is_enabled() ? "enabled" : "disabled");
	return 0;
}

static int cmd_free(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	uint64_t total_pages = bitmap_pages;
	uint64_t usable_pages = pmm_usable_pages();
	uint64_t free_pages = pmm_free_pages();
	uint64_t used_pages_usable =
		(usable_pages >= free_pages) ? (usable_pages - free_pages) : 0;
	uint64_t used_pages_total = pmm_used_pages();

	uint64_t total_mem = total_pages * (uint64_t)PAGE_SIZE;
	uint64_t usable_mem = usable_pages * (uint64_t)PAGE_SIZE;
	uint64_t free_mem = free_pages * (uint64_t)PAGE_SIZE;
	uint64_t used_mem = used_pages_usable * (uint64_t)PAGE_SIZE;

	uint64_t mib = 1024ull * 1024ull;

	kprintf("mem: total=%lluMiB usable=%lluMiB used=%lluMiB free=%lluMiB "
			"pages(total=%llu usable=%llu used=%llu free=%llu)\n",
			(unsigned long long)(total_mem / mib),
			(unsigned long long)(usable_mem / mib),
			(unsigned long long)(used_mem / mib),
			(unsigned long long)(free_mem / mib),
			(unsigned long long)total_pages, (unsigned long long)usable_pages,
			(unsigned long long)used_pages_total,
			(unsigned long long)free_pages);

	return 0;
}

static int cmd_clear(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	kprintf("\033[2J\033[H");
	return 0;
}

static int cmd_fetch(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	struct cpu *c = &cpuinfo[0];

	uint64_t uptime_ms = get_ms();

	uint64_t total_pages = bitmap_pages;
	uint64_t usable_pages = pmm_usable_pages();
	uint64_t free_pages = pmm_free_pages();
	uint64_t used_pages = usable_pages - free_pages;

	uint64_t total_mem = total_pages * (uint64_t)PAGE_SIZE;
	uint64_t used_mem = used_pages * (uint64_t)PAGE_SIZE;

	uint64_t mib = 1024ull * 1024ull;

	kprintf("        /\\        aurix-" AURIX_VERSION "-" AURIX_GITREV
			" (" AURIX_CODENAME ")\n"
			"       /  \\       -------------------\n"
			"      / /\\ \\      cpu: %s (%zu)\n"
			"     / ____ \\     uptime: %llu ms\n"
			"    /_/    \\_\\    memory: %llu / %llu MiB\n",
			c->name_ext, cpu_count, (unsigned long long)uptime_ms,
			(unsigned long long)(used_mem / mib),
			(unsigned long long)(total_mem / mib));

	return 0;
}

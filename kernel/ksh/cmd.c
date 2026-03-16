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
#include <boot/axprot.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <loader/module.h>
#include <vfs/fileio.h>
#include <vfs/vfs.h>
#include <loader/elf.h>
#include <mm/heap.h>

extern const char *aurix_banner;

extern struct aurix_parameters *boot_params;

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
static int cmd_modls(int argc, char **argv);
static int cmd_exec(int argc, char **argv);
static int cmd_readf(int argc, char **argv);
static int cmd_dir(int argc, char **argv);
static int cmd_draw(int argc, char **argv);

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
	{ "modls", "modls", "shows loaded modules", cmd_modls },
	{ "exec", "exec <path>", "executes executable from path", cmd_exec },
	{ "readf", "readf <path>", "reads file from path", cmd_readf },
	{ "dir", "dir [path]", "list directory contents", cmd_dir },
	{ "draw", "draw <path>", "draw TGA image to framebuffer", cmd_draw }
};

static bool ksh_is_idle_thread_on_cpu(tcb *t, struct cpu *cpu)
{
	if (!t || !cpu || !t->process)
		return false;
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

static int cmd_modls(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	struct module_info_node *m = module_get_list();

	while (m) {
		kprintf("Module: %s\n", m->name ? m->name : "(unknown)");

		if (m->desc)
			kprintf("  Desc: %s\n", m->desc);

		if (m->author)
			kprintf("  Author: %s\n", m->author);

		kprintf("  Load base: 0x%lx\n", m->load_base);

		m = m->next;
	}
	return 0;
}

static int cmd_exec(int argc, char **argv)
{
	if (argc < 2) {
		kprintf("usage: exec <path>\n");
		return 1;
	}

	const char *path = argv[1];
	struct fileio *f = open(path, 0, 0);
	if (!f) {
		kprintf("ksh: failed to open file: %s\n", path);
		return 1;
	}

	char *buf = (char *)kmalloc(f->size);
	if (!buf) {
		kprintf("ksh: failed to allocate buffer for file: %s\n", path);
		return 1;
	}

	if (read(f, f->size, buf) != f->size) {
		kprintf("ksh: failed to read file: %s\n", path);
		return 1;
	}

	struct pcb *proc = proc_create();
	if (!proc) {
		kprintf("ksh: failed to create process for: %s\n", path);
		kfree(buf);
		return 1;
	}

	char *name_copy = kmalloc(strlen(path) + 1);
	if (name_copy)
		strcpy(name_copy, path);
	proc->name = (const char *)name_copy;

	uint64_t addr, size = 0;
	uintptr_t entry = elf_load(buf, &addr, &size, proc->pm);
	if (entry == 0) {
		kprintf("ksh: failed to load ELF: %s\n", path);
		proc_destroy(proc);
		kfree(buf);
		return 1;
	}

	kprintf("ksh: loaded ELF '%s' entry=0x%lx addr=0x%lx size=0x%lx\n", path,
			entry, addr, size);

	struct tcb *thread = thread_create(proc, (void (*)(void))entry);
	if (!thread) {
		kprintf("ksh: failed to create thread for: %s\n", path);
		proc_destroy(proc);
		kfree(buf);
		return 1;
	}
	return 0;
}

static int cmd_readf(int argc, char **argv)
{
	if (argc < 2) {
		kprintf("usage: exec <path>\n");
		return 1;
	}

	const char *path = argv[1];
	struct fileio *f = open(path, 0, 0);
	if (!f) {
		kprintf("ksh: failed to open file: %s\n", path);
		return 1;
	}

	char *buf = (char *)kmalloc(f->size);
	if (!buf) {
		kprintf("ksh: failed to allocate buffer for file: %s\n", path);
		return 1;
	}

	if (read(f, f->size, buf) != f->size) {
		kprintf("ksh: failed to read file: %s\n", path);
		kfree(buf);
		return 1;
	}

	kprintf("%s\n", buf);
	kfree(buf);
	close(f);
	return 0;
}

static int cmd_dir(int argc, char **argv)
{
	const char *input_path = (argc > 1) ? argv[1] : "/";
	char full_path[256];
	if (input_path[0] == '/') {
		strncpy(full_path, input_path, sizeof(full_path) - 1);
		full_path[sizeof(full_path) - 1] = '\0';
	} else {
		strcpy(full_path, "/");
		strncpy(full_path + 1, input_path, sizeof(full_path) - 2);
		full_path[sizeof(full_path) - 1] = '\0';
	}

	struct vnode *vnode;
	if (vfs_lookup(full_path, &vnode) != 0) {
		kprintf("dir: %s: not found\n", full_path);
		return 1;
	}

	if (vnode->vtype != VNODE_DIR) {
		kprintf("dir: %s: not a directory\n", full_path);
		vnode_unref(vnode);
		return 1;
	}

	struct dirent entries[64];
	size_t count = 64;
	if (vfs_readdir(vnode, entries, &count) != 0) {
		kprintf("dir: failed to read directory\n");
		vnode_unref(vnode);
		return 1;
	}

	kprintf("%-20s %8s %s\n", "Name", "Size", "Type");
	kprintf("%-20s %8s %s\n", "--------------------", "--------", "----");

	for (size_t i = 0; i < count; i++) {
		char entry_path[512];
		strcpy(entry_path, full_path);
		if (strcmp(full_path, "/") != 0) {
			strcat(entry_path, "/");
		}
		strcat(entry_path, entries[i].d_name);

		struct stat st;
		if (vfs_stat(entry_path, &st) == 0) {
			char type = '?';
			switch (entries[i].d_type) {
			case DT_DIR:
				type = 'd';
				break;
			case DT_REG:
				type = '-';
				break;
			case DT_LNK:
				type = 'l';
				break;
			case DT_CHR:
				type = 'c';
				break;
			case DT_BLK:
				type = 'b';
				break;
			case DT_FIFO:
				type = 'p';
				break;
			case DT_SOCK:
				type = 's';
				break;
			default:
				type = '?';
				break;
			}

			char name[256];
			strncpy(name, entries[i].d_name, sizeof(name) - 1);
			name[sizeof(name) - 1] = '\0';
			if (entries[i].d_type == DT_DIR) {
				size_t len = strlen(name);
				if (len < sizeof(name) - 1) {
					name[len] = '/';
					name[len + 1] = '\0';
				}
			}

			kprintf("%-20s %8llu %c\n", name, (unsigned long long)st.st_size,
					type);
		} else {
			kprintf("%-20s %8s %s\n", entries[i].d_name, "?", "?");
		}
	}

	vnode_unref(vnode);
	return 0;
}

static int cmd_draw(int argc, char **argv)
{
	if (argc < 2) {
		kprintf("usage: draw <path>\n");
		return 1;
	}

	const char *path = argv[1];

	if (!boot_params || !boot_params->framebuffer.addr) {
		kprintf("ksh: framebuffer not available\n");
		return 1;
	}

	struct fileio *f = open(path, 0, 0);
	if (!f) {
		kprintf("ksh: failed to open file: %s\n", path);
		return 1;
	}

	uint8_t *buf = (uint8_t *)kmalloc(f->size);
	if (!buf) {
		kprintf("ksh: failed to allocate buffer\n");
		close(f);
		return 1;
	}

	if (read(f, f->size, buf) != f->size) {
		kprintf("ksh: failed to read file\n");
		kfree(buf);
		close(f);
		return 1;
	}

	close(f);

	if (f->size < 18) {
		kprintf("ksh: invalid TGA file\n");
		kfree(buf);
		return 1;
	}

	uint8_t image_type = buf[2];
	if (image_type != 2 && image_type != 3 && image_type != 10) {
		kprintf("ksh: unsupported TGA type\n");
		kfree(buf);
		return 1;
	}

	uint16_t width = buf[12] | (buf[13] << 8);
	uint16_t height = buf[14] | (buf[15] << 8);
	uint8_t bpp = buf[16];

	size_t data_offset = 18 + buf[0];
	if (buf[1])
		data_offset += 5;
	if (data_offset >= f->size) {
		kprintf("ksh: invalid TGA data\n");
		kfree(buf);
		return 1;
	}

	uint32_t *fb = (uint32_t *)boot_params->framebuffer.addr;
	uint32_t fb_width = boot_params->framebuffer.width;
	uint32_t fb_height = boot_params->framebuffer.height;
	uint32_t fb_pitch = boot_params->framebuffer.pitch / 4;
	int fb_format = boot_params->framebuffer.format;

	uint8_t *pixel_data = buf + data_offset;
	size_t pixel_size = bpp / 8;
	size_t total_pixels = (size_t)width * height;

	uint32_t *decoded = (uint32_t *)kmalloc(total_pixels * 4);
	if (!decoded) {
		kprintf("ksh: failed to allocate decode buffer\n");
		kfree(buf);
		return 1;
	}

	size_t decoded_count = 0;

	if (image_type == 2) {
		for (size_t i = 0; i < total_pixels; i++) {
			size_t idx = i * pixel_size;
			if (idx + pixel_size > f->size - data_offset)
				break;
			uint8_t b = pixel_data[idx], g = pixel_data[idx + 1],
					r = pixel_data[idx + 2],
					a = pixel_size >= 4 ? pixel_data[idx + 3] : 255;
			uint32_t color = fb_format == AURIX_FB_BGRA ?
								 (a << 24) | (r << 16) | (g << 8) | b :
								 (r << 24) | (g << 16) | (b << 8) | a;
			decoded[decoded_count++] = color;
		}
	} else if (image_type == 3) {
		for (size_t i = 0; i < total_pixels; i++) {
			size_t idx = i * pixel_size;
			if (idx + pixel_size > f->size - data_offset)
				break;
			uint8_t gray = pixel_data[idx],
					a = pixel_size >= 2 ? pixel_data[idx + 1] : 255;
			uint32_t color = fb_format == AURIX_FB_BGRA ?
								 (a << 24) | (gray << 16) | (gray << 8) | gray :
								 (gray << 24) | (gray << 16) | (gray << 8) | a;
			decoded[decoded_count++] = color;
		}
	} else if (image_type == 10) {
		size_t data_idx = 0;
		while (decoded_count < total_pixels &&
			   data_idx < f->size - data_offset) {
			uint8_t header = pixel_data[data_idx++];
			uint8_t type = header & 0x80, count = (header & 0x7F) + 1;
			if (type == 0) {
				for (uint8_t j = 0; j < count && decoded_count < total_pixels;
					 j++) {
					if (data_idx + pixel_size > f->size - data_offset)
						break;
					uint8_t b = pixel_data[data_idx],
							g = pixel_data[data_idx + 1],
							r = pixel_data[data_idx + 2],
							a = pixel_size >= 4 ? pixel_data[data_idx + 3] :
												  255;
					uint32_t color = fb_format == AURIX_FB_BGRA ?
										 (a << 24) | (r << 16) | (g << 8) | b :
										 (r << 24) | (g << 16) | (b << 8) | a;
					decoded[decoded_count++] = color;
					data_idx += pixel_size;
				}
			} else {
				if (data_idx + pixel_size > f->size - data_offset)
					break;
				uint8_t b = pixel_data[data_idx], g = pixel_data[data_idx + 1],
						r = pixel_data[data_idx + 2],
						a = pixel_size >= 4 ? pixel_data[data_idx + 3] : 255;
				uint32_t color = fb_format == AURIX_FB_BGRA ?
									 (a << 24) | (r << 16) | (g << 8) | b :
									 (r << 24) | (g << 16) | (b << 8) | a;
				for (uint8_t j = 0; j < count && decoded_count < total_pixels;
					 j++)
					decoded[decoded_count++] = color;
				data_idx += pixel_size;
			}
		}
	}

	uint32_t draw_width = width < fb_width ? width : fb_width;
	uint32_t draw_height = height < fb_height ? height : fb_height;

	for (uint32_t y = 0; y < draw_height; y++) {
		for (uint32_t x = 0; x < draw_width; x++) {
			size_t idx = ((uint32_t)height - 1 - y) * width + x;
			if (idx < decoded_count) {
				fb[y * fb_pitch + x] = decoded[idx];
			}
		}
	}

	int rows = (height + 15) / 16;
	for (int i = 0; i < rows; i++)
		kprintf("\n");

	kfree(decoded);
	kfree(buf);
	return 0;
}
/*********************************************************************************/
/* Module Name:  kinit.c */
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

#include <boot/axprot.h>
#include <aurix.h>
#include <arch/cpu/cpu.h>
#include <arch/apic/apic.h>
#include <arch/cpu/irq.h>
#include <acpi/acpi.h>
#include <boot/args.h>
#include <cpu/cpu.h>
#include <debug/uart.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/heap.h>
#include <loader/module.h>
#include <smbios/smbios.h>
#include <time/time.h>
#include <lib/string.h>
#include <platform/time/pit.h>
#include <platform/time/time.h>
#include <dev/driver.h>
#include <vfs/vfs.h>
#include <flanterm/flanterm.h>
#include <flanterm/backends/fb.h>
#include <test/test.h>
#include <aurix.h>
#include <sys/sched.h>
#include <sys/panic.h>
#include <fs/devfs.h>
#include <ksh/ksh.h>
#include <fs/ramfs.h>
#include <fs/cpio/newc.h>
#include <loader/elf.h>
#include <user/syscall.h>
#include <dev/builtin/stdio.h>
#include <util/kprintf.h>
#include <lib/align.h>

struct aurix_parameters *boot_params = NULL;
struct flanterm_context *ft_ctx = NULL;

// ======= Flanterm theme, just for looks (kernel log, pre-TTY) =======
static uint32_t ft_ansi_colours[8] = {
	0x00101114, /* black */
	0x008b2e2e, /* red */
	0x003a7d44, /* green */
	0x008b6f1f, /* yellow */
	0x002f4f7f, /* blue */
	0x006a3f7a, /* magenta */
	0x002f6f73, /* cyan */
	0x00c5c8c6, /* white */
};

static uint32_t ft_ansi_bright_colours[8] = {
	0x00202024, /* bright black (dark gray) */
	0x00b84a4a, /* bright red */
	0x004fae66, /* bright green */
	0x00b8952e, /* bright yellow */
	0x004c78c4, /* bright blue */
	0x008c5fbf, /* bright magenta */
	0x004fb3b8, /* bright cyan */
	0x00e6e6e6, /* bright white */
};
static uint32_t ft_default_bg = 0x000d0f12; /* near-black */
static uint32_t ft_default_fg = 0x00d0d0d0; /* neutral light gray */
static uint32_t ft_default_bg_bright = 0x0015151a; /* slightly lifted */
static uint32_t ft_default_fg_bright = 0x00ffffff; /* clean white */
// ====================================================================

uintptr_t hhdm_offset = 0;
vctx_t *kvctx = NULL;

const char *aurix_banner =
	"    _              _       ___  ____\n"
	"   / \\  _   _ _ __(_)_  __/ _ \\/ ___|\n"
	"  / _ \\| | | | '__| \\ \\/ / | | \\___ \\\n"
	" / ___ \\ |_| | |  | |>  <| |_| |___) |\n"
	"/_/   \\_\\__,_|_|  |_/_/ \\_\\___/|____/  (c) Copyright 2024-2026 Jozef Nagy";

pcb *load_init(const char *path)
{
	struct fileio *f = open(path, 0, 0);
	if (!f) {
		error("failed to open file: %s\n", path);
		return NULL;
	}

	char *buf = (char *)kmalloc(f->size);
	if (!buf) {
		error("failed to allocate buffer for file: %s\n", path);
		close(f);
		return NULL;
	}

	if (read(f, f->size, buf) != f->size) {
		error("failed to read file: %s\n", path);
		close(f);
		kfree(buf);
		return NULL;
	}

	close(f);

	struct pcb *proc = proc_create();
	if (!proc) {
		error("failed to create process for: %s\n", path);
		kfree(buf);
		return NULL;
	}

	proc->name = strdup("init");

	uintptr_t entry = 0;
	if (!elf_load_user_process(buf, path, proc, NULL, 0, NULL, 0, &entry)) {
		error("failed to load ELF: %s\n", path);
		proc_destroy(proc);
		kfree(buf);
		return NULL;
	}

	info("loaded ELF '%s' entry=0x%lx\n", path, entry);

	struct tcb *thread = thread_create_user(proc, (void (*)(void))entry);
	if (!thread) {
		error("failed to create thread for: %s\n", path);
		proc_destroy(proc);
		kfree(buf);
		return NULL;
	}
	kfree(buf);
	return proc;
}

static bool stage_module_file(const char *name, void *phys_addr, size_t size,
							  struct fileio *manifest)
{
	if (!name || !phys_addr || size == 0) {
		return false;
	}

	char path[64];
	int path_len = snprintf(path, sizeof(path), "/sys/%s", name);
	if (path_len <= 1 || (size_t)path_len >= sizeof(path)) {
		return false;
	}

	struct fileio *out = open(path, O_CREATE | O_WRONLY | O_TRUNC, 0644);
	if (!out) {
		return false;
	}

	int written = write(out, (void *)PHYS_TO_VIRT((uintptr_t)phys_addr), size);
	close(out);
	if (written < 0 || (size_t)written != size) {
		return false;
	}

	if (manifest) {
		int list_written = write(manifest, path, (size_t)path_len);
		if (list_written < 0 || list_written != path_len) {
			return false;
		}

		list_written = write(manifest, "\n", 1);
		if (list_written != 1) {
			return false;
		}
	}

	return true;
}

static void stage_boot_modules_to_ramfs(void)
{
	if (vfs_mkdir("/sys", 0755) != 0) {
		struct fileio *sysdir = open("/sys", O_RDONLY | O_DIRECTORY, 0);
		if (!sysdir)
			kpanic(NULL, "Failed to create /sys directory");
		close(sysdir);
	}

	struct fileio *manifest =
		open("/sys/modules.list", O_CREATE | O_WRONLY | O_TRUNC, 0644);
	if (!manifest) {
		kpanic(NULL, "Failed to create /sys/modules.list");
	}

	for (uint32_t m = 0; m < boot_params->module_count; m++) {
		struct aurix_module *mod = &boot_params->modules[m];
		const char *name = mod->filename;
		size_t len = strlen(name);

		if (len < 4 || strcmp(name + len - 4, ".sys") != 0) {
			continue;
		}

		if (!stage_module_file(name, mod->addr, mod->size, manifest)) {
			close(manifest);
			kpanicf(NULL, "Failed to stage module '%s'", name);
		}

		trace("Staged module '%s' to ramfs\n", name);
	}

	close(manifest);
}

void _start(struct aurix_parameters *params)
{
	boot_params = params;
	hhdm_offset = params->hhdm_offset;
	log_init();
	serial_init();

	if (params->revision != AURIX_PROTOCOL_REVISION) {
		kpanicf(NULL, "Aurix Protocol revision mismatch: expected %u, got %u",
				AURIX_PROTOCOL_REVISION, params->revision);
	}

	ft_ctx = flanterm_fb_init(
		NULL, NULL, (uint32_t *)boot_params->framebuffer.addr,
		boot_params->framebuffer.width, boot_params->framebuffer.height,
		boot_params->framebuffer.pitch, 8, 16, 8, 8, 8, 0, NULL,
		ft_ansi_colours, ft_ansi_bright_colours, &ft_default_bg, &ft_default_fg,
		&ft_default_bg_bright, &ft_default_fg_bright, NULL, 0, 0, 1, 0, 0, 0);

	if (!ft_ctx)
		error("Failed to init flanterm\n");

	kprintf("%s\n", aurix_banner);

	cpu_early_init();

	pmm_init();

	paging_init();

	smbios_init((void *)boot_params->smbios_addr);

	acpi_init((void *)boot_params->rsdp_addr);
	apic_init();

	cpu_init();

	debug("kernel cmdline: %s\n", boot_params->cmdline);
	parse_boot_args(boot_params->cmdline);

	kvctx = vinit(kernel_pm, 0xffffffff90000000ULL);
	heap_init(kvctx);

	// TODO: Add kernel cmdline parsing
	if (1) {
		test_run(10);
	}

#ifdef __x86_64__
	// TODO: Use HPET instead?
	pit_init(50);
#else
#warning No clock implemented, the scheduler will not fire!
#endif

	// setup fs
	struct aurix_module *initrd_mod = NULL;
	for (uint32_t m = 0; m < boot_params->module_count; m++) {
		struct aurix_module *mod = &boot_params->modules[m];
		if (strcmp(mod->filename, "initrd.cpio") == 0) {
			initrd_mod = mod;
			break;
		}
	}

	if (!initrd_mod) {
		kpanic(NULL, "No initrd found, checked \\System\\initrd.cpio");
	}

	struct cpio_fs *cpio = kmalloc(sizeof(struct cpio_fs));
	memset(cpio, 0, sizeof(struct cpio_fs));
	if (cpio_fs_parse(cpio, (void *)PHYS_TO_VIRT((uintptr_t)initrd_mod->addr),
					  initrd_mod->size) != 0) {
		kpanic(NULL, "Failed to parse initrd file.");
	}

	ramfs_init();
	struct ramfs *ramfs = ramfs_create_fs();

	if (ramfs_vfs_init(ramfs, "/") != 0) {
		kpanic(NULL, "Failed to initialize ramfs");
	}

	if (cpio_extract(cpio, "/") != 0) {
		kpanic(NULL, "Failed to parse initrd file on second pass.");
	}

	vfs_mkdir("/dev", 0755);

	devfs_init();
	struct devfs *devfs = devfs_create_fs();
	if (devfs_vfs_init(devfs, "/dev") != 0)
		kpanic(NULL, "Failed to initialize devfs");

	driver_core_init(devfs);
	stdio_init();
	cpu_init_mp();
	syscall_builtin_init();
	sched_init();

	platform_timekeeper_init();
	stage_boot_modules_to_ramfs();
	struct fileio *klog_file =
		open("/sys/klog", O_CREATE | O_WRONLY | O_TRUNC, 0644);
	if (!klog_file) {
		warn("Failed to open /sys/klog\n");
	} else {
		close(klog_file);
	}

	debug("Current time: %04d-%02d-%02d %02d:%02d:%02d\n", time_get_year(),
		  time_get_month(), time_get_day(), time_get_hour(), time_get_minute(),
		  time_get_second());

	pmm_reclaim_bootparms();
	{
		uint64_t ms = get_ms();
		success("Kernel boot complete in %u.%03u seconds\n",
				(uint32_t)(ms / 1000ull), (uint32_t)(ms % 1000ull));
		success("Running at %d cores on an %s\n", cpu_count,
				cpu_get_current()->name_ext);
	}

	pcb *init_proc = load_init("/bin/init");
	if (!init_proc) {
		error(
			"launching ksh (builtin debug shell), reason: failed to load init\n");
		pcb *p = proc_create();
		p->pm = kernel_pm;
		p->vctx = kvctx;
		struct tcb *ksh = thread_create(p, ksh_thread);
		ksh->process->name = strdup("ksh");
	}

	pit_set_freq(1000); // 1kHz should be fast enough
	sched_enable();

#if CONFIG_KSH

	uint64_t last_check_ms = get_ms();
	bool ksh_launched = (init_proc == NULL);
	uint32_t init_pid = init_proc ? init_proc->pid : 0;

	while (!ksh_launched) {
		uint64_t now = get_ms();
		if (!ksh_launched && now - last_check_ms >= 1000) {
			last_check_ms = now;
			if (!proc_has_threads(init_pid)) {
				kprintf(
					"\x1b[38;2;100;100;100minit process pid=%u exited, launching ksh\x1b[0m\n",
					init_pid);
				pcb *p = proc_create();
				p->pm = kernel_pm;
				p->vctx = kvctx;
				struct tcb *ksh = thread_create(p, ksh_thread);
				ksh->process->name = strdup("ksh");
				ksh_launched = true;
			}
		}
	}
#endif // CONFIG_KSH

	for (;;) {
#ifdef __x86_64__
		__asm__ volatile("hlt");
#elif __aarch64__
		__asm__ volatile("wfe");
#endif
	}

	UNREACHABLE();
}

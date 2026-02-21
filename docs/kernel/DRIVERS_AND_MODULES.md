# Drivers and Modules

This document describes AurixOS's in-kernel driver core (`device`/`driver`) and the loadable module system.

## Driver core (kernel)

Source:

- `kernel/include/dev/device.h`
- `kernel/include/dev/driver.h`
- `kernel/dev/driver.c`

### Concepts

- `struct device`
  - A kernel-known object describing a piece of hardware (or a logical device).
  - Key fields:
    - `name`: unique device name (string)
    - `class_name`: device class used for matching (string)
    - `driver_data`: driver-owned pointer for per-device state
    - `bound_driver`: set by the core once a driver successfully binds

- `struct driver`
  - A kernel-known object describing a driver that can bind to devices.
  - Key fields:
    - `name`: unique driver name (string)
    - `class_name`: class this driver can handle (string)
    - `probe(dev)`: called to attempt binding
    - `remove(dev)`: optional (not used by the core yet)

### Registration and binding

The core is intentionally small:

- `device_register(struct device *dev)`
  - Copies `dev->name` and `dev->class_name` into kernel memory and adds the device to the device list.
  - Device names must be unique.

- `driver_register(struct driver *drv)`
  - Copies `drv->name` and `drv->class_name` into kernel memory and adds the driver to the driver list.
  - Driver names must be unique.

- `driver_bind_all(void)`
  - For each registered driver, tries to bind it to every registered device with matching `class_name`.
  - A device is considered bound when `probe()` returns `0`.
  - Bound devices are not probed again.

Matching is currently based on `strcmp(drv->class_name, dev->class_name) == 0`.

## Module system (loadable modules)

Source:

- `kernel/loader/module.c`
- `kernel/include/aurix/axapi.h`
- `kernel/include/aurix/axapi_defs.inc`
- `kernel/sys/axapi.c`

### What a module is

- A module is an ELF image packaged as a `.sys` file (see `modules/*`).
- On boot, modules listed in the boot parameters are loaded by `module_load()`.
- Each module runs as a thread in its own process (separate CR3/pagemap).

### `modinfo` and entrypoints

Modules can define a `modinfo` symbol in the `.aurix.mod` section:

```c
__attribute__((section(".aurix.mod"))) const struct axmod_info modinfo = {
  .name = "My Module",
  .desc = "...",
  .author = "...",
  .mod_init = mod_init,
  .mod_exit = mod_exit,
};
```

If `modinfo` is present, the kernel uses `mod_init` as the module entrypoint.

### Polling for driver readiness

Modules start as soon as they're loaded. If a module wants to wait for a driver to show up or finish binding devices, it should poll via AXAPI:

```c
while (!ax_driver_exists("serial16550"))
  sched_yield();
while (!ax_driver_is_ready("serial16550"))
  sched_yield();
```

### AXAPI imports/exports

Modules do not link against the full kernel; instead they use AXAPI:

- Kernel exports a fixed symbol list (see `kernel/include/aurix/axapi_defs.inc`).
- Modules include `aurix/axapi.h` and the loader patches imported function pointers at load time.

Example (module-side):

```c
#include <aurix/axapi.h>

int mod_init(void) {
  kprintf("hello\n");
  return 0;
}
```

## Writing a driver as a module

The recommended pattern is:

1. Register a `struct device` describing the hardware.
2. Register a `struct driver` that can bind to the device's `class_name`.
3. Call `driver_bind_all()` (via AXAPI) to trigger binding.

AXAPI wrappers exist specifically for modules:

- `ax_device_register(struct device *dev)`
- `ax_driver_register(struct driver *drv)`
- `ax_driver_bind_all(void)`
- `ax_driver_exists(const char *driver_name)`
- `ax_driver_is_ready(const char *driver_name)`

Minimal example:

```c
#include <aurix/axapi.h>
#include <dev/device.h>
#include <dev/driver.h>

static int my_probe(struct device *dev) {
  (void)dev;
  /* init hardware, register devfs nodes, etc */
  return 0;
}

static struct driver my_driver = {
  .name = "mydrv",
  .class_name = "serial",
  .probe = my_probe,
  .remove = 0,
};

static struct device my_device = {
  .name = "com1",
  .class_name = "serial",
  .driver_data = 0,
  .bound_driver = 0,
};

int mod_init(void) {
  ax_device_register(&my_device);
  ax_driver_register(&my_driver);
  ax_driver_bind_all();
  return 0;
}
```

Reference implementation:

- `modules/serial16550/serial16550.c`

## Important notes (CR3 / memory ownership)

Because modules run in their own address space, the kernel driver core must not keep pointers to module memory.

Current behavior:

- On registration, the core copies `device`/`driver` metadata into kernel memory.
- When binding, the core switches to the registering driver's CR3 while calling `probe()`, so the module's code and data are accessible.

Guidelines:

- Prefer storing per-device state in kernel memory via `kmalloc()` (available to modules via AXAPI) and put that pointer in `dev->driver_data`.
- Avoid storing pointers to module-allocated data (e.g. static buffers or module heap) inside kernel-known structures unless they are only ever dereferenced while running in the module's CR3.

## Devfs integration

Many drivers expose character devices via devfs:

- Register a node: `devfs_register("/dev/ttyS0", &ops, ctx)`
- Write to a node: `devfs_write("/dev/ttyS0", buf, len)`

Example usage:

- Driver: `modules/serial16550/serial16550.c`
- Consumer: `modules/test/test.c`

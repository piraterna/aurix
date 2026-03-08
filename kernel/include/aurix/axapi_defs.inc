AXAPI_SYM(int, kprintf, (const char *fmt, ...))

AXAPI_SYM(void, sched_yield, (void))

AXAPI_SYM(uint8_t, cpu_get_current_id, (void))

AXAPI_SYM(void *, kmalloc, (size_t size))
AXAPI_SYM(void, kfree, (void *ptr))

AXAPI_SYM(uint8_t, ax_inb, (uint16_t port))
AXAPI_SYM(void, ax_outb, (uint16_t port, uint8_t val))
AXAPI_SYM(void, ax_io_wait, (void))

AXAPI_SYM(int, device_register, (struct device *dev))
AXAPI_SYM(int, driver_register, (struct driver *drv))
AXAPI_SYM(int, driver_bind_all, (void))
AXAPI_SYM(int, driver_poll, (const char *driver_name))

AXAPI_SYM(uint64_t, get_ms, (void))
AXAPI_SYM(void, sleep_ms, (uint64_t ms))

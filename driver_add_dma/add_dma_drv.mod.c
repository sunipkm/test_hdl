#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x26a33e1b, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0xc995c66c, __VMLINUX_SYMBOL_STR(param_ops_int) },
	{ 0xff9c5062, __VMLINUX_SYMBOL_STR(platform_driver_unregister) },
	{ 0x21b1535d, __VMLINUX_SYMBOL_STR(__platform_driver_register) },
	{ 0xccf1e993, __VMLINUX_SYMBOL_STR(dma_release_channel) },
	{ 0xa9e9a2c1, __VMLINUX_SYMBOL_STR(device_destroy) },
	{ 0xb2d48a2e, __VMLINUX_SYMBOL_STR(queue_work_on) },
	{ 0x2d3385d3, __VMLINUX_SYMBOL_STR(system_wq) },
	{ 0x3551dbe3, __VMLINUX_SYMBOL_STR(class_destroy) },
	{ 0xf4c265c8, __VMLINUX_SYMBOL_STR(cdev_del) },
	{ 0xcffa10e4, __VMLINUX_SYMBOL_STR(__class_create) },
	{ 0x7485e15e, __VMLINUX_SYMBOL_STR(unregister_chrdev_region) },
	{ 0xf5429edb, __VMLINUX_SYMBOL_STR(dev_err) },
	{ 0x72655d5, __VMLINUX_SYMBOL_STR(dmam_alloc_coherent) },
	{ 0x11d28b06, __VMLINUX_SYMBOL_STR(device_create) },
	{ 0xe914e41e, __VMLINUX_SYMBOL_STR(strcpy) },
	{ 0x92a94bdb, __VMLINUX_SYMBOL_STR(cdev_add) },
	{ 0x2ff3bb75, __VMLINUX_SYMBOL_STR(cdev_init) },
	{ 0x29537c9e, __VMLINUX_SYMBOL_STR(alloc_chrdev_region) },
	{ 0x85fb4afb, __VMLINUX_SYMBOL_STR(dma_request_slave_channel) },
	{ 0xcd897bff, __VMLINUX_SYMBOL_STR(dma_common_mmap) },
	{ 0x87d641, __VMLINUX_SYMBOL_STR(arm_dma_ops) },
	{ 0x29dec96f, __VMLINUX_SYMBOL_STR(wait_for_completion_timeout) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x93de854a, __VMLINUX_SYMBOL_STR(__init_waitqueue_head) },
	{ 0xf88c3301, __VMLINUX_SYMBOL_STR(sg_init_table) },
	{ 0x689e06e, __VMLINUX_SYMBOL_STR(complete) },
	{ 0xefd6cf06, __VMLINUX_SYMBOL_STR(__aeabi_unwind_cpp_pr0) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


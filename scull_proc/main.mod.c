#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
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
	{ 0x91d94495, "module_layout" },
	{ 0x6980fe91, "param_get_int" },
	{ 0xff964b25, "param_set_int" },
	{ 0x2da418b5, "copy_to_user" },
	{ 0xf2a644fb, "copy_from_user" },
	{ 0x8fde5204, "slab_buffer_size" },
	{ 0xfc822e6e, "kmem_cache_alloc_notrace" },
	{ 0xc288f8ce, "malloc_sizes" },
	{ 0x3eccf67, "create_proc_entry" },
	{ 0xd228f305, "cdev_add" },
	{ 0x9b55d1e2, "cdev_init" },
	{ 0x12da5bb2, "__kmalloc" },
	{ 0x29537c9e, "alloc_chrdev_region" },
	{ 0xb72397d5, "printk" },
	{ 0xd8e484f0, "register_chrdev_region" },
	{ 0x105e2727, "__tracepoint_kmalloc" },
	{ 0x3c2c5af5, "sprintf" },
	{ 0x3f1899f1, "up" },
	{ 0xfc4f55f3, "down_interruptible" },
	{ 0x7485e15e, "unregister_chrdev_region" },
	{ 0xb3ee9454, "remove_proc_entry" },
	{ 0x3e6dcf1e, "cdev_del" },
	{ 0x37a0cba, "kfree" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "1E656D4037DEA0C3B24BD6C");

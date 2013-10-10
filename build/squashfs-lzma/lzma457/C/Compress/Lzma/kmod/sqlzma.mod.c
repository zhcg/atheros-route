#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

#undef unix
struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = __stringify(KBUILD_MODNAME),
};

static const char __module_depends[]
__attribute_used__
__attribute__((section(".modinfo"))) =
"depends=unlzma";


MODULE_INFO(srcversion, "AA8EEA19B66153D6BF9DC81");

#include "register_types.h"
#include "shangcloud_mmo.h"

using namespace godot;

void initialize_shangcloud_mmo_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	GDREGISTER_CLASS(ShangCloudMMO);
}

void uninitialize_shangcloud_mmo_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}

extern "C" {
GDExtensionBool GDE_EXPORT shangcloud_mmo_library_init(
		GDExtensionInterfaceGetProcAddress p_get_proc_address,
		const GDExtensionClassLibraryPtr p_library,
		GDExtensionInitialization *r_initialization) {
	godot::GDExtensionBinding::InitObject init_obj(
			p_get_proc_address, p_library, r_initialization);

	init_obj.register_initializer(initialize_shangcloud_mmo_module);
	init_obj.register_terminator(uninitialize_shangcloud_mmo_module);
	init_obj.set_minimum_library_initialization_level(
			MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
}

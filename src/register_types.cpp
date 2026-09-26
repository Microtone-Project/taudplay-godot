#include <gdextension_interface.h>

#include <godot_cpp/classes/editor_plugin_registration.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/godot.hpp>

#include "audio_stream_taud.h"
#include "taud_dispatcher.h"
#include "taud_editor.h"

using namespace godot;

static TaudDispatcher *dispatcher = nullptr;

static void initialize_taudplay(ModuleInitializationLevel p_level)
{
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(AudioStreamTaud);
		// Abstract: only a stream can make one, since a playback needs a song.
		GDREGISTER_ABSTRACT_CLASS(AudioStreamPlaybackTaud);
		GDREGISTER_INTERNAL_CLASS(TaudDispatcher);
		dispatcher = memnew(TaudDispatcher);
	} else if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		GDREGISTER_INTERNAL_CLASS(ResourceImporterTaud);
		GDREGISTER_INTERNAL_CLASS(TaudEditorPlugin);
		EditorPlugins::add_by_type<TaudEditorPlugin>();
	}
}

static void uninitialize_taudplay(ModuleInitializationLevel p_level)
{
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE && dispatcher != nullptr) {
		memdelete(dispatcher);
		dispatcher = nullptr;
	}
}

extern "C" {

GDExtensionBool GDE_EXPORT taudplay_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address,
		GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization)
{
	GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
	init_obj.register_initializer(initialize_taudplay);
	init_obj.register_terminator(uninitialize_taudplay);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
	return init_obj.init();
}
}

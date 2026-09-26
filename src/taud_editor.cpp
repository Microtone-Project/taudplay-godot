#include "taud_editor.h"

#include <godot_cpp/classes/resource_saver.hpp>

#include "audio_stream_taud.h"

namespace godot {

String ResourceImporterTaud::_get_importer_name() const { return "taudplay.taud"; }
String ResourceImporterTaud::_get_visible_name() const { return "Taud song"; }
int32_t ResourceImporterTaud::_get_preset_count() const { return 1; }
String ResourceImporterTaud::_get_preset_name(int32_t p_preset_index) const { return "Default"; }

PackedStringArray ResourceImporterTaud::_get_recognized_extensions() const
{
	PackedStringArray ext;
	ext.push_back("taud");
	return ext;
}

static Dictionary option(const String &p_name, const Variant &p_default)
{
	Dictionary d;
	d["name"] = p_name;
	d["default_value"] = p_default;
	return d;
}

TypedArray<Dictionary> ResourceImporterTaud::_get_import_options(const String &p_path, int32_t p_preset_index) const
{
	TypedArray<Dictionary> opts;
	opts.push_back(option("song", 0));
	opts.push_back(option("loop", false));
	opts.push_back(option("binaural", false));
	return opts;
}

String ResourceImporterTaud::_get_save_extension() const { return "res"; }
String ResourceImporterTaud::_get_resource_type() const { return "AudioStreamTaud"; }
double ResourceImporterTaud::_get_priority() const { return 1.0; }
int32_t ResourceImporterTaud::_get_import_order() const { return 0; }

bool ResourceImporterTaud::_get_option_visibility(const String &p_path, const StringName &p_option_name, const Dictionary &p_options) const
{
	return true;
}

Error ResourceImporterTaud::_import(const String &p_source_file, const String &p_save_path, const Dictionary &p_options,
		const TypedArray<String> &p_platform_variants, const TypedArray<String> &p_gen_files) const
{
	Ref<AudioStreamTaud> stream = AudioStreamTaud::load_from_file(p_source_file);
	if (stream.is_null()) return ERR_FILE_UNRECOGNIZED;
	stream->set_song(p_options.get("song", 0));
	stream->set_loop(p_options.get("loop", false));
	stream->set_binaural(p_options.get("binaural", false));
	return ResourceSaver::get_singleton()->save(stream, p_save_path + String(".") + _get_save_extension());
}

void TaudEditorPlugin::_enter_tree()
{
	importer.instantiate();
	add_import_plugin(importer);
}

void TaudEditorPlugin::_exit_tree()
{
	remove_import_plugin(importer);
	importer.unref();
}

} // namespace godot

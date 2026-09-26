/* Editor side: .taud files import as AudioStreamTaud resources.
 *
 * Copyright (C) 2026 CuriousTorvald. Licensed under the GNU Lesser General
 * Public License version 3 or later — see COPYING.LESSER.
 */
#ifndef TAUD_EDITOR_H
#define TAUD_EDITOR_H

#include <godot_cpp/classes/editor_import_plugin.hpp>
#include <godot_cpp/classes/editor_plugin.hpp>

namespace godot {

class ResourceImporterTaud : public EditorImportPlugin {
	GDCLASS(ResourceImporterTaud, EditorImportPlugin)

protected:
	static void _bind_methods() {}

public:
	String _get_importer_name() const override;
	String _get_visible_name() const override;
	int32_t _get_preset_count() const override;
	String _get_preset_name(int32_t p_preset_index) const override;
	PackedStringArray _get_recognized_extensions() const override;
	TypedArray<Dictionary> _get_import_options(const String &p_path, int32_t p_preset_index) const override;
	String _get_save_extension() const override;
	String _get_resource_type() const override;
	double _get_priority() const override;
	int32_t _get_import_order() const override;
	bool _get_option_visibility(const String &p_path, const StringName &p_option_name, const Dictionary &p_options) const override;
	Error _import(const String &p_source_file, const String &p_save_path, const Dictionary &p_options,
			const TypedArray<String> &p_platform_variants, const TypedArray<String> &p_gen_files) const override;
};

class TaudEditorPlugin : public EditorPlugin {
	GDCLASS(TaudEditorPlugin, EditorPlugin)

	Ref<ResourceImporterTaud> importer;

protected:
	static void _bind_methods() {}

public:
	void _enter_tree() override;
	void _exit_tree() override;
};

} // namespace godot

#endif // TAUD_EDITOR_H

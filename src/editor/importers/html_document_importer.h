#pragma once

#include <godot_cpp/classes/editor_import_plugin.hpp>

namespace godot {
	class HTMLDocumentImporter : public EditorImportPlugin {
		GDCLASS(HTMLDocumentImporter, EditorImportPlugin)

	protected:
		static void _bind_methods() {}

	public:
		static constexpr int32_t FORMAT_VERSION = 2;

		String _get_importer_name() const override;
		String _get_visible_name() const override;
		PackedStringArray _get_recognized_extensions() const override;
		TypedArray<Dictionary> _get_import_options(const String&, int32_t) const override;
		String _get_save_extension() const override;
		String _get_resource_type() const override;
		int32_t _get_import_order() const override;
		int32_t _get_format_version() const override;
		Error _import(const String& p_source_file, const String& p_save_path, const Dictionary&,
			const TypedArray<String>&, const TypedArray<String>&) const override;
	};
}

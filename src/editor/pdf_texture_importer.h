#pragma once

#include <godot_cpp/classes/editor_import_plugin.hpp>

#include <memory>

namespace godot {
	class PDFTextureImporter : public EditorImportPlugin {
		GDCLASS(PDFTextureImporter, EditorImportPlugin)

	public:
		PDFTextureImporter();
		~PDFTextureImporter() override;

		static bool is_supported();
		String _get_importer_name() const override;
		String _get_visible_name() const override;
		PackedStringArray _get_recognized_extensions() const override;
		TypedArray<Dictionary> _get_import_options(const String& p_path, int32_t p_preset_index) const override;
		String _get_save_extension() const override;
		String _get_resource_type() const override;
		bool _can_import_threaded() const override;
		Error _import(const String& p_source_file, const String& p_save_path, const Dictionary& p_options,
			const TypedArray<String>& p_platform_variants, const TypedArray<String>& p_gen_files) const override;

	protected:
		static void _bind_methods() {}

	private:
		struct Backend;
		mutable std::unique_ptr<Backend> backend;
	};
} // namespace godot

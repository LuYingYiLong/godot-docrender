#include "pdf_texture_importer.h"

#include "formats/pdf/reader/pdf_reader.h"
#include "formats/pdf/pdf_texture.h"

#include <godot_cpp/classes/resource_saver.hpp>

#include <limits>

namespace godot {
	bool PDFTextureImporter::is_supported() {
		return PDFReader::is_supported();
	}

	String PDFTextureImporter::_get_importer_name() const {
		return "docrender.pdf_texture";
	}

	String PDFTextureImporter::_get_visible_name() const {
		return tr("PDF Texture", "Godot DocRender");
	}

	PackedStringArray PDFTextureImporter::_get_recognized_extensions() const {
		PackedStringArray extensions;
		extensions.append("pdf");
		return extensions;
	}

	TypedArray<Dictionary> PDFTextureImporter::_get_import_options(const String&, int32_t) const {
		Dictionary page;
		page["name"] = "page";
		page["default_value"] = 1;
		page["property_hint"] = PROPERTY_HINT_RANGE;
		page["hint_string"] = "1,100,1,or_greater";

		Dictionary dpi;
		dpi["name"] = "dpi";
		dpi["default_value"] = PDFTexture::DEFAULT_DPI;
		dpi["property_hint"] = PROPERTY_HINT_RANGE;
		dpi["hint_string"] = "1,600,1,or_greater,suffix:dpi";

		TypedArray<Dictionary> options;
		options.append(page);
		options.append(dpi);
		return options;
	}

	String PDFTextureImporter::_get_save_extension() const {
		return "res";
	}

	String PDFTextureImporter::_get_resource_type() const {
		return PDFTexture::get_class_static();
	}

	bool PDFTextureImporter::_can_import_threaded() const {
		return false;
	}

	Error PDFTextureImporter::_import(const String& p_source_file, const String& p_save_path, const Dictionary& p_options,
		const TypedArray<String>&, const TypedArray<String>&) const {
		const int64_t page_number = p_options.get("page", 1);
		const int64_t dpi = p_options.get("dpi", PDFTexture::DEFAULT_DPI);
		ERR_FAIL_COND_V_MSG(page_number < 1, ERR_INVALID_PARAMETER, "PDF Texture: page numbers start at 1.");
		ERR_FAIL_COND_V_MSG(dpi < 1 || dpi > std::numeric_limits<int32_t>::max(), ERR_INVALID_PARAMETER,
			"PDF Texture: DPI must be a positive 32-bit integer.");

		Ref<PDFReader> reader;
		reader.instantiate();
		const Error error = reader->open_file(p_source_file);
		ERR_FAIL_COND_V_MSG(error != OK, error, vformat("PDF Texture: %s", reader->get_last_error_message()));
		const int64_t page_count = reader->get_page_count();
		ERR_FAIL_COND_V_MSG(page_number > page_count, ERR_INVALID_PARAMETER,
			vformat("PDF Texture: requested page %d, but %s has %d pages.", page_number, p_source_file, page_count));
		const Ref<Image> image = reader->render_page(page_number - 1, dpi);
		ERR_FAIL_COND_V_MSG(image.is_null(), reader->get_last_error(),
			vformat("PDF Texture: %s (file %s, page %d at %d DPI)", reader->get_last_error_message(), p_source_file, page_number, dpi));

		Ref<PDFTexture> texture;
		texture.instantiate();
		texture->set_image(image);
		texture->set_page(static_cast<int32_t>(page_number));
		texture->set_dpi(static_cast<int32_t>(dpi));
		return ResourceSaver::get_singleton()->save(texture, p_save_path + String(".res"), ResourceSaver::FLAG_COMPRESS);
	}
} // namespace godot

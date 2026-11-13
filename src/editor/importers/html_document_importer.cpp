#include "html_document_importer.h"

#include "formats/docrender_source.h"
#include "formats/html/html_document.h"
#include "formats/html/html_markup.h"

namespace godot {
	String HTMLDocumentImporter::_get_importer_name() const {
		return "docrender.html_document";
	}

	String HTMLDocumentImporter::_get_visible_name() const {
		return tr("HTML Document", "Godot DocRender");
	}

	PackedStringArray HTMLDocumentImporter::_get_recognized_extensions() const {
		return { "html", "htm" };
	}

	TypedArray<Dictionary> HTMLDocumentImporter::_get_import_options(const String&, int32_t) const {
		return {};
	}

	String HTMLDocumentImporter::_get_save_extension() const {
		return "res";
	}

	String HTMLDocumentImporter::_get_resource_type() const {
		return String(HTMLDocument::get_class_static());
	}

	int32_t HTMLDocumentImporter::_get_import_order() const {
		return IMPORT_ORDER_SCENE;
	}

	int32_t HTMLDocumentImporter::_get_format_version() const {
		return FORMAT_VERSION;
	}

	Error HTMLDocumentImporter::_import(const String& p_source_file, const String& p_save_path, const Dictionary&,
		const TypedArray<String>&, const TypedArray<String>&) const {
		String text;
		const Error error = docrender::read_document_text(p_source_file, text);
		if (error != OK) {
			return error;
		}
		const String base_dir = p_source_file.get_base_dir();
		Ref<HTMLDocument> document;
		document.instantiate();
		document->set_text(text);
		document->set_base_dir(base_dir);
		const auto assets = docrender::collect_html_assets(text, base_dir);
		document->set_textures(assets.textures);
		document->set_local_stylesheets(assets.stylesheets);
		return ResourceSaver::get_singleton()->save(document, p_save_path + String(".res"));
	}
}

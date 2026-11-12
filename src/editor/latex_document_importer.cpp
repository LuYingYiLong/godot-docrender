#include "latex_document_importer.h"

#include "core/docrender_source.h"
#include "core/latex_document.h"

namespace godot {
	String LatexDocumentImporter::_get_importer_name() const {
		return "docrender.latex_document";
	}

	String LatexDocumentImporter::_get_visible_name() const {
		return tr("LaTeX Document", "Godot DocRender");
	}

	PackedStringArray LatexDocumentImporter::_get_recognized_extensions() const {
		PackedStringArray extensions;
		extensions.append("tex");
		return extensions;
	}

	TypedArray<Dictionary> LatexDocumentImporter::_get_import_options(const String&, int32_t) const {
		return {};
	}

	String LatexDocumentImporter::_get_save_extension() const {
		return "res";
	}

	String LatexDocumentImporter::_get_resource_type() const {
		return String(LatexDocument::get_class_static());
	}

	Error LatexDocumentImporter::_import(const String& p_source_file, const String& p_save_path, const Dictionary&,
		const TypedArray<String>&, const TypedArray<String>&) const {
		return docrender::import_document<LatexDocument>(p_source_file, p_save_path);
	}
} // namespace godot

#include "mermaid_document_importer.h"

#include "formats/docrender_source.h"
#include "formats/mermaid/mermaid_document.h"

namespace godot {
	String MermaidDocumentImporter::_get_importer_name() const {
		return "docrender.mermaid_document";
	}

	String MermaidDocumentImporter::_get_visible_name() const {
		return tr("Mermaid Document", "Godot DocRender");
	}

	PackedStringArray MermaidDocumentImporter::_get_recognized_extensions() const {
		PackedStringArray extensions;
		extensions.append("mmd");
		extensions.append("mermaid");
		return extensions;
	}

	TypedArray<Dictionary> MermaidDocumentImporter::_get_import_options(const String&, int32_t) const {
		return {};
	}

	String MermaidDocumentImporter::_get_save_extension() const {
		return "res";
	}

	String MermaidDocumentImporter::_get_resource_type() const {
		return String(MermaidDocument::get_class_static());
	}

	Error MermaidDocumentImporter::_import(const String& p_source_file, const String& p_save_path, const Dictionary&,
		const TypedArray<String>&, const TypedArray<String>&) const {
		return docrender::import_document<MermaidDocument>(p_source_file, p_save_path);
	}
} // namespace godot

#pragma once

#include <godot_cpp/classes/resource_saver.hpp>

namespace docrender {
	godot::Error read_document_text(const godot::String& p_path, godot::String& r_text);

	template <typename Document>
	godot::Error import_document(const godot::String& p_source_file, const godot::String& p_save_path) {
		godot::String text;
		const godot::Error error = read_document_text(p_source_file, text);
		if (error != godot::OK) {
			return error;
		}
		godot::Ref<Document> document;
		document.instantiate();
		document->set_text(text);
		return godot::ResourceSaver::get_singleton()->save(document, p_save_path + godot::String(".res"));
	}
} // namespace docrender

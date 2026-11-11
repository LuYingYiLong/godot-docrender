#include "markdown_document.h"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void MarkdownDocument::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_text", "text"), &MarkdownDocument::set_text);
	ClassDB::bind_method(D_METHOD("get_text"), &MarkdownDocument::get_text);
	ClassDB::bind_method(D_METHOD("set_source_path", "source_path"), &MarkdownDocument::set_source_path);
	ClassDB::bind_method(D_METHOD("get_source_path"), &MarkdownDocument::get_source_path);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text", PROPERTY_HINT_MULTILINE_TEXT), "set_text", "get_text");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "source_path", PROPERTY_HINT_FILE, "*.md"), "set_source_path", "get_source_path");
}

void MarkdownDocument::set_text(const String& p_text) {
	text = p_text;
	emit_changed();
}

String MarkdownDocument::get_text() const {
	return text;
}

void MarkdownDocument::set_source_path(const String& p_path) {
	source_path = p_path.strip_edges().replace("\\", "/");
	emit_changed();
}

String MarkdownDocument::get_source_path() const {
	return source_path;
}

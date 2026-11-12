#include "mermaid_document.h"

using namespace godot;

void MermaidDocument::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_text"), &MermaidDocument::get_text);
	ClassDB::bind_method(D_METHOD("_set_text", "text"), &MermaidDocument::set_text);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE), "_set_text", "get_text");
}

String MermaidDocument::get_text() const {
	return text;
}

void MermaidDocument::set_text(const String& p_text) {
	if (text != p_text) {
		text = p_text;
		emit_changed();
	}
}

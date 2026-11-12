#include "latex_document.h"

using namespace godot;

void LatexDocument::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_text"), &LatexDocument::get_text);
	ClassDB::bind_method(D_METHOD("_set_text", "text"), &LatexDocument::set_text);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE), "_set_text", "get_text");
}

String LatexDocument::get_text() const {
	return text;
}

void LatexDocument::set_text(const String& p_text) {
	if (text != p_text) {
		text = p_text;
		emit_changed();
	}
}

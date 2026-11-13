#include "html_document.h"

using namespace godot;

void HTMLDocument::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_text", "text"), &HTMLDocument::set_text);
	ClassDB::bind_method(D_METHOD("get_text"), &HTMLDocument::get_text);
	ClassDB::bind_method(D_METHOD("set_base_dir", "base_dir"), &HTMLDocument::set_base_dir);
	ClassDB::bind_method(D_METHOD("get_base_dir"), &HTMLDocument::get_base_dir);
	ClassDB::bind_method(D_METHOD("set_stylesheet", "stylesheet"), &HTMLDocument::set_stylesheet);
	ClassDB::bind_method(D_METHOD("get_stylesheet"), &HTMLDocument::get_stylesheet);
	ClassDB::bind_method(D_METHOD("_set_textures", "textures"), &HTMLDocument::set_textures);
	ClassDB::bind_method(D_METHOD("_get_textures"), &HTMLDocument::get_textures);
	ClassDB::bind_method(D_METHOD("_set_local_stylesheets", "stylesheets"), &HTMLDocument::set_local_stylesheets);
	ClassDB::bind_method(D_METHOD("_get_local_stylesheets"), &HTMLDocument::get_local_stylesheets);
	ClassDB::bind_method(D_METHOD("create_layout"), &HTMLDocument::create_layout);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text", PROPERTY_HINT_MULTILINE_TEXT), "set_text", "get_text");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "base_dir", PROPERTY_HINT_DIR), "set_base_dir", "get_base_dir");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "stylesheet", PROPERTY_HINT_MULTILINE_TEXT), "set_stylesheet", "get_stylesheet");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "_textures", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE), "_set_textures", "_get_textures");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "_local_stylesheets", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE),
		"_set_local_stylesheets", "_get_local_stylesheets");
}

void HTMLDocument::set_text(const String& p_text) {
	if (text != p_text) {
		text = p_text;
		emit_changed();
	}
}

String HTMLDocument::get_text() const {
	return text;
}

void HTMLDocument::set_base_dir(const String& p_dir) {
	if (base_dir != p_dir) {
		base_dir = p_dir;
		emit_changed();
	}
}

String HTMLDocument::get_base_dir() const {
	return base_dir;
}

void HTMLDocument::set_stylesheet(const String& p_stylesheet) {
	if (stylesheet != p_stylesheet) {
		stylesheet = p_stylesheet;
		emit_changed();
	}
}

String HTMLDocument::get_stylesheet() const {
	return stylesheet;
}

void HTMLDocument::set_textures(const Dictionary& p_textures) {
	textures = p_textures.duplicate();
	emit_changed();
}

Dictionary HTMLDocument::get_textures() const {
	return textures.duplicate();
}

void HTMLDocument::set_local_stylesheets(const Dictionary& p_stylesheets) {
	local_stylesheets = p_stylesheets.duplicate();
	emit_changed();
}

Dictionary HTMLDocument::get_local_stylesheets() const {
	return local_stylesheets.duplicate();
}

Ref<HTMLLayout> HTMLDocument::create_layout() const {
	Ref<HTMLLayout> result;
	result.instantiate();
	parse_into(*result.ptr());
	return result;
}

void HTMLDocument::parse_into(HTMLLayout& p_layout) const {
	p_layout.parse_source(text, base_dir, stylesheet, textures, false, {}, {}, local_stylesheets);
}

#include "pdf_texture.h"

using namespace godot;

void PDFTexture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_page"), &PDFTexture::get_page);
	ClassDB::bind_method(D_METHOD("get_dpi"), &PDFTexture::get_dpi);
	ClassDB::bind_method(D_METHOD("_set_page", "page"), &PDFTexture::set_page);
	ClassDB::bind_method(D_METHOD("_set_dpi", "dpi"), &PDFTexture::set_dpi);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "_page", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE), "_set_page", "get_page");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "_dpi", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE), "_set_dpi", "get_dpi");
	const uint32_t usage = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY;
	ADD_PROPERTY(PropertyInfo(Variant::INT, "page", PROPERTY_HINT_NONE, "", usage), "", "get_page");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "dpi", PROPERTY_HINT_NONE, "", usage), "", "get_dpi");
}

int32_t PDFTexture::get_page() const {
	return page;
}

int32_t PDFTexture::get_dpi() const {
	return dpi;
}

void PDFTexture::set_page(int32_t p_page) {
	page = p_page;
}

void PDFTexture::set_dpi(int32_t p_dpi) {
	dpi = p_dpi;
}

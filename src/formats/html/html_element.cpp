#include "html_element.h"

#include "html_layout.h"

#include <litehtml/element.h>

using namespace godot;

namespace {
	void append_text(const litehtml::element& p_element, litehtml::string& r_text) {
		const auto tag = p_element.tag();
		if (tag == litehtml::__tag_before_ || tag == litehtml::__tag_after_) {
			return;
		}
		if (p_element.is_text()) {
			p_element.get_text(r_text);
			return;
		}
		for (const auto& child : p_element.children()) {
			append_text(*child, r_text);
		}
	}

	const char* find_attribute(const litehtml::element& p_element, const String& p_name) {
		if (p_name.is_empty()) {
			return nullptr;
		}
		CharString name = p_name.utf8();
		char* bytes = name.ptrw();
		for (int64_t i = 0; i < name.length(); ++i) {
			if (bytes[i] == '\0') {
				return nullptr;
			}
			if (bytes[i] >= 'A' && bytes[i] <= 'Z') {
				bytes[i] += 'a' - 'A';
			}
		}
		return p_element.get_attr(bytes);
	}
}

void HTMLElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_valid"), &HTMLElement::is_valid);
	ClassDB::bind_method(D_METHOD("get_tag_name"), &HTMLElement::get_tag_name);
	ClassDB::bind_method(D_METHOD("has_attribute", "name"), &HTMLElement::has_attribute);
	ClassDB::bind_method(D_METHOD("get_attribute", "name", "default_value"), &HTMLElement::get_attribute, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_text_content"), &HTMLElement::get_text_content);
	ClassDB::bind_method(D_METHOD("get_parent"), &HTMLElement::get_parent);
	ClassDB::bind_method(D_METHOD("get_children"), &HTMLElement::get_children);
	ClassDB::bind_method(D_METHOD("get_client_rects"), &HTMLElement::get_client_rects);
	ClassDB::bind_method(D_METHOD("get_bounding_rect"), &HTMLElement::get_bounding_rect);
}

HTMLLayout* HTMLElement::get_owner() const {
	auto* layout = Object::cast_to<HTMLLayout>(ObjectDB::get_instance(layout_id));
	return layout && layout->parse_version == parse_version ? layout : nullptr;
}

std::shared_ptr<litehtml::element> HTMLElement::lock_element() const {
	return get_owner() ? element.lock() : nullptr;
}

bool HTMLElement::is_valid() const {
	return get_owner() && !element.expired();
}

String HTMLElement::get_tag_name() const {
	const auto node = lock_element();
	return node ? String::utf8(node->get_tagName()) : String();
}

bool HTMLElement::has_attribute(const String& p_name) const {
	const auto node = lock_element();
	return node && find_attribute(*node, p_name);
}

String HTMLElement::get_attribute(const String& p_name, const String& p_default_value) const {
	const auto node = lock_element();
	const char* value = node ? find_attribute(*node, p_name) : nullptr;
	return value ? String::utf8(value) : p_default_value;
}

String HTMLElement::get_text_content() const {
	const auto node = lock_element();
	if (!node) {
		return {};
	}
	litehtml::string text;
	append_text(*node, text);
	return String::utf8(text.c_str());
}

Ref<HTMLElement> HTMLElement::get_parent() const {
	if (const auto node = lock_element()) {
		return get_owner()->make_element(node->parent());
	}
	return {};
}

TypedArray<HTMLElement> HTMLElement::get_children() const {
	TypedArray<HTMLElement> children;
	if (const auto node = lock_element()) {
		HTMLLayout* layout = get_owner();
		for (const auto& child : node->children()) {
			const Ref<HTMLElement> wrapped = layout->make_element(child);
			if (wrapped.is_valid()) {
				children.append(wrapped);
			}
		}
	}
	return children;
}

TypedArray<Rect2> HTMLElement::get_client_rects() const {
	const HTMLLayout* layout = get_owner();
	return layout ? layout->get_element_rects(element.lock()) : TypedArray<Rect2>();
}

Rect2 HTMLElement::get_bounding_rect() const {
	const TypedArray<Rect2> boxes = get_client_rects();
	if (boxes.is_empty()) {
		return {};
	}
	Rect2 bounds = boxes[0];
	for (int64_t i = 1; i < boxes.size(); ++i) {
		bounds = bounds.merge(boxes[i]);
	}
	return bounds;
}

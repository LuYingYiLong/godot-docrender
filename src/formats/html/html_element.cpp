#include "html_element.h"

#include "html_layout.h"

#include <godot_cpp/templates/hash_set.hpp>
#include <litehtml/element.h>

#include <string_view>

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

	CharString normalize_attribute_name(const String& p_name) {
		if (p_name.is_empty()) {
			return {};
		}
		CharString name = p_name.utf8();
		char* bytes = name.ptrw();
		for (int64_t i = 0; i < name.length(); ++i) {
			if (bytes[i] == '\0') {
				return {};
			}
			if (bytes[i] >= 'A' && bytes[i] <= 'Z') {
				bytes[i] += 'a' - 'A';
			}
		}
		return name;
	}

	String dataset_key(const String& p_name) {
		String key;
		for (int64_t i = 5; i < p_name.length(); ++i) {
			const char32_t ch = p_name[i];
			if (ch == '-' && i + 1 < p_name.length() && p_name[i + 1] >= 'a' && p_name[i + 1] <= 'z') {
				key += String::chr(p_name[++i] - ('a' - 'A'));
			}
			else {
				key += String::chr(ch);
			}
		}
		return key;
	}

	bool is_class_separator(char32_t p_character) {
		return p_character == ' ' || p_character == '\t' || p_character == '\n' || p_character == '\r' || p_character == '\f';
	}

	bool is_valid_class_name(const String& p_name) {
		if (p_name.is_empty()) {
			return false;
		}
		for (int64_t i = 0; i < p_name.length(); ++i) {
			if (p_name[i] == '\0' || is_class_separator(p_name[i])) {
				return false;
			}
		}
		return true;
	}

	bool is_valid_attribute_name(const CharString& p_name) {
		if (p_name.length() == 0) {
			return false;
		}
		for (const unsigned char byte : std::string_view(p_name.get_data(), p_name.length())) {
			if (byte <= ' ' || byte == '/' || byte == '>' || byte == '<' || byte == '=' || byte == '"' || byte == '\'') {
				return false;
			}
		}
		return true;
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
	ClassDB::bind_method(D_METHOD("query_selector", "selector"), &HTMLElement::query_selector);
	ClassDB::bind_method(D_METHOD("query_selector_all", "selector"), &HTMLElement::query_selector_all);
	ClassDB::bind_method(D_METHOD("matches", "selector"), &HTMLElement::matches);
	ClassDB::bind_method(D_METHOD("closest", "selector"), &HTMLElement::closest);
	ClassDB::bind_method(D_METHOD("get_attributes"), &HTMLElement::get_attributes);
	ClassDB::bind_method(D_METHOD("get_dataset"), &HTMLElement::get_dataset);
	ClassDB::bind_method(D_METHOD("get_class_list"), &HTMLElement::get_class_list);
	ClassDB::bind_method(D_METHOD("has_class", "class_name"), &HTMLElement::has_class);
	ClassDB::bind_method(D_METHOD("set_text_content", "text"), &HTMLElement::set_text_content);
	ClassDB::bind_method(D_METHOD("set_attribute", "name", "value"), &HTMLElement::set_attribute);
	ClassDB::bind_method(D_METHOD("remove_attribute", "name"), &HTMLElement::remove_attribute);
	ClassDB::bind_method(D_METHOD("add_class", "class_name"), &HTMLElement::add_class);
	ClassDB::bind_method(D_METHOD("remove_class", "class_name"), &HTMLElement::remove_class);
	ClassDB::bind_method(D_METHOD("toggle_class", "class_name", "force"), &HTMLElement::toggle_class, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("get_inner_html"), &HTMLElement::get_inner_html);
	ClassDB::bind_method(D_METHOD("set_inner_html", "html"), &HTMLElement::set_inner_html);
	ClassDB::bind_method(D_METHOD("append_child", "child"), &HTMLElement::append_child);
	ClassDB::bind_method(D_METHOD("remove"), &HTMLElement::remove);
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
	const CharString name = normalize_attribute_name(p_name);
	return node && name.length() > 0 && get_owner()->get_element_attribute(*node, name.get_data());
}

String HTMLElement::get_attribute(const String& p_name, const String& p_default_value) const {
	const auto node = lock_element();
	const CharString name = normalize_attribute_name(p_name);
	const char* value = node && name.length() > 0 ? get_owner()->get_element_attribute(*node, name.get_data()) : nullptr;
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

Ref<HTMLElement> HTMLElement::query_selector(const String& p_selector) const {
	if (const auto node = lock_element()) {
		const auto matches = get_owner()->select_elements(p_selector, HTMLLayout::SelectorQuery::FIRST, node);
		return matches.is_empty() ? Ref<HTMLElement>() : Ref<HTMLElement>(matches[0]);
	}
	return {};
}

TypedArray<HTMLElement> HTMLElement::query_selector_all(const String& p_selector) const {
	if (const auto node = lock_element()) {
		return get_owner()->select_elements(p_selector, HTMLLayout::SelectorQuery::ALL, node);
	}
	return {};
}

bool HTMLElement::matches(const String& p_selector) const {
	const auto node = lock_element();
	return node && !get_owner()->select_elements(p_selector, HTMLLayout::SelectorQuery::SELF, node).is_empty();
}

Ref<HTMLElement> HTMLElement::closest(const String& p_selector) const {
	if (const auto node = lock_element()) {
		const auto matches = get_owner()->select_elements(p_selector, HTMLLayout::SelectorQuery::CLOSEST, node);
		return matches.is_empty() ? Ref<HTMLElement>() : Ref<HTMLElement>(matches[0]);
	}
	return {};
}

Dictionary HTMLElement::get_attributes() const {
	if (const auto node = lock_element()) {
		return get_owner()->get_element_attributes(*node);
	}
	return {};
}

Dictionary HTMLElement::get_dataset() const {
	const Dictionary attributes = get_attributes();
	const Array keys = attributes.keys();
	Dictionary dataset;
	for (int64_t i = 0; i < keys.size(); ++i) {
		const String name = keys[i];
		if (name.begins_with("data-")) {
			dataset[dataset_key(name)] = attributes[name];
		}
	}
	return dataset;
}

PackedStringArray HTMLElement::get_class_list() const {
	const String classes = get_attribute("class");
	PackedStringArray result;
	HashSet<String> seen;
	int64_t start = 0;
	for (int64_t i = 0; i <= classes.length(); ++i) {
		if (i < classes.length() && !is_class_separator(classes[i])) {
			continue;
		}
		if (i > start) {
			const String name = classes.substr(start, i - start);
			if (!seen.has(name)) {
				seen.insert(name);
				result.append(name);
			}
		}
		start = i + 1;
	}
	return result;
}

bool HTMLElement::has_class(const String& p_class_name) const {
	return get_class_list().has(p_class_name);
}

Error HTMLElement::set_text_content(const String& p_text) {
	const auto node = lock_element();
	return node ? get_owner()->replace_children(node, p_text, false) : ERR_INVALID_PARAMETER;
}

Error HTMLElement::set_attribute(const String& p_name, const String& p_value) {
	const auto node = lock_element();
	const CharString name = normalize_attribute_name(p_name);
	const CharString value = p_value.utf8();
	if (!node || !is_valid_attribute_name(name) ||
		std::string_view(value.get_data(), value.length()).find('\0') != std::string_view::npos) {
		return ERR_INVALID_PARAMETER;
	}
	return get_owner()->edit_attribute(node, name.get_data(), value.get_data());
}

Error HTMLElement::remove_attribute(const String& p_name) {
	const auto node = lock_element();
	const CharString name = normalize_attribute_name(p_name);
	return node && is_valid_attribute_name(name) ?
		get_owner()->edit_attribute(node, name.get_data(), nullptr) : ERR_INVALID_PARAMETER;
}

Error HTMLElement::add_class(const String& p_class_name) {
	return toggle_class(p_class_name, true);
}

Error HTMLElement::remove_class(const String& p_class_name) {
	return toggle_class(p_class_name, false);
}

Error HTMLElement::toggle_class(const String& p_class_name, const Variant& p_force) {
	if (!is_valid() || !is_valid_class_name(p_class_name) ||
		(p_force.get_type() != Variant::NIL && p_force.get_type() != Variant::BOOL)) {
		return ERR_INVALID_PARAMETER;
	}
	PackedStringArray classes = get_class_list();
	const int64_t index = classes.find(p_class_name);
	const bool present = index >= 0;
	const bool include = p_force.get_type() == Variant::NIL ? !present : bool(p_force);
	if (include == present) {
		return OK;
	}
	if (include) {
		classes.append(p_class_name);
	}
	else {
		classes.remove_at(index);
	}
	return set_attribute("class", String(" ").join(classes));
}

String HTMLElement::get_inner_html() const {
	const auto node = lock_element();
	return node ? get_owner()->serialize_children(*node) : String();
}

Error HTMLElement::set_inner_html(const String& p_html) {
	const auto node = lock_element();
	return node ? get_owner()->replace_children(node, p_html, true) : ERR_INVALID_PARAMETER;
}

Error HTMLElement::append_child(const Ref<HTMLElement>& p_child) {
	HTMLLayout* owner = get_owner();
	const auto parent = lock_element();
	const auto child = p_child.is_valid() ? p_child->lock_element() : nullptr;
	if (!parent || !child || p_child->get_owner() != owner) {
		return ERR_INVALID_PARAMETER;
	}
	return owner->move_element(child, parent);
}

Error HTMLElement::remove() {
	const auto node = lock_element();
	return node ? get_owner()->move_element(node, {}) : ERR_INVALID_PARAMETER;
}

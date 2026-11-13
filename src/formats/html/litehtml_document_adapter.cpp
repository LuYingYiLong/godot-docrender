#include "litehtml_document_adapter.h"

#include "litehtml_container.h"

#include <gumbo.h>
#include <litehtml/el_space.h>
#include <litehtml/html_tag.h>
#include <litehtml/render_item.h>

#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace litehtml {
	using docrender::LiteHTMLContainer;

	document::ptr docrender_editor::parse(const godot::CharString& p_html, LiteHTMLContainer& p_container,
		const string& p_default_styles, const string& p_user_styles) {
		auto doc = parse_tree(p_html, p_container);
		if (doc) {
			restyle(doc, p_container, p_default_styles, p_user_styles);
		}
		return doc;
	}

	document::ptr docrender_editor::parse_tree(const godot::CharString& p_html, document_container& p_container) {
		auto doc = std::make_shared<document>(&p_container);
		const auto destroy = [](GumboOutput* p_output) { gumbo_destroy_output(&kGumboDefaultOptions, p_output); };
		// Godot has already decoded the source, regardless of the HTML meta charset.
		const std::unique_ptr<GumboOutput, decltype(destroy)> output(doc->parse_html(
			estring(string(p_html.get_data(), p_html.length()), encoding::utf_8)), destroy);
		if (!output || !output->document || !output->root) {
			return {};
		}
		switch (output->document->v.document.doc_type_quirks_mode) {
			case GUMBO_DOCTYPE_NO_QUIRKS: doc->m_mode = no_quirks_mode; break;
			case GUMBO_DOCTYPE_QUIRKS: doc->m_mode = quirks_mode; break;
			case GUMBO_DOCTYPE_LIMITED_QUIRKS: doc->m_mode = limited_quirks_mode; break;
		}
		// The mode determines how node creation normalizes class and ID selectors.
		elements_list roots;
		doc->create_node(output->root, roots, true, true);
		if (roots.empty()) {
			return {};
		}
		doc->m_root = std::move(roots.back());
		doc->m_root->set_pseudo_class(_root_, true);
		return doc;
	}

	void docrender_editor::reset_link(html_tag& p_element, bool p_has_href) {
		auto& classes = p_element.m_pseudo_classes;
		classes.erase(std::remove(classes.begin(), classes.end(), _link_), classes.end());
		if (p_has_href) {
			classes.push_back(_link_);
		}
	}

	void docrender_editor::remove_attribute(html_tag& p_element, const char* p_name) {
		const std::string_view name(p_name);
		if (name == "class" || name == "id") {
			p_element.set_attr(p_name, "");
		}
		p_element.m_attrs.erase(p_name);
	}

	elements_list docrender_editor::parse_fragment(document& p_document, const element& p_parent, const godot::CharString& p_html) {
		GumboOptions options = kGumboDefaultOptions;
		options.fragment_context = gumbo_tag_enum(p_parent.get_tagName());
		if (options.fragment_context == GUMBO_TAG_UNKNOWN) {
			options.fragment_context = GUMBO_TAG_DIV;
		}
		const auto destroy = [&options](GumboOutput* p_output) { gumbo_destroy_output(&options, p_output); };
		const std::unique_ptr<GumboOutput, decltype(destroy)> output(
			gumbo_parse_with_options(&options, p_html.get_data(), p_html.length()), destroy);
		if (!output || !output->root) {
			throw std::runtime_error("Could not parse the HTML fragment.");
		}
		elements_list children;
		p_document.create_node(output->root, children, true, false);
		return children;
	}

	void docrender_editor::replace_children(element& p_parent, elements_list p_children) {
		for (const auto& child : p_parent.m_children) {
			child->parent(nullptr);
		}
		for (const auto& child : p_children) {
			child->parent(p_parent.shared_from_this());
		}
		p_parent.m_children.swap(p_children);
	}

	bool docrender_editor::move_child(const element::ptr& p_child, element& p_parent) {
		auto* to = dynamic_cast<html_tag*>(&p_parent);
		if (!to) {
			return false;
		}
		const auto old_parent = p_child->parent();
		if (!old_parent) {
			return to->appendChild(p_child);
		}
		auto* from = dynamic_cast<html_tag*>(old_parent.get());
		if (!from) {
			return false;
		}
		const auto child = std::find(from->m_children.begin(), from->m_children.end(), p_child);
		if (child == from->m_children.end()) {
			return false;
		}
		to->m_children.splice(to->m_children.end(), from->m_children, child);
		p_child->parent(to->shared_from_this());
		return true;
	}

	void docrender_editor::reset_tree(element& p_element) {
		p_element.m_renders.clear();
		p_element.m_used_styles.clear();
		p_element.m_counter_values.clear();
		p_element.m_children.remove_if([](const element::ptr& p_child) {
			const bool generated = p_child->tag() == __tag_before_ || p_child->tag() == __tag_after_;
			if (generated) {
				p_child->parent(nullptr);
			}
			return generated;
		});
		if (auto* tag = dynamic_cast<html_tag*>(&p_element)) {
			tag->m_style.clear();
			if (tag->tag() == _a_) {
				reset_link(*tag, false);
			}
		}
		for (const auto& child : p_element.children()) {
			reset_tree(*child);
		}
	}

	bool docrender_editor::media_changed(document& p_document) {
		p_document.container()->get_media_features(p_document.m_media);
		return p_document.update_media_lists(p_document.m_media);
	}

	void docrender_editor::restyle(const document::ptr& p_document, LiteHTMLContainer& p_container,
		const string& p_default_styles, const string& p_user_styles) {
		p_container.reset_stylesheet_reads();
		auto& doc = *p_document;
		const auto& root = doc.m_root;
		doc.m_root_render.reset();
		doc.m_tabular_elements.clear();
		doc.m_fixed_boxes.clear();
		doc.m_size = {};
		reset_tree(*root);
		doc.m_css.clear();
		doc.m_media_lists.clear();
		doc.m_master_css = {};
		doc.m_styles = {};
		doc.m_user_css = {};

		doc.container()->get_media_features(doc.m_media);
		doc.m_master_css.parse_css_stylesheet(p_default_styles, "", p_document);
		doc.m_master_css.sort_selectors();
		if (!p_user_styles.empty()) {
			doc.m_user_css.parse_css_stylesheet(p_user_styles, "", p_document);
			doc.m_user_css.sort_selectors();
		}
		doc.update_media_lists(doc.m_media);
		root->apply_stylesheet(doc.m_master_css);
		root->parse_attributes();
		for (const auto& sheet : doc.m_css) {
			media_query_list_list::ptr media;
			if (!sheet.media.empty()) {
				media = std::make_shared<media_query_list_list>();
				media->add(parse_media_query_list(sheet.media, p_document));
			}
			doc.m_styles.parse_css_stylesheet(sheet.text, sheet.baseurl, p_document, media);
		}
		doc.m_styles.sort_selectors();
		doc.update_media_lists(doc.m_media);
		root->apply_stylesheet(doc.m_styles);
		root->apply_stylesheet(doc.m_user_css);
		root->compute_styles();

		doc.m_root_render = root->create_render_item(nullptr);
		doc.fix_tables_layout();
		if (doc.m_root_render) {
			doc.m_root_render = doc.m_root_render->init();
		}
	}

	bool docrender_editor::set_attribute(element& p_element, const char* p_name, const char* p_value) {
		auto* tag = dynamic_cast<html_tag*>(&p_element);
		if (!tag) {
			return false;
		}
		if (p_value) {
			tag->set_attr(p_name, p_value);
		}
		else {
			remove_attribute(*tag, p_name);
		}
		if (tag->tag() == _a_ && std::string_view(p_name) == "href") {
			reset_link(*tag, p_value != nullptr);
		}
		return true;
	}

	elements_list docrender_editor::create_text(const document::ptr& p_document, const char* p_text, bool p_split) {
		elements_list children;
		if (*p_text && !p_split) {
			children.push_back(std::make_shared<el_text>(p_text, p_document));
		}
		else if (*p_text) {
			p_document->container()->split_text(p_text,
				[&](const char* p_word) { children.push_back(std::make_shared<el_text>(p_word, p_document)); },
				[&](const char* p_space) { children.push_back(std::make_shared<el_space>(p_space, p_document)); });
		}
		return children;
	}
}

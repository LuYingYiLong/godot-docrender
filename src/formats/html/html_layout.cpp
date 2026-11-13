#include "html_layout.h"

#include "html_markup.h"
#include "litehtml_container.h"

#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <gumbo.h>
#include <litehtml/css_parser.h>
#include <litehtml/css_selector.h>
#include <litehtml/document.h>
#include <litehtml/el_space.h>
#include <litehtml/html_tag.h>
#include <litehtml/render_item.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <string_view>

using namespace godot;
using docrender::LiteHTMLContainer;

namespace litehtml {
	// The staged litehtml headers grant access to rebuild the existing document in place.
	struct docrender_editor {
		static void reset_link(html_tag& p_element, bool p_has_href) {
			auto& classes = p_element.m_pseudo_classes;
			classes.erase(std::remove(classes.begin(), classes.end(), _link_), classes.end());
			if (p_has_href) {
				classes.push_back(_link_);
			}
		}

		static void remove_attribute(html_tag& p_element, const char* p_name) {
			if (std::string_view(p_name) == "class" || std::string_view(p_name) == "id") {
				p_element.set_attr(p_name, "");
			}
			p_element.m_attrs.erase(p_name);
		}

		static elements_list parse_fragment(document& p_document, const element& p_parent, const godot::CharString& p_html) {
			GumboOptions options = kGumboDefaultOptions;
			options.fragment_context = gumbo_tag_enum(p_parent.get_tagName());
			if (options.fragment_context == GUMBO_TAG_UNKNOWN) {
				options.fragment_context = GUMBO_TAG_DIV;
			}
			const auto destroy = [&options](GumboOutput* p_output) { gumbo_destroy_output(&options, p_output); };
			const std::unique_ptr<GumboOutput, decltype(destroy)> output(
				gumbo_parse_with_options(&options, p_html.get_data(), p_html.length()), destroy);
			elements_list children;
			p_document.create_node(output->root, children, true, false);
			return children;
		}

		static elements_list create_text(const document::ptr& p_document, const char* p_text, bool p_split) {
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

		static void replace_children(element& p_parent, elements_list p_children) {
			for (const auto& child : p_parent.m_children) {
				child->parent(nullptr);
			}
			for (const auto& child : p_children) {
				child->parent(p_parent.shared_from_this());
			}
			p_parent.m_children.swap(p_children);
		}

		static void move(html_tag& p_old_parent, html_tag& p_new_parent, const element::ptr& p_child) {
			const auto child = std::find(p_old_parent.m_children.begin(), p_old_parent.m_children.end(), p_child);
			p_new_parent.m_children.splice(p_new_parent.m_children.end(), p_old_parent.m_children, child);
			p_child->parent(p_new_parent.shared_from_this());
		}

		static void reset_tree(element& p_element) {
			p_element.m_renders.clear();
			p_element.m_used_styles.clear();
			p_element.m_counter_values.clear();
			p_element.m_children.remove_if([](const element::ptr& p_child) {
				return p_child->tag() == __tag_before_ || p_child->tag() == __tag_after_;
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

		static bool media_changed(document& p_document) {
			p_document.container()->get_media_features(p_document.m_media);
			return p_document.update_media_lists(p_document.m_media);
		}

		static void rebuild(document& p_document, const string& p_master, const string& p_user) {
			const auto doc = p_document.shared_from_this();
			const auto& root = doc->m_root;
			doc->m_root_render.reset();
			doc->m_tabular_elements.clear();
			doc->m_fixed_boxes.clear();
			doc->m_size = {};
			reset_tree(*root);
			doc->m_css.clear();
			doc->m_media_lists.clear();
			doc->m_master_css = {};
			doc->m_styles = {};
			doc->m_user_css = {};
			doc->container()->get_media_features(doc->m_media);
			doc->m_master_css.parse_css_stylesheet(p_master, "", doc);
			doc->m_master_css.sort_selectors();
			if (!p_user.empty()) {
				doc->m_user_css.parse_css_stylesheet(p_user, "", doc);
				doc->m_user_css.sort_selectors();
			}
			doc->update_media_lists(doc->m_media);
			root->apply_stylesheet(doc->m_master_css);
			root->parse_attributes();
			for (const auto& sheet : doc->m_css) {
				media_query_list_list::ptr media;
				if (!sheet.media.empty()) {
					media = std::make_shared<media_query_list_list>();
					media->add(parse_media_query_list(sheet.media, doc));
				}
				doc->m_styles.parse_css_stylesheet(sheet.text, sheet.baseurl, doc, media);
			}
			doc->m_styles.sort_selectors();
			doc->update_media_lists(doc->m_media);
			root->apply_stylesheet(doc->m_styles);
			root->apply_stylesheet(doc->m_user_css);
			root->compute_styles();
			doc->m_root_render = root->create_render_item(nullptr);
			doc->fix_tables_layout();
			if (doc->m_root_render) {
				doc->m_root_render = doc->m_root_render->init();
			}
		}
	};
}

namespace {
	struct CanvasDrawing {
		Callable redraw;
		Callable exiting;
		std::vector<RID> items;
		std::vector<Ref<ShaderMaterial>> materials;
		int32_t run_count = 0;

		void clear() {
			if (RenderingServer* server = RenderingServer::get_singleton()) {
				for (auto item = items.rbegin(); item != items.rend(); ++item) {
					server->free_rid(*item);
				}
			}
			items.clear();
			materials.clear();
			run_count = 0;
		}

		RID add_item(RenderingServer& p_server, const RID& p_parent, int32_t p_draw_index) {
			const RID item = p_server.canvas_item_create();
			p_server.canvas_item_set_parent(item, p_parent);
			p_server.canvas_item_set_draw_index(item, p_draw_index);
			items.push_back(item);
			return item;
		}
	};

	using RenderItems = std::vector<std::shared_ptr<litehtml::render_item>>;
	using RenderIndex = HashMap<const litehtml::element*, RenderItems>;

	bool is_valid_tag_name(std::string_view p_name) {
		const auto is_letter = [](unsigned char p_byte) {
			return (p_byte >= 'A' && p_byte <= 'Z') || (p_byte >= 'a' && p_byte <= 'z');
		};
		if (p_name.empty() || !is_letter(p_name.front())) {
			return false;
		}
		return std::all_of(p_name.begin(), p_name.end(), [&](unsigned char p_byte) {
			return is_letter(p_byte) || (p_byte >= '0' && p_byte <= '9') ||
				p_byte == '-' || p_byte == '_' || p_byte == '.' || p_byte == ':' || p_byte >= 0x80;
		});
	}

	const litehtml::element* tree_root(const litehtml::element& p_element) {
		const litehtml::element* root = &p_element;
		for (auto parent = root->parent(); parent; parent = parent->parent()) {
			root = parent.get();
		}
		return root;
	}

	bool is_void_element(const litehtml::element& p_element) {
		const std::string_view tag = p_element.get_tagName();
		static constexpr std::string_view void_tags[] = {
			"area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "param",
			"source", "track", "wbr", "basefont", "bgsound", "frame", "keygen",
		};
		return std::find(std::begin(void_tags), std::end(void_tags), tag) != std::end(void_tags);
	}

	bool can_contain_elements(const litehtml::element& p_element) {
		const std::string_view tag = p_element.get_tagName();
		return !is_void_element(p_element) && tag != "style" && tag != "script" &&
			tag != "title" && tag != "textarea" && tag != "xmp" && tag != "iframe" &&
			tag != "noembed" && tag != "noframes" && tag != "plaintext";
	}

	bool has_raw_text(const litehtml::element& p_element) {
		const std::string_view tag = p_element.get_tagName();
		return tag == "style" || tag == "script" || tag == "xmp" || tag == "iframe" ||
			tag == "noembed" || tag == "noframes" || tag == "plaintext";
	}

	void append_escaped(std::string& r_html, std::string_view p_value, bool p_attribute) {
		for (const char byte : p_value) {
			switch (byte) {
			case '&': r_html += "&amp;"; break;
			case '<': r_html += "&lt;"; break;
			case '>': r_html += "&gt;"; break;
			case '"': r_html += p_attribute ? "&quot;" : "\""; break;
			default: r_html += byte; break;
			}
		}
	}

	bool is_html_element(const litehtml::element& p_element) {
		const auto tag = p_element.tag();
		return tag != litehtml::empty_id && tag != litehtml::__tag_before_ && tag != litehtml::__tag_after_ && *p_element.get_tagName();
	}

	template <typename Visitor>
	bool visit_elements(const litehtml::element::ptr& p_node, const Visitor& p_visit) {
		if (!p_node || p_node->tag() == litehtml::__tag_before_ || p_node->tag() == litehtml::__tag_after_) {
			return false;
		}
		if (is_html_element(*p_node) && p_visit(p_node)) {
			return true;
		}
		for (const auto& child : p_node->children()) {
			if (visit_elements(child, p_visit)) {
				return true;
			}
		}
		return false;
	}

	void index_render_items(const std::shared_ptr<litehtml::render_item>& p_item, RenderIndex& r_items) {
		if (p_item->css().get_display() != litehtml::display_inline_text) {
			r_items[p_item->src_el().get()].emplace_back(p_item);
		}
		for (const auto& child : p_item->children()) {
			index_render_items(child, r_items);
		}
	}

	template <typename Visitor>
	void visit_boxes(const litehtml::render_item& p_item, bool p_use_placement, const Visitor& p_visit) {
		litehtml::position::vector boxes;
		p_item.get_rendering_boxes(boxes);
		if (boxes.empty() && p_use_placement) {
			boxes.push_back(p_item.get_placement());
		}
		for (const auto& box : boxes) {
			const Rect2 rect(box.x, box.y, box.width, box.height);
			if (rect.is_finite() && rect.size.x >= 0 && rect.size.y >= 0) {
				p_visit(rect);
			}
		}
	}

	void collect_anchors(const litehtml::element& p_element, const RenderIndex& p_items, HashMap<String, Rect2>& r_anchors) {
		if (p_element.css().get_display() == litehtml::display_none) {
			return;
		}
		const char* id = p_element.get_attr("id");
		if (id && *id) {
			const String name = String::utf8(id);
			const auto* renderers = p_items.getptr(&p_element);
			if (renderers && !r_anchors.has(name)) {
				std::optional<Rect2> bounds;
				for (const auto& item : *renderers) {
					if (item->is_visible()) {
						visit_boxes(*item, true, [&](const Rect2& rect) {
							bounds = bounds ? bounds->merge(rect) : rect;
						});
					}
				}
				if (bounds) {
					r_anchors.insert(name, *bounds);
				}
			}
		}
		for (const auto& child : p_element.children()) {
			collect_anchors(*child, p_items, r_anchors);
		}
	}

	bool has_balanced_selector_tokens(const litehtml::css_token_vector& p_tokens) {
		using namespace litehtml;
		std::vector<css_token_type> closing;
		for (const auto& token : p_tokens) {
			switch (token.type) {
			case LEFT_BRACKET:
				closing.push_back(RIGHT_BRACKET);
				break;
			case FUNCTION:
			case LEFT_PAREN:
				closing.push_back(RIGHT_PAREN);
				break;
			case RIGHT_BRACKET:
			case RIGHT_PAREN:
				if (closing.empty() || closing.back() != token.type) {
					return false;
				}
				closing.pop_back();
				break;
			case BAD_STRING:
			case BAD_URL:
				return false;
			default:
				break;
			}
		}
		return closing.empty();
	}

	class SelectorScope {
		litehtml::element* scope;
		const litehtml::string_id scope_id = litehtml::_id("scope");
		bool added;

	public:
		explicit SelectorScope(litehtml::element* p_scope) :
			scope(p_scope), added(scope && scope->set_pseudo_class(scope_id, true)) {}

		~SelectorScope() {
			if (added) {
				scope->set_pseudo_class(scope_id, false);
			}
		}
	};

} // namespace

struct HTMLElement::DetachedTree {
	ObjectID owner;
	uint64_t version;
	litehtml::element::ptr root;

	DetachedTree(ObjectID p_owner, uint64_t p_version, const litehtml::element::ptr& p_root) :
		owner(p_owner), version(p_version), root(p_root) {}

	~DetachedTree();
};

struct HTMLLayout::State {
	// Render items are cleared before the document releases its fonts through the container.
	std::unique_ptr<LiteHTMLContainer> container;
	litehtml::document::ptr document;
	HashMap<uint64_t, CanvasDrawing> drawings;
	RenderIndex render_items;
	HashMap<const litehtml::element*, ObjectID> element_handles;
	HashMap<const litehtml::element*, std::weak_ptr<HTMLElement::DetachedTree>> detached_trees;
	HashMap<String, Rect2> anchors;
	Vector2i viewport_size;
	Vector2i content_size;
	std::string master_styles;
	std::string user_styles;
	Callable edit_callback;
	String error;
	Error parse_error = OK;
	bool has_layout = false;
	bool implicit_empty_document = false;
	bool styles_dirty = false;
	bool edited = false;
	bool reflow_queued = false;
};

HTMLElement::DetachedTree::~DetachedTree() {
	auto* layout = Object::cast_to<HTMLLayout>(ObjectDB::get_instance(owner));
	if (root && layout && layout->parse_version == version) {
		auto& current = *layout->state;
		current.detached_trees.erase(root.get());
		visit_elements(root, [&](const litehtml::element::ptr& p_element) {
			current.element_handles.erase(p_element.get());
			current.container->forget_source_attributes(*p_element);
			return false;
		});
	}
}

void HTMLLayout::_bind_methods() {
	ClassDB::bind_method(D_METHOD("parse", "html", "base_dir", "stylesheet"), &HTMLLayout::parse,
		DEFVAL(String("res://")), DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("layout", "viewport_size"), &HTMLLayout::layout);
	ClassDB::bind_method(D_METHOD("get_content_size"), &HTMLLayout::get_content_size);
	ClassDB::bind_method(D_METHOD("get_plain_text"), &HTMLLayout::get_plain_text);
	ClassDB::bind_method(D_METHOD("hit_test_text", "position"), &HTMLLayout::hit_test_text);
	ClassDB::bind_method(D_METHOD("get_selection_rects", "from", "to"), &HTMLLayout::get_selection_rects);
	ClassDB::bind_method(D_METHOD("draw", "canvas", "position", "clip"), &HTMLLayout::draw,
		DEFVAL(Vector2()), DEFVAL(Rect2()));
	ClassDB::bind_method(D_METHOD("get_link_at", "position"), &HTMLLayout::get_link_at);
	ClassDB::bind_method(D_METHOD("get_anchor_rect", "id"), &HTMLLayout::get_anchor_rect);
	ClassDB::bind_method(D_METHOD("create_element", "tag_name"), &HTMLLayout::create_element);
	ClassDB::bind_method(D_METHOD("query_selector", "selector"), &HTMLLayout::query_selector);
	ClassDB::bind_method(D_METHOD("query_selector_all", "selector"), &HTMLLayout::query_selector_all);
	ClassDB::bind_method(D_METHOD("get_element_by_id", "id"), &HTMLLayout::get_element_by_id);
	ClassDB::bind_method(D_METHOD("get_element_at", "position"), &HTMLLayout::get_element_at);
	ClassDB::bind_method(D_METHOD("get_render_error"), &HTMLLayout::get_render_error);
	ClassDB::bind_method(D_METHOD("clear"), &HTMLLayout::clear);
}

HTMLLayout::HTMLLayout() :
	state(std::make_unique<State>()) {
}

HTMLLayout::~HTMLLayout() {
	clear();
}

Error HTMLLayout::parse(const String& p_html, const String& p_base_dir, const String& p_stylesheet) {
	return parse_source(p_html, p_base_dir, p_stylesheet, Dictionary());
}

Error HTMLLayout::parse_source(const String& p_html, const String& p_base_dir, const String& p_stylesheet,
	const Dictionary& p_textures, bool p_fit_images, const String& p_default_stylesheet, const Fonts& p_fonts,
	const Dictionary& p_local_stylesheets) {
	clear();
	const std::optional<String> base_dir = docrender::normalize_html_resource_path(p_base_dir);
	if (!base_dir) {
		state->error = "The base directory must be a project resource path (res://).";
		return state->parse_error = ERR_INVALID_PARAMETER;
	}
	try {
		state->container = std::make_unique<LiteHTMLContainer>(*base_dir, p_textures, p_fit_images, p_fonts, p_local_stylesheets);
		state->master_styles = litehtml::master_css;
		if (!p_default_stylesheet.is_empty()) {
			state->master_styles += '\n';
			state->master_styles += p_default_stylesheet.utf8().get_data();
		}
		const CharString stylesheet = p_stylesheet.utf8();
		state->user_styles.assign(stylesheet.get_data(), stylesheet.length());
		if (!p_html.strip_edges().is_empty() && !initialize_document(p_html.utf8())) {
			clear();
			state->error = "litehtml could not create the document.";
			return state->parse_error = ERR_PARSE_ERROR;
		}
	}
	catch (const std::exception& p_error) {
		clear();
		state->error = "Could not parse HTML. " + String::utf8(p_error.what());
		return state->parse_error = ERR_PARSE_ERROR;
	}
	return OK;
}

bool HTMLLayout::initialize_document(const CharString& p_html) {
	// Godot strings have already been decoded, regardless of an HTML meta charset.
	auto document = litehtml::document::createFromString(
		litehtml::estring(std::string(p_html.get_data(), p_html.length()), litehtml::encoding::utf_8),
		state->container.get(), state->master_styles, state->user_styles);
	if (!document || !document->root()) {
		return false;
	}
	state->document = std::move(document);
	return true;
}

Error HTMLLayout::layout(const Vector2i& p_viewport_size) {
	if (state->parse_error != OK) {
		return state->parse_error;
	}
	if (p_viewport_size.x <= 0 || p_viewport_size.y <= 0) {
		state->error = "The viewport width and height must be positive.";
		return ERR_INVALID_PARAMETER;
	}
	if (!state->container) {
		state->error = "Call parse() before laying out the document.";
		return ERR_UNCONFIGURED;
	}
	state->error = String();
	if (state->has_layout && state->viewport_size == p_viewport_size) {
		return OK;
	}
	state->has_layout = false;
	state->anchors.clear();
	state->render_items.clear();
	state->container->set_viewport(p_viewport_size);
	if (state->document && !state->implicit_empty_document) {
		try {
			if (state->styles_dirty || (state->edited && litehtml::docrender_editor::media_changed(*state->document))) {
				state->container->reset_stylesheet_reads();
				litehtml::docrender_editor::rebuild(*state->document, state->master_styles, state->user_styles);
				state->styles_dirty = false;
			}
			else if (state->edited || !state->document->media_changed()) {
				// Viewport units also change when all media queries keep matching.
				state->document->root()->compute_styles();
			}
			state->document->render(p_viewport_size.x);
			const double width = std::ceil(state->document->width());
			const double height = std::ceil(state->document->height());
			constexpr double MAX_SIZE = std::numeric_limits<int32_t>::max();
			if (!std::isfinite(width) || !std::isfinite(height) || width > MAX_SIZE || height > MAX_SIZE) {
				state->error = "The HTML content size is outside the supported range.";
				return ERR_INVALID_DATA;
			}
			const Vector2i size(static_cast<int32_t>(std::max(0.0, width)), static_cast<int32_t>(std::max(0.0, height)));
			state->container->record(*state->document, size);
			if (const auto root = state->document->root_render()) {
				index_render_items(root, state->render_items);
			}
			collect_anchors(*state->document->root(), state->render_items, state->anchors);
			state->content_size = size;
		}
		catch (const std::exception& p_error) {
			state->anchors.clear();
			state->render_items.clear();
			state->error = "Could not lay out HTML. " + String::utf8(p_error.what());
			return ERR_INVALID_DATA;
		}
	}
	state->viewport_size = p_viewport_size;
	state->has_layout = true;
	return OK;
}

Vector2i HTMLLayout::get_content_size() const {
	return state->content_size;
}

void HTMLLayout::draw(CanvasItem* p_canvas, const Vector2& p_position, const Rect2& p_clip) {
	draw_with_highlights(p_canvas, p_position, p_clip, {}, nullptr, Color());
}

String HTMLLayout::get_plain_text() const {
	return state->has_layout ? state->container->get_plain_text() : String();
}

int64_t HTMLLayout::hit_test_text(const Vector2& p_position) const {
	return state->has_layout ? state->container->hit_test_text(p_position) : 0;
}

TypedArray<Rect2> HTMLLayout::get_selection_rects(int64_t p_from, int64_t p_to) const {
	if (!state->has_layout) {
		return {};
	}
	const int64_t length = state->container->get_plain_text().length();
	const int64_t from = CLAMP(std::min(p_from, p_to), int64_t(0), length);
	const int64_t to = CLAMP(std::max(p_from, p_to), int64_t(0), length);
	return state->container->get_selection_rects(from, to);
}

void HTMLLayout::draw_with_highlights(CanvasItem* p_canvas, const Vector2& p_position, const Rect2& p_clip,
	const Vector2i& p_selection, const StyleBox* p_selection_style, const Color& p_selection_color,
	const Vector2i& p_search, const StyleBox* p_search_style) {
	if (!state->has_layout || !p_canvas || !p_canvas->is_inside_tree()) {
		return;
	}
	const auto& runs = state->container->get_paint_runs();
	if (runs.empty()) {
		return;
	}
	const uint64_t instance_id = p_canvas->get_instance_id();
	CanvasDrawing* drawing = state->drawings.getptr(instance_id);
	if (!drawing) {
		CanvasDrawing target;
		target.redraw = callable_mp(this, &HTMLLayout::_on_canvas_redraw).bind(instance_id);
		target.exiting = callable_mp(this, &HTMLLayout::_detach_canvas).bind(instance_id);
		// The draw signal precedes the script callback, including redraws that skip HTML.
		p_canvas->connect("draw", target.redraw);
		p_canvas->connect("tree_exiting", target.exiting);
		drawing = &state->drawings.insert(instance_id, std::move(target))->value;
	}

	RenderingServer& server = *RenderingServer::get_singleton();
	const LiteHTMLContainer::Highlight selection{ p_selection.x, p_selection.y, p_selection_style, p_selection_color };
	const LiteHTMLContainer::Highlight search{ p_search.x, p_search.y, p_search_style, Color(0, 0, 0, 0) };
	const Ref<TextServer> text_server = p_selection.x < p_selection.y || p_search.x < p_search.y
		? TextServerManager::get_singleton()->get_primary_interface() : Ref<TextServer>();
	RID target;
	std::optional<Rect2> previous_clip;
	Ref<ShaderMaterial> previous_material;
	for (const auto& run : runs) {
		std::optional<Rect2> clip;
		if (run.clip) {
			clip = Rect2(run.clip->position + p_position, run.clip->size);
		}
		if (p_clip != Rect2()) {
			clip = clip ? clip->intersection(p_clip) : p_clip;
		}
		if (clip && !clip->has_area()) {
			continue;
		}

		if (!target.is_valid() || clip != previous_clip || run.material != previous_material) {
			if (drawing->items.empty()) {
				drawing->add_item(server, p_canvas->get_canvas_item(), -1);
			}
			const RID root = drawing->items.front();
			const int32_t index = drawing->run_count++;
			if (clip) {
				const RID clip_item = drawing->add_item(server, root, index);
				server.canvas_item_set_custom_rect(clip_item, true, *clip);
				server.canvas_item_set_clip(clip_item, true);
				target = drawing->add_item(server, clip_item, 0);
			}
			else {
				target = drawing->add_item(server, root, index);
			}
			if (run.material.is_valid()) {
				server.canvas_item_set_material(target, run.material->get_rid());
				server.canvas_item_set_transform(target, Transform2D(0.0, p_position));
				drawing->materials.push_back(run.material);
			}
			previous_clip = clip;
			previous_material = run.material;
		}
		run.draw(server, target, run.material.is_valid() ? Vector2() : p_position, text_server.ptr(), search, selection);
	}
}

String HTMLLayout::get_link_at(const Vector2& p_position) const {
	return find_link_at(p_position).uri;
}

Rect2 HTMLLayout::get_anchor_rect(const String& p_id) const {
	const Rect2* bounds = state->anchors.getptr(p_id);
	return bounds ? *bounds : Rect2();
}

const HashMap<String, Rect2>& HTMLLayout::get_anchor_rects() const {
	return state->anchors;
}

Ref<HTMLElement> HTMLLayout::create_element(const String& p_tag_name) {
	CharString tag = p_tag_name.utf8();
	if (!state->container || state->parse_error != OK || !is_valid_tag_name(std::string_view(tag.get_data(), tag.length()))) {
		return {};
	}
	for (char* byte = tag.ptrw(); *byte; ++byte) {
		if (*byte >= 'A' && *byte <= 'Z') {
			*byte += 'a' - 'A';
		}
	}
	try {
		if (!state->document) {
			if (!initialize_document(String("<!doctype html>").utf8())) {
				return {};
			}
			state->implicit_empty_document = true;
		}
		const auto node = state->document->create_element(tag.get_data(), {});
		Ref<HTMLElement> handle = make_element(node);
		if (handle.is_valid()) {
			auto tree = std::make_shared<HTMLElement::DetachedTree>(ObjectID(get_instance_id()), parse_version, node);
			state->detached_trees.insert(node.get(), tree);
			handle->detached_tree = std::move(tree);
		}
		return handle;
	}
	catch (const std::exception&) {
		return {};
	}
}

std::shared_ptr<HTMLElement::DetachedTree> HTMLLayout::get_detached_tree(const litehtml::element& p_element) const {
	const auto* tree = state->detached_trees.getptr(tree_root(p_element));
	return tree ? tree->lock() : nullptr;
}

Ref<HTMLElement> HTMLLayout::make_element(const litehtml::element::ptr& p_element) const {
	if (!p_element || !is_html_element(*p_element)) {
		return {};
	}
	if (const ObjectID* id = state->element_handles.getptr(p_element.get())) {
		auto* existing = Object::cast_to<HTMLElement>(ObjectDB::get_instance(*id));
		if (existing && existing->element.lock() == p_element) {
			return Ref<HTMLElement>(existing);
		}
	}
	Ref<HTMLElement> handle;
	handle.instantiate();
	handle->layout_id = ObjectID(get_instance_id());
	handle->parse_version = parse_version;
	handle->element = p_element;
	handle->detached_tree = get_detached_tree(*p_element);
	state->element_handles.insert(p_element.get(), ObjectID(handle->get_instance_id()));
	return handle;
}

Dictionary HTMLLayout::get_element_attributes(const litehtml::element& p_element) const {
	Dictionary result;
	if (const auto* attributes = state->container->get_source_attributes(p_element)) {
		for (const auto& [name, value] : *attributes) {
			result[String::utf8(name.c_str())] = String::utf8(value.c_str());
		}
	}
	return result;
}

const char* HTMLLayout::get_element_attribute(const litehtml::element& p_element, const char* p_name) const {
	if (const auto* attributes = state->container->get_source_attributes(p_element)) {
		const auto found = attributes->find(p_name);
		if (found != attributes->end()) {
			return found->second.c_str();
		}
	}
	return p_element.get_attr(p_name);
}

Error HTMLLayout::edit_attribute(const litehtml::element::ptr& p_element, const char* p_name, const char* p_value) {
	auto* tag = dynamic_cast<litehtml::html_tag*>(p_element.get());
	if (!tag) {
		return ERR_UNAVAILABLE;
	}
	const auto* source = state->container->get_source_attributes(*tag);
	const char* previous = nullptr;
	if (source) {
		const auto found = source->find(p_name);
		if (found != source->end()) {
			previous = found->second.c_str();
		}
	}
	if ((!previous && !p_value) || (previous && p_value && std::string_view(previous) == p_value)) {
		return OK;
	}
	if (p_value) {
		tag->set_attr(p_name, p_value);
	}
	else {
		litehtml::docrender_editor::remove_attribute(*tag, p_name);
	}
	state->container->set_source_attribute(p_element, p_name, p_value);
	if (tag->tag() == litehtml::_a_ && std::string_view(p_name) == "href") {
		litehtml::docrender_editor::reset_link(*tag, p_value != nullptr);
	}
	if (!get_detached_tree(*p_element)) {
		invalidate_layout();
	}
	return OK;
}

String HTMLLayout::serialize_children(const litehtml::element& p_element) const {
	std::string html;
	const auto append_children = [&](const auto& p_append_children, const litehtml::element& p_parent) -> void {
		for (const auto& child : p_parent.children()) {
			if (child->tag() == litehtml::__tag_before_ || child->tag() == litehtml::__tag_after_) {
				continue;
			}
			if (is_html_element(*child)) {
				const char* tag = child->get_tagName();
				html += '<';
				html += tag;
				if (const auto* attributes = state->container->get_source_attributes(*child)) {
					for (const auto& [name, value] : *attributes) {
						html += ' ';
						html += name;
						html += "=\"";
						append_escaped(html, value, true);
						html += '"';
					}
				}
				html += '>';
				if (!is_void_element(*child)) {
					p_append_children(p_append_children, *child);
					html += "</";
					html += tag;
					html += '>';
				}
			}
			else {
				std::string text;
				child->get_text(text);
				if (child->is_comment()) {
					html += "<!--" + text + "-->";
				}
				else if (has_raw_text(p_parent)) {
					html += text;
				}
				else {
					append_escaped(html, text, false);
				}
			}
		}
	};
	append_children(append_children, p_element);
	return String::utf8(html.data(), html.size());
}

Error HTMLLayout::replace_children(const litehtml::element::ptr& p_element, const String& p_content, bool p_parse_html) {
	if (is_void_element(*p_element)) {
		return p_content.is_empty() ? OK : ERR_INVALID_PARAMETER;
	}
	const CharString content = p_content.utf8();
	if (std::string_view(content.get_data(), content.length()).find('\0') != std::string_view::npos) {
		return ERR_INVALID_PARAMETER;
	}
	litehtml::elements_list children;
	try {
		const bool split = !has_raw_text(*p_element);
		children = p_parse_html ? litehtml::docrender_editor::parse_fragment(*state->document, *p_element, content) :
			litehtml::docrender_editor::create_text(state->document, content.get_data(), split);
	}
	catch (const std::exception&) {
		return ERR_PARSE_ERROR;
	}
	for (const auto& child : p_element->children()) {
		forget_subtree(*child);
	}
	litehtml::docrender_editor::replace_children(*p_element, std::move(children));
	if (!get_detached_tree(*p_element)) {
		invalidate_layout();
	}
	return OK;
}

Error HTMLLayout::move_element(const litehtml::element::ptr& p_element, const litehtml::element::ptr& p_parent) {
	const auto old_parent = p_element->parent();
	const auto old_tree = get_detached_tree(*p_element);
	if (!old_parent && !old_tree) {
		return ERR_INVALID_PARAMETER;
	}
	bool needs_layout = !old_tree;
	if (p_parent) {
		auto* from = dynamic_cast<litehtml::html_tag*>(old_parent.get());
		auto* to = dynamic_cast<litehtml::html_tag*>(p_parent.get());
		if ((old_parent && !from) || !to || !can_contain_elements(*to)) {
			return ERR_INVALID_PARAMETER;
		}
		for (auto ancestor = p_parent; ancestor; ancestor = ancestor->parent()) {
			if (ancestor == p_element) {
				return ERR_INVALID_PARAMETER;
			}
		}
		const auto new_tree = get_detached_tree(*p_parent);
		if (from) {
			litehtml::docrender_editor::move(*from, *to, p_element);
		}
		else if (!to->appendChild(p_element)) {
			return ERR_INVALID_PARAMETER;
		}
		if (old_tree != new_tree) {
			if (old_tree && old_tree->root == p_element) {
				state->detached_trees.erase(p_element.get());
				old_tree->root.reset();
			}
			visit_elements(p_element, [&](const litehtml::element::ptr& p_child) {
				if (const ObjectID* id = state->element_handles.getptr(p_child.get())) {
					if (auto* handle = Object::cast_to<HTMLElement>(ObjectDB::get_instance(*id))) {
						handle->detached_tree = new_tree;
					}
				}
				return false;
			});
		}
		needs_layout |= !new_tree;
	}
	else {
		if (!old_parent) {
			return OK;
		}
		if (!old_parent->removeChild(p_element)) {
			return ERR_INVALID_PARAMETER;
		}
		forget_subtree(*p_element);
	}
	if (needs_layout) {
		invalidate_layout();
	}
	return OK;
}

void HTMLLayout::forget_subtree(const litehtml::element& p_element) {
	if (const ObjectID* id = state->element_handles.getptr(&p_element)) {
		if (auto* handle = Object::cast_to<HTMLElement>(ObjectDB::get_instance(*id))) {
			handle->element.reset();
			handle->detached_tree.reset();
		}
		state->element_handles.erase(&p_element);
	}
	state->container->forget_source_attributes(p_element);
	for (const auto& child : p_element.children()) {
		forget_subtree(*child);
	}
}

void HTMLLayout::set_edit_callback(const Callable& p_callback) {
	state->edit_callback = p_callback;
}

void HTMLLayout::invalidate_layout() {
	state->has_layout = false;
	state->implicit_empty_document = false;
	state->styles_dirty = true;
	state->edited = true;
	state->anchors.clear();
	if (state->edit_callback.is_valid()) {
		state->edit_callback.call();
	}
	else if (state->viewport_size != Vector2i() && !state->reflow_queued) {
		state->reflow_queued = true;
		callable_mp_static(&HTMLLayout::reflow_after_edit).call_deferred(get_instance_id());
	}
}

void HTMLLayout::reflow_after_edit(uint64_t p_id) {
	auto* owner = Object::cast_to<HTMLLayout>(ObjectDB::get_instance(ObjectID(p_id)));
	if (!owner) {
		return;
	}
	State& current = *owner->state;
	current.reflow_queued = false;
	if (!current.has_layout && current.viewport_size != Vector2i()) {
		owner->layout(current.viewport_size);
	}
	for (const auto& drawing : current.drawings) {
		if (auto* canvas = Object::cast_to<CanvasItem>(ObjectDB::get_instance(ObjectID(drawing.key)))) {
			canvas->queue_redraw();
		}
	}
}

TypedArray<HTMLElement> HTMLLayout::select_elements(const String& p_selector, SelectorQuery p_query,
	const litehtml::element::ptr& p_scope) const {
	TypedArray<HTMLElement> matches;
	const CharString utf8 = p_selector.utf8();
	const auto tokens = litehtml::tokenize(std::string(utf8.get_data(), utf8.length()));
	ERR_FAIL_COND_V_MSG(!has_balanced_selector_tokens(tokens), matches, "Invalid HTML selector: " + p_selector);
	const auto selectors = litehtml::parse_selector_list(litehtml::normalize(tokens, litehtml::f_componentize),
		litehtml::strict_mode | litehtml::forbid_pseudo_elements,
		state->document ? state->document->mode() : litehtml::no_quirks_mode);
	ERR_FAIL_COND_V_MSG(selectors.empty(), matches, "Invalid or unsupported HTML selector: " + p_selector);
	if (!state->document) {
		return matches;
	}
	const auto root = p_scope ? p_scope : state->document->root();
	const SelectorScope scope(root.get());
	const auto collect = [&](const litehtml::element::ptr& p_element) {
		if (p_element->select(selectors) == litehtml::select_no_match) {
			return false;
		}
		matches.append(make_element(p_element));
		return p_query != SelectorQuery::ALL;
	};

	if (p_query == SelectorQuery::SELF || p_query == SelectorQuery::CLOSEST) {
		for (auto element = root; element; element = element->parent()) {
			if ((is_html_element(*element) && collect(element)) || p_query == SelectorQuery::SELF) {
				break;
			}
		}
	}
	else if (p_scope) {
		for (const auto& child : p_scope->children()) {
			if (visit_elements(child, collect)) {
				break;
			}
		}
	}
	else {
		visit_elements(root, collect);
	}
	return matches;
}

Ref<HTMLElement> HTMLLayout::query_selector(const String& p_selector) const {
	const auto matches = select_elements(p_selector, SelectorQuery::FIRST);
	return matches.is_empty() ? Ref<HTMLElement>() : Ref<HTMLElement>(matches[0]);
}

TypedArray<HTMLElement> HTMLLayout::query_selector_all(const String& p_selector) const {
	return select_elements(p_selector, SelectorQuery::ALL);
}

Ref<HTMLElement> HTMLLayout::get_element_by_id(const String& p_id) const {
	if (!state->document || p_id.is_empty()) {
		return {};
	}
	const CharString id = p_id.utf8();
	litehtml::element::ptr match;
	visit_elements(state->document->root(), [&](const litehtml::element::ptr& p_element) {
		const char* candidate = get_element_attribute(*p_element, "id");
		if (candidate && std::string_view(candidate) == std::string_view(id.get_data(), id.length())) {
			match = p_element;
		}
		return match != nullptr;
	});
	return make_element(match);
}

TypedArray<Rect2> HTMLLayout::get_element_rects(const litehtml::element::ptr& p_element) const {
	TypedArray<Rect2> result;
	if (const auto* items = state->render_items.getptr(p_element.get()); state->has_layout && items) {
		for (const auto& item : *items) {
			if (!item->skip()) {
				visit_boxes(*item, false, [&](const Rect2& p_rect) { result.append(p_rect); });
			}
		}
	}
	return result;
}

Ref<HTMLElement> HTMLLayout::get_element_at(const Vector2& p_position) const {
	return wrap_hit_element(hit_test_element(p_position));
}

Ref<HTMLElement> HTMLLayout::wrap_hit_element(litehtml::element::ptr p_element) const {
	while (p_element && !is_html_element(*p_element)) {
		p_element = p_element->parent();
	}
	return make_element(p_element);
}

litehtml::element::ptr HTMLLayout::hit_test_element(const Vector2& p_position) const {
	const Rect2 bounds(Vector2(), Vector2(state->content_size.max(state->viewport_size)));
	if (!state->has_layout || !state->document || !p_position.is_finite() || !bounds.has_point(p_position)) {
		return {};
	}
	const auto root = state->document->root_render();
	if (!root) {
		return {};
	}
	const auto accepts = [&](const std::shared_ptr<litehtml::render_item>& p_item) {
		// Some litehtml fallback paths offer another fragment of the same element.
		const auto* current = state->render_items.getptr(p_item->src_el().get());
		return current && std::find(current->begin(), current->end(), p_item) != current->end() &&
			p_item->is_visible() && LiteHTMLContainer::is_hit_visible(*p_item, p_position);
	};
	struct SkippedItems {
		RenderItems items;
		~SkippedItems() {
			for (const auto& item : items) {
				item->skip(false);
			}
		}
	};
	SkippedItems skipped;
	for (;;) {
		auto element = root->get_element_by_point(p_position.x, p_position.y, p_position.x, p_position.y, accepts);
		if (!element) {
			return {};
		}
		const auto* items = state->render_items.getptr(element.get());
		if (!items) {
			return {};
		}
		if (std::any_of(items->begin(), items->end(), accepts)) {
			return element;
		}

		// litehtml's float fallback bypasses the filter. Exclude its rejected boxes and retry in the same order.
		const size_t previous_size = skipped.items.size();
		for (const auto& item : *items) {
			if (item->is_visible()) {
				skipped.items.push_back(item);
				item->skip(true);
			}
		}
		if (skipped.items.size() == previous_size) {
			return {};
		}
	}
}

std::optional<Rect2> HTMLLayout::get_element_scroll_bounds(const Ref<HTMLElement>& p_element) const {
	if (!state->has_layout || p_element.is_null() || p_element->get_owner() != this) {
		return std::nullopt;
	}
	const auto element = p_element->element.lock();
	const auto* items = state->render_items.getptr(element.get());
	std::optional<Rect2> bounds;
	if (items) {
		for (const auto& item : *items) {
			if (item->is_visible()) {
				visit_boxes(*item, false, [&](const Rect2& p_rect) {
					if (LiteHTMLContainer::has_visible_area(*item, p_rect)) {
						bounds = bounds ? bounds->merge(p_rect) : p_rect;
					}
				});
			}
		}
	}
	return bounds;
}

HTMLLayout::LinkHit HTMLLayout::find_link_at(const Vector2& p_position, Ref<HTMLElement>* r_element) const {
	auto element = hit_test_element(p_position);
	if (r_element) {
		*r_element = wrap_hit_element(element);
	}
	while (element) {
		if (element->tag() == litehtml::_a_) {
			const char* href = element->get_attr("href");
			return href && *href ? LinkHit{ String::utf8(href), reinterpret_cast<uintptr_t>(element.get()) } : LinkHit{};
		}
		element = element->parent();
	}
	return {};
}

String HTMLLayout::get_render_error() const {
	return state->error;
}

void HTMLLayout::clear() {
	++parse_version;
	while (!state->drawings.is_empty()) {
		_detach_canvas(state->drawings.begin()->key);
	}
	state->render_items.clear();
	state->element_handles.clear();
	for (const auto& entry : state->detached_trees) {
		if (const auto tree = entry.value.lock()) {
			tree->root.reset();
		}
	}
	state->detached_trees.clear();
	state->document.reset();
	state->container.reset();
	state->anchors.clear();
	state->viewport_size = Vector2i();
	state->content_size = Vector2i();
	state->error = String();
	state->parse_error = OK;
	state->has_layout = false;
	state->implicit_empty_document = false;
	state->styles_dirty = false;
	state->edited = false;
	state->master_styles.clear();
	state->user_styles.clear();
}

void HTMLLayout::_on_canvas_redraw(uint64_t p_id) {
	if (CanvasDrawing* drawing = state->drawings.getptr(p_id)) {
		drawing->clear();
	}
}

void HTMLLayout::_detach_canvas(uint64_t p_id) {
	CanvasDrawing* drawing = state->drawings.getptr(p_id);
	if (!drawing) {
		return;
	}
	if (CanvasItem* canvas = Object::cast_to<CanvasItem>(ObjectDB::get_instance(ObjectID(p_id)))) {
		canvas->disconnect("draw", drawing->redraw);
		canvas->disconnect("tree_exiting", drawing->exiting);
	}
	drawing->clear();
	state->drawings.erase(p_id);
}

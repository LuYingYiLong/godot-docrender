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
#include <litehtml/css_parser.h>
#include <litehtml/css_selector.h>
#include <litehtml/document.h>
#include <litehtml/render_item.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <string_view>

using namespace godot;
using docrender::LiteHTMLContainer;

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

} // namespace

struct HTMLLayout::State {
	// Render items are cleared before the document releases its fonts through the container.
	std::unique_ptr<LiteHTMLContainer> container;
	litehtml::document::ptr document;
	HashMap<uint64_t, CanvasDrawing> drawings;
	RenderIndex render_items;
	HashMap<String, Rect2> anchors;
	Vector2i viewport_size;
	Vector2i content_size;
	String error;
	Error parse_error = OK;
	bool has_layout = false;
};

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
		if (!p_html.strip_edges().is_empty()) {
			const CharString html = p_html.utf8();
			const CharString stylesheet = p_stylesheet.utf8();
			std::string master_styles = litehtml::master_css;
			if (!p_default_stylesheet.is_empty()) {
				master_styles += '\n';
				master_styles += p_default_stylesheet.utf8().get_data();
			}
			// Godot strings have already been decoded, regardless of an HTML meta charset.
			state->document = litehtml::document::createFromString(
				litehtml::estring(std::string(html.get_data(), html.length()), litehtml::encoding::utf_8),
				state->container.get(), master_styles, stylesheet.get_data());
			if (!state->document || !state->document->root()) {
				clear();
				state->error = "litehtml could not create the document.";
				return state->parse_error = ERR_PARSE_ERROR;
			}
		}
	}
	catch (const std::exception& p_error) {
		clear();
		state->error = "Could not parse HTML. " + String::utf8(p_error.what());
		return state->parse_error = ERR_PARSE_ERROR;
	}
	return OK;
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
	if (state->document) {
		try {
			if (!state->document->media_changed()) {
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

Ref<HTMLElement> HTMLLayout::make_element(const litehtml::element::ptr& p_element) const {
	if (!p_element || !is_html_element(*p_element)) {
		return {};
	}
	Ref<HTMLElement> handle;
	handle.instantiate();
	handle->layout_id = ObjectID(get_instance_id());
	handle->parse_version = parse_version;
	handle->element = p_element;
	return handle;
}

TypedArray<HTMLElement> HTMLLayout::select_elements(const String& p_selector, bool p_first_only) const {
	TypedArray<HTMLElement> matches;
	const CharString utf8 = p_selector.utf8();
	const auto tokens = litehtml::tokenize(std::string(utf8.get_data(), utf8.length()));
	ERR_FAIL_COND_V_MSG(!has_balanced_selector_tokens(tokens), matches, "Invalid HTML selector: " + p_selector);
	const auto selectors = litehtml::parse_selector_list(litehtml::normalize(tokens, litehtml::f_componentize),
		litehtml::strict_mode | litehtml::forbid_pseudo_elements,
		state->document ? state->document->mode() : litehtml::no_quirks_mode);
	ERR_FAIL_COND_V_MSG(selectors.empty(), matches, "Invalid or unsupported HTML selector: " + p_selector);
	if (state->document) {
		visit_elements(state->document->root(), [&](const litehtml::element::ptr& p_element) {
			if (p_element->select(selectors) == litehtml::select_no_match) {
				return false;
			}
			matches.append(make_element(p_element));
			return p_first_only;
		});
	}
	return matches;
}

Ref<HTMLElement> HTMLLayout::query_selector(const String& p_selector) const {
	const auto matches = select_elements(p_selector, true);
	return matches.is_empty() ? Ref<HTMLElement>() : Ref<HTMLElement>(matches[0]);
}

TypedArray<HTMLElement> HTMLLayout::query_selector_all(const String& p_selector) const {
	return select_elements(p_selector, false);
}

Ref<HTMLElement> HTMLLayout::get_element_by_id(const String& p_id) const {
	if (!state->document || p_id.is_empty()) {
		return {};
	}
	const CharString id = p_id.utf8();
	litehtml::element::ptr match;
	visit_elements(state->document->root(), [&](const litehtml::element::ptr& p_element) {
		const char* candidate = p_element->get_attr("id");
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
	auto element = hit_test_element(p_position);
	while (element && !is_html_element(*element)) {
		element = element->parent();
	}
	return make_element(element);
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

HTMLLayout::LinkHit HTMLLayout::find_link_at(const Vector2& p_position) const {
	auto element = hit_test_element(p_position);
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
	state->document.reset();
	state->container.reset();
	state->anchors.clear();
	state->viewport_size = Vector2i();
	state->content_size = Vector2i();
	state->error = String();
	state->parse_error = OK;
	state->has_layout = false;
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

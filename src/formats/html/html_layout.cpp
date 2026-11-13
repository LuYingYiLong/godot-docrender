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
#include <litehtml/document.h>
#include <litehtml/render_item.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>

using namespace godot;
using docrender::LiteHTMLContainer;

namespace {
	struct CanvasDrawing {
		Callable redraw;
		Callable exiting;
		std::vector<RID> items;
		int32_t run_count = 0;

		void clear() {
			if (RenderingServer* server = RenderingServer::get_singleton()) {
				for (auto item = items.rbegin(); item != items.rend(); ++item) {
					server->free_rid(*item);
				}
			}
			items.clear();
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

	void collect_anchors(litehtml::element& p_element, HashMap<String, Rect2>& r_anchors) {
		if (p_element.css().get_display() == litehtml::display_none) {
			return;
		}
		const char* id = p_element.get_attr("id");
		if (id && *id) {
			const String name = String::utf8(id);
			if (!r_anchors.has(name)) {
				std::optional<Rect2> bounds;
				// A media change can replace render items while leaving their old weak references in the DOM.
				p_element.run_on_renderers([&](const std::shared_ptr<litehtml::render_item>& p_item) {
					if (!p_item->is_visible()) {
						return true;
					}
					litehtml::position::vector boxes;
					p_item->get_rendering_boxes(boxes);
					if (boxes.empty()) {
						boxes.push_back(p_item->get_placement());
					}
					for (const auto& box : boxes) {
						const Rect2 rect(box.x, box.y, box.width, box.height);
						if (rect.position.is_finite() && rect.get_end().is_finite()) {
							bounds = bounds ? bounds->merge(rect) : rect;
						}
					}
					return true;
				});
				if (bounds) {
					r_anchors.insert(name, *bounds);
				}
			}
		}
		for (const auto& child : p_element.children()) {
			collect_anchors(*child, r_anchors);
		}
	}

} // namespace

struct HTMLLayout::State {
	// The document releases its fonts through the container when destroyed.
	std::unique_ptr<LiteHTMLContainer> container;
	litehtml::document::ptr document;
	HashMap<uint64_t, CanvasDrawing> drawings;
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
	const Dictionary& p_textures, bool p_fit_images, const String& p_default_stylesheet, const Fonts& p_fonts) {
	clear();
	const std::optional<String> base_dir = docrender::normalize_html_resource_path(p_base_dir);
	if (!base_dir) {
		state->error = "The base directory must be a project resource path (res://).";
		return state->parse_error = ERR_INVALID_PARAMETER;
	}
	try {
		state->container = std::make_unique<LiteHTMLContainer>(*base_dir, p_textures, p_fit_images, p_fonts);
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
			collect_anchors(*state->document->root(), state->anchors);
			state->content_size = size;
		}
		catch (const std::exception& p_error) {
			state->anchors.clear();
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

		if (!target.is_valid() || clip != previous_clip) {
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
			previous_clip = clip;
		}
		run.draw(server, target, p_position, text_server.ptr(), search, selection);
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

HTMLLayout::LinkHit HTMLLayout::find_link_at(const Vector2& p_position) const {
	if (!state->has_layout || !state->document) {
		return {};
	}
	const auto root = state->document->root_render();
	if (!root) {
		return {};
	}
	auto element = root->get_element_by_point(p_position.x, p_position.y, p_position.x, p_position.y, nullptr);
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
	while (!state->drawings.is_empty()) {
		_detach_canvas(state->drawings.begin()->key);
	}
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

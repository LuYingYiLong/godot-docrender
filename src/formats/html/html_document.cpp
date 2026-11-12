#include "html_document.h"

#include "litehtml_container.h"

#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
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

} // namespace

struct HTMLDocument::State {
	// The document releases its fonts through the container when destroyed.
	std::unique_ptr<LiteHTMLContainer> container;
	litehtml::document::ptr document;
	HashMap<uint64_t, CanvasDrawing> drawings;
	Vector2i viewport_size;
	Vector2i content_size;
	String error;
	bool has_layout = false;
};

void HTMLDocument::_bind_methods() {
	ClassDB::bind_method(D_METHOD("parse", "html", "base_dir", "stylesheet"), &HTMLDocument::parse,
		DEFVAL(String("res://")), DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("layout", "viewport_size"), &HTMLDocument::layout);
	ClassDB::bind_method(D_METHOD("get_content_size"), &HTMLDocument::get_content_size);
	ClassDB::bind_method(D_METHOD("draw", "canvas", "position", "clip"), &HTMLDocument::draw,
		DEFVAL(Vector2()), DEFVAL(Rect2()));
	ClassDB::bind_method(D_METHOD("get_link_at", "position"), &HTMLDocument::get_link_at);
	ClassDB::bind_method(D_METHOD("get_render_error"), &HTMLDocument::get_render_error);
	ClassDB::bind_method(D_METHOD("clear"), &HTMLDocument::clear);
}

HTMLDocument::HTMLDocument() :
	state(std::make_unique<State>()) {
}

HTMLDocument::~HTMLDocument() {
	clear();
}

Error HTMLDocument::parse(const String& p_html, const String& p_base_dir, const String& p_stylesheet) {
	clear();
	const std::optional<String> base_dir = LiteHTMLContainer::normalize_resource_path(p_base_dir);
	if (!base_dir) {
		state->error = "The base directory must be a project resource path (res://).";
		return ERR_INVALID_PARAMETER;
	}
	try {
		state->container = std::make_unique<LiteHTMLContainer>(*base_dir);
		if (!p_html.strip_edges().is_empty()) {
			const CharString html = p_html.utf8();
			const CharString stylesheet = p_stylesheet.utf8();
			// Godot strings have already been decoded, regardless of an HTML meta charset.
			state->document = litehtml::document::createFromString(
				litehtml::estring(std::string(html.get_data(), html.length()), litehtml::encoding::utf_8),
				state->container.get(), litehtml::master_css, stylesheet.get_data());
			if (!state->document || !state->document->root()) {
				clear();
				state->error = "litehtml could not create the document.";
				return ERR_PARSE_ERROR;
			}
		}
	}
	catch (const std::exception& p_error) {
		clear();
		state->error = "Could not parse HTML. " + String::utf8(p_error.what());
		return ERR_PARSE_ERROR;
	}
	return OK;
}

Error HTMLDocument::layout(const Vector2i& p_viewport_size) {
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
			state->content_size = size;
		}
		catch (const std::exception& p_error) {
			state->error = "Could not lay out HTML. " + String::utf8(p_error.what());
			return ERR_INVALID_DATA;
		}
	}
	state->viewport_size = p_viewport_size;
	state->has_layout = true;
	return OK;
}

Vector2i HTMLDocument::get_content_size() const {
	return state->content_size;
}

void HTMLDocument::draw(CanvasItem* p_canvas, const Vector2& p_position, const Rect2& p_clip) {
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
		target.redraw = callable_mp(this, &HTMLDocument::_on_canvas_redraw).bind(instance_id);
		target.exiting = callable_mp(this, &HTMLDocument::_detach_canvas).bind(instance_id);
		// The draw signal precedes the script callback, including redraws that skip HTML.
		p_canvas->connect("draw", target.redraw);
		p_canvas->connect("tree_exiting", target.exiting);
		drawing = &state->drawings.insert(instance_id, std::move(target))->value;
	}

	RenderingServer& server = *RenderingServer::get_singleton();
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
		run.draw(server, target, p_position);
	}
}

String HTMLDocument::get_link_at(const Vector2& p_position) const {
	return find_link_at(p_position).uri;
}

HTMLDocument::LinkHit HTMLDocument::find_link_at(const Vector2& p_position) const {
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

String HTMLDocument::get_render_error() const {
	return state->error;
}

void HTMLDocument::clear() {
	while (!state->drawings.is_empty()) {
		_detach_canvas(state->drawings.begin()->key);
	}
	state->document.reset();
	state->container.reset();
	state->viewport_size = Vector2i();
	state->content_size = Vector2i();
	state->error = String();
	state->has_layout = false;
}

void HTMLDocument::_on_canvas_redraw(uint64_t p_id) {
	if (CanvasDrawing* drawing = state->drawings.getptr(p_id)) {
		drawing->clear();
	}
}

void HTMLDocument::_detach_canvas(uint64_t p_id) {
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

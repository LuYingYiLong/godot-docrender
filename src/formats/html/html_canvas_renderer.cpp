#include "html_canvas_renderer.h"

#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>

#include <algorithm>
#include <cmath>
#include <type_traits>

using namespace godot;

namespace docrender {
	void HTMLPaintRun::draw(RenderingServer& p_server, const RID& p_canvas, const Vector2& p_offset,
		const TextServer* p_text_server, const HTMLHighlight& p_search, const HTMLHighlight& p_selection) const {
		for (const HTMLPaintCommand& command : commands) {
			std::visit([&](const auto& p_draw) {
				using Draw = std::decay_t<decltype(p_draw)>;
				if constexpr (std::is_same_v<Draw, HTMLFill>) {
					const Rect2 rect = translate_rect(p_draw.rect, p_offset);
					p_server.canvas_item_add_rect(p_canvas, rect, p_draw.color);
				}
				else if constexpr (std::is_same_v<Draw, HTMLText>) {
					if (p_text_server && p_draw.offset >= 0) {
						for (const HTMLHighlight* highlight : { &p_search, &p_selection }) {
							const int64_t from = std::max<int64_t>(0, highlight->from - p_draw.offset);
							const int64_t to = std::min(p_draw.length, highlight->to - p_draw.offset);
							if (from < to) {
								const auto ranges = p_text_server->shaped_text_get_selection(p_draw.line->get_rid(), from, to);
								for (int64_t i = 0; i < ranges.size(); ++i) {
									const Vector2 range = ranges[i];
									const Rect2 rect = translate_rect(Rect2(range.x, 0, range.y - range.x, p_draw.line->get_size().y),
										p_draw.position + p_offset);
									if (highlight->style) {
										highlight->style->draw(p_canvas, rect);
									} else {
										p_server.canvas_item_add_rect(p_canvas, rect, highlight->color);
									}
								}
							}
						}
					}
					p_draw.line->draw(p_canvas, p_draw.position + p_offset, p_draw.color);
				}
				else if constexpr (std::is_same_v<Draw, HTMLImage>) {
					const Rect2 rect = translate_rect(p_draw.destination, p_offset);
					if (p_draw.repeat) {
						p_server.canvas_item_set_default_texture_repeat(p_canvas, RenderingServer::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED);
					}
					p_draw.texture->draw_rect_region(p_canvas, rect, p_draw.source, Color(1, 1, 1), false, !p_draw.repeat);
				}
				else if constexpr (std::is_same_v<Draw, HTMLCircle>) {
					const Vector2 center = p_draw.center + p_offset;
					if (p_draw.filled) {
						p_server.canvas_item_add_circle(p_canvas, center, p_draw.radius, p_draw.color, true);
					}
					else {
						PackedVector2Array points;
						constexpr int SEGMENTS = 20;
						for (int index = 0; index <= SEGMENTS; ++index) {
							const float angle = static_cast<float>(Math_TAU) * index / SEGMENTS;
							points.push_back(center + Vector2(std::cos(angle), std::sin(angle)) * p_draw.radius);
						}
						p_server.canvas_item_add_polyline(p_canvas, points, PackedColorArray({ p_draw.color }), 1.0f, true);
					}
				}
			}, command);
		}
	}

	void HTMLCanvasRenderer::CanvasDrawing::clear() {
		if (RenderingServer* server = RenderingServer::get_singleton()) {
			for (auto item = items.rbegin(); item != items.rend(); ++item) {
				server->free_rid(*item);
			}
		}
		items.clear();
		materials.clear();
		run_count = 0;
	}

	RID HTMLCanvasRenderer::CanvasDrawing::add_item(RenderingServer& p_server, const RID& p_parent, int32_t p_draw_index) {
		const RID item = p_server.canvas_item_create();
		p_server.canvas_item_set_parent(item, p_parent);
		p_server.canvas_item_set_draw_index(item, p_draw_index);
		items.push_back(item);
		return item;
	}

	HTMLCanvasRenderer::~HTMLCanvasRenderer() {
		while (!drawings.is_empty()) {
			detach_canvas(drawings.begin()->key);
		}
	}

	void HTMLCanvasRenderer::submit(CanvasItem& p_canvas, const Vector2& p_position, const Rect2& p_clip,
		const std::vector<HTMLPaintRun>& p_runs, const std::vector<HTMLPaintRun>& p_diagnostics,
		const HTMLHighlight& p_selection, const HTMLHighlight& p_search) {
		const uint64_t instance_id = p_canvas.get_instance_id();
		CanvasDrawing* drawing = drawings.getptr(instance_id);
		if (!drawing) {
			CanvasDrawing target;
			target.redraw = callable_mp(this, &HTMLCanvasRenderer::on_canvas_redraw).bind(instance_id);
			target.exiting = callable_mp(this, &HTMLCanvasRenderer::detach_canvas).bind(instance_id);
			p_canvas.connect("draw", target.redraw);
			p_canvas.connect("tree_exiting", target.exiting);
			drawing = &drawings.insert(instance_id, std::move(target))->value;
		}
		if (!drawing->submitted) {
			// Replace the previous submission once, then append further submissions in this draw.
			drawing->clear();
			drawing->submitted = true;
			on_canvas_redraw(instance_id);
		}

		RenderingServer& server = *RenderingServer::get_singleton();
		const Ref<TextServer> text_server = p_selection.from < p_selection.to || p_search.from < p_search.to
			? TextServerManager::get_singleton()->get_primary_interface() : Ref<TextServer>();
		RID target;
		std::optional<Rect2> previous_clip;
		Ref<ShaderMaterial> previous_material;
		Transform2D previous_matrix;
		const auto submit_run = [&](const HTMLPaintRun& p_run) {
			std::optional<Rect2> clip;
			if (p_run.clip) {
				clip = translate_rect(*p_run.clip, p_position);
			}
			if (p_clip != Rect2()) {
				clip = clip ? clip->intersection(p_clip) : p_clip;
			}
			if (clip && !clip->has_area()) {
				return;
			}

			const bool transformed = p_run.material.is_valid() || p_run.matrix != Transform2D();
			if (!target.is_valid() || clip != previous_clip || p_run.material != previous_material || p_run.matrix != previous_matrix) {
				if (drawing->items.empty()) {
					drawing->add_item(server, p_canvas.get_canvas_item(), -1);
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
				if (p_run.material.is_valid()) {
					server.canvas_item_set_material(target, p_run.material->get_rid());
					drawing->materials.push_back(p_run.material);
				}
				if (transformed) {
					server.canvas_item_set_transform(target, Transform2D(0.0, p_position) * p_run.matrix);
				}
				previous_clip = clip;
				previous_material = p_run.material;
				previous_matrix = p_run.matrix;
			}
			p_run.draw(server, target, transformed ? Vector2() : p_position, text_server.ptr(), p_search, p_selection);
		};
		for (const auto& run : p_runs) {
			submit_run(run);
		}
		target = RID();
		for (const auto& run : p_diagnostics) {
			submit_run(run);
		}
	}

	void HTMLCanvasRenderer::queue_redraws() const {
		for (const auto& drawing : drawings) {
			if (auto* canvas = Object::cast_to<CanvasItem>(ObjectDB::get_instance(ObjectID(drawing.key)))) {
				canvas->queue_redraw();
			}
		}
	}

	void HTMLCanvasRenderer::on_canvas_redraw(uint64_t p_canvas_id) {
		CanvasDrawing* drawing = drawings.getptr(p_canvas_id);
		if (!drawing || drawing->redraw_id != 0) {
			return;
		}
		// Another draw listener may have already submitted HTML before this signal arrives.
		drawing->redraw_id = ++next_redraw_id;
		callable_mp_static(&HTMLCanvasRenderer::finish_canvas_redraw).call_deferred(get_instance_id(), p_canvas_id, drawing->redraw_id);
	}

	void HTMLCanvasRenderer::finish_canvas_redraw(uint64_t p_renderer_id, uint64_t p_canvas_id, uint64_t p_redraw_id) {
		auto* renderer = Object::cast_to<HTMLCanvasRenderer>(ObjectDB::get_instance(ObjectID(p_renderer_id)));
		if (!renderer) {
			return;
		}
		CanvasDrawing* drawing = renderer->drawings.getptr(p_canvas_id);
		if (!drawing || drawing->redraw_id != p_redraw_id) {
			return;
		}
		if (!drawing->submitted) {
			drawing->clear();
		}
		drawing->submitted = false;
		drawing->redraw_id = 0;
	}

	void HTMLCanvasRenderer::detach_canvas(uint64_t p_canvas_id) {
		CanvasDrawing* drawing = drawings.getptr(p_canvas_id);
		if (!drawing) {
			return;
		}
		if (auto* canvas = Object::cast_to<CanvasItem>(ObjectDB::get_instance(ObjectID(p_canvas_id)))) {
			canvas->disconnect("draw", drawing->redraw);
			canvas->disconnect("tree_exiting", drawing->exiting);
		}
		drawing->clear();
		drawings.erase(p_canvas_id);
	}
}

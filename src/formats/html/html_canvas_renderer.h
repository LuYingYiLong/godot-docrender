#pragma once

#include "html_paint_run.h"

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/rid.hpp>

namespace godot {
	class CanvasItem;
}

namespace docrender {
	class HTMLCanvasRenderer : public godot::RefCounted {
		GDCLASS(HTMLCanvasRenderer, godot::RefCounted)

		struct CanvasDrawing {
			godot::Callable redraw;
			godot::Callable exiting;
			std::vector<godot::RID> items;
			std::vector<godot::Ref<godot::ShaderMaterial>> materials;
			int32_t run_count = 0;
			uint64_t redraw_id = 0;
			bool submitted = false;

			void clear();
			godot::RID add_item(godot::RenderingServer& p_server, const godot::RID& p_parent, int32_t p_draw_index);
		};

		godot::HashMap<uint64_t, CanvasDrawing> drawings;
		uint64_t next_redraw_id = 0;

		void on_canvas_redraw(uint64_t p_canvas_id);
		static void finish_canvas_redraw(uint64_t p_renderer_id, uint64_t p_canvas_id, uint64_t p_redraw_id);
		void detach_canvas(uint64_t p_canvas_id);

	protected:
		static void _bind_methods() {}

	public:
		~HTMLCanvasRenderer() override;

		void submit(godot::CanvasItem& p_canvas, const godot::Vector2& p_position, const godot::Rect2& p_clip,
			const std::vector<HTMLPaintRun>& p_runs, const std::vector<HTMLPaintRun>& p_diagnostics,
			const HTMLHighlight& p_selection, const HTMLHighlight& p_search);
		void queue_redraws() const;
	};
}

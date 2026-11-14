#pragma once

#include "layout/html_geometry.h"

#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/text_line.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/rid.hpp>

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

namespace godot {
	class RenderingServer;
	class StyleBox;
	class TextServer;
}

namespace docrender {
	void release_html_paint_shader();

	struct HTMLFill {
		godot::Rect2 rect;
		godot::Color color;
	};

	struct HTMLText {
		godot::Ref<godot::TextLine> line;
		godot::Vector2 position;
		godot::Color color;
		int64_t offset = -1;
		int64_t length = 0;
		const litehtml::render_item* renderer = nullptr;
	};

	struct HTMLImage {
		godot::Ref<godot::Texture2D> texture;
		godot::Rect2 destination;
		godot::Rect2 source;
		bool repeat;
	};

	struct HTMLCircle {
		godot::Vector2 center;
		float radius;
		godot::Color color;
		bool filled;
	};

	using HTMLPaintCommand = std::variant<HTMLFill, HTMLText, HTMLImage, HTMLCircle>;

	struct HTMLHighlight {
		int64_t from = 0;
		int64_t to = 0;
		const godot::StyleBox* style = nullptr;
		godot::Color color;
	};

	struct HTMLPaintRun {
		std::optional<godot::Rect2> clip;
		godot::Transform2D matrix;
		std::vector<TransformedClip> clips;
		godot::Ref<godot::ShaderMaterial> material;
		std::vector<HTMLPaintCommand> commands;

		void draw(
			godot::RenderingServer& p_server,
			const godot::RID& p_canvas,
			const godot::Vector2& p_offset,
			const godot::TextServer* p_text_server,
			const HTMLHighlight& p_search,
			const HTMLHighlight& p_selection
		) const;
	};
}

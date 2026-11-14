#pragma once

#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/vector4.hpp>

#include <deque>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace litehtml {
	class element;
	class render_item;
	struct border_radiuses;
	struct position;
}

namespace docrender {
	inline constexpr int MAX_ROUNDED_CLIPS = 16;

	godot::Rect2 to_rect(const litehtml::position& p_position);

	// Drawing offsets map document coordinates to the target canvas. Box and text
	// hit tests use the inverse translation to reach their local coordinates.
	inline godot::Rect2 translate_rect(const godot::Rect2& p_rect, const godot::Vector2& p_offset) {
		return { p_rect.position + p_offset, p_rect.size };
	}

	inline godot::Vector2 to_local_point(const godot::Vector2& p_point, const godot::Vector2& p_origin) {
		return p_point - p_origin;
	}

	struct RoundedRect {
		godot::Rect2 rect;
		godot::Vector4 radii_x;
		godot::Vector4 radii_y;

		RoundedRect(const godot::Rect2& p_rect, const litehtml::border_radiuses& p_radii);
		RoundedRect inset(const godot::Rect2& p_rect) const; // 返回内缩到指定矩形并相应缩小圆角后的形状
		RoundedRect spread(godot::real_t p_distance) const; // 按阴影扩张距离调整边界和圆角
		bool has_radius() const; // 检查是否存在有效的圆角
		bool has_point(const godot::Vector2& p_position) const; // 检查指定坐标是否在圆角矩形内
	};

	struct TransformedClip {
		RoundedRect shape;
		godot::Transform2D matrix;
	};

	godot::Rect2 clipped_visual_bounds(const godot::Rect2& p_rect, const godot::Transform2D& p_matrix,
		const std::vector<TransformedClip>& p_clips);

	// Geometry uses document coordinates after litehtml has placed and scrolled
	// the boxes. A link to the next ancestor shares clip geometry across siblings.
	struct OverflowClip {
		RoundedRect shape;
		godot::Transform2D matrix;
		godot::Transform2D inverse;
		litehtml::render_item* item;
		const OverflowClip* parent;
	};

	struct RenderGeometry {
		std::shared_ptr<litehtml::render_item> item;
		std::vector<RoundedRect> fragments;
		std::vector<godot::Rect2> visual_boxes;
		std::optional<godot::Rect2> anchor_fallback;
		godot::Transform2D matrix;
		std::optional<godot::Transform2D> inverse;
		const OverflowClip* overflow;
		bool viewport_fixed_subtree;

		bool contains_visible_point(const godot::Vector2& p_point) const;
		bool has_visible_area(const godot::Rect2& p_box) const;
	};

	class HTMLGeometryIndex {
		using ElementGeometry = std::vector<const RenderGeometry*>;

		// Renderer and clip addresses are stable for the lifetime of one reflow.
		std::deque<OverflowClip> clips;
		std::unordered_map<const litehtml::render_item*, RenderGeometry> renderers;
		godot::HashMap<const litehtml::element*, ElementGeometry> elements;
		bool transforms = false;

		void index(const std::shared_ptr<litehtml::render_item>& p_item, const OverflowClip* p_overflow,
			const godot::Transform2D& p_parent_matrix, bool p_viewport_fixed_subtree);

	public:
		HTMLGeometryIndex() = default;
		HTMLGeometryIndex(const HTMLGeometryIndex&) = delete;
		HTMLGeometryIndex& operator=(const HTMLGeometryIndex&) = delete;

		void rebuild(const std::shared_ptr<litehtml::render_item>& p_root);
		void clear();
		const ElementGeometry* find_element(const litehtml::element* p_element) const;
		const RenderGeometry* find_renderer(const litehtml::render_item* p_item) const;
		bool has_transforms() const { return transforms; }
		godot::Vector2 extend_overflow(const godot::Vector2& p_size) const;
	};
}

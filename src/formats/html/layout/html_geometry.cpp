#include "html_geometry.h"

#include <litehtml/render_item.h>

#include <algorithm>
#include <cmath>

using namespace godot;
using namespace litehtml;

namespace docrender {
	namespace {
		Transform2D element_matrix(const css_properties& p_css, const Rect2& p_box) {
			const auto& origin = p_css.get_transform_origin();
			const Vector2 pivot = p_box.position + Vector2(origin.x.calc_percent(p_box.size.x), origin.y.calc_percent(p_box.size.y));
			Transform2D matrix(0, pivot);
			for (const auto& function : p_css.get_transforms()) {
				using Kind = css_transform_function::kind;
				switch (function.type) {
					case Kind::translate:
						matrix *= Transform2D(0, Vector2(function.x.calc_percent(p_box.size.x), function.y.calc_percent(p_box.size.y)));
						break;
					case Kind::rotate:
						matrix *= Transform2D(function.first, Vector2());
						break;
					case Kind::scale:
						matrix *= Transform2D(Vector2(function.first, 0), Vector2(0, function.second), Vector2());
						break;
				}
			}
			return matrix * Transform2D(0, -pivot);
		}

		std::optional<Transform2D> inverse_of(const Transform2D& p_matrix) {
			if (!p_matrix.is_finite() || !std::isfinite(p_matrix.determinant()) || p_matrix.determinant() == 0) {
				return std::nullopt;
			}
			const auto inverse = p_matrix.affine_inverse();
			return inverse.is_finite() ? std::optional<Transform2D>(inverse) : std::nullopt;
		}

		using Polygon = std::vector<Vector2>;

		Polygon rect_polygon(const Rect2& p_box, const Transform2D& p_matrix) {
			const Vector2 end = p_box.get_end();
			return { p_matrix.xform(p_box.position), p_matrix.xform(Vector2(end.x, p_box.position.y)),
				p_matrix.xform(end), p_matrix.xform(Vector2(p_box.position.x, end.y)) };
		}

		Polygon clip_polygon(const TransformedClip& p_clip, bool p_rounded) {
			if (!p_rounded || !p_clip.shape.has_radius()) return rect_polygon(p_clip.shape.rect, p_clip.matrix);
			const auto& box = p_clip.shape.rect;
			const Vector2 end = box.get_end();
			const Vector2 corners[] = { box.position, Vector2(end.x, box.position.y), end, Vector2(box.position.x, end.y) };
			const Vector2 directions[] = { Vector2(1, 1), Vector2(-1, 1), Vector2(-1, -1), Vector2(1, -1) };
			constexpr real_t quarter_turn = 1.5707963267948966;
			constexpr int segments = 16;
			Polygon result;
			result.reserve(4 * (segments + 1));
			for (int corner = 0; corner < 4; ++corner) {
				const Vector2 radii(p_clip.shape.radii_x[corner], p_clip.shape.radii_y[corner]);
				if (radii.x <= 0 || radii.y <= 0) {
					result.push_back(p_clip.matrix.xform(corners[corner]));
					continue;
				}
				const Vector2 center = corners[corner] + radii * directions[corner];
				for (int step = 0; step <= segments; ++step) {
					const real_t angle = quarter_turn * (corner + 2 + static_cast<real_t>(step) / segments);
					result.push_back(p_clip.matrix.xform(center + radii * Vector2(std::cos(angle), std::sin(angle))));
				}
			}
			return result;
		}

		void intersect_polygon(Polygon& r_polygon, const Polygon& p_clip, real_t p_winding) {
			Polygon input;
			for (std::size_t edge = 0; edge < p_clip.size() && !r_polygon.empty(); ++edge) {
				const Vector2 a = p_clip[edge];
				const Vector2 direction = p_clip[(edge + 1) % p_clip.size()] - a;
				if (direction == Vector2()) continue;
				r_polygon.swap(input);
				r_polygon.clear();
				Vector2 previous = input.back();
				real_t previous_side = p_winding * direction.cross(previous - a);
				for (const Vector2& point : input) {
					const real_t side = p_winding * direction.cross(point - a);
					if ((previous_side >= 0) != (side >= 0)) {
						r_polygon.push_back(previous.lerp(point, previous_side / (previous_side - side)));
					}
					if (side >= 0) r_polygon.push_back(point);
					previous = point;
					previous_side = side;
				}
			}
		}

		real_t polygon_area(const Polygon& p_polygon) {
			const Vector2 origin = p_polygon.front();
			real_t twice_area = 0;
			for (std::size_t index = 1; index + 1 < p_polygon.size(); ++index)
				twice_area += (p_polygon[index] - origin).cross(p_polygon[index + 1] - origin);
			return twice_area;
		}
	}

	bool TransformedClip::contains_point(const Vector2& p_point, bool p_precise) const {
		return p_precise ? shape.has_point(matrix.affine_inverse().xform(p_point)) : matrix.xform(shape.rect).has_point(p_point);
	}

	Rect2 clipped_visual_bounds(const Rect2& p_rect, const Transform2D& p_matrix,
		const std::vector<TransformedClip>& p_clips) {
		if (!p_rect.has_area()) return {};
		if (p_clips.empty()) return p_matrix.xform(p_rect);
		Polygon polygon = rect_polygon(p_rect, p_matrix);
		int remaining = MAX_ROUNDED_CLIPS;
		for (auto clip = p_clips.rbegin(); clip != p_clips.rend(); ++clip) {
			if (!clip->shape.rect.has_area()) return {};
			const bool use_precise = use_precise_clip(clip->shape, clip->matrix, remaining);
			const Transform2D matrix = use_precise ? clip->matrix : Transform2D();
			const auto shape = use_precise ? clip_polygon(*clip, true) :
				rect_polygon(clip->matrix.xform(clip->shape.rect), matrix);
			intersect_polygon(polygon, shape, matrix.determinant() < 0 ? -1 : 1);
			if (polygon.size() < 3) return {};
		}
		if (polygon_area(polygon) == 0) return {};
		Rect2 bounds(polygon.front(), Vector2());
		for (const Vector2& point : polygon) bounds = bounds.expand(point);
		return bounds;
	}

	Rect2 to_rect(const litehtml::position& p_position) {
		return { p_position.x, p_position.y, p_position.width, p_position.height };
	}

	RoundedRect::RoundedRect(const Rect2& p_rect, const border_radiuses& p_radii) :
		rect(p_rect), radii_x(p_radii.top_left_x, p_radii.top_right_x, p_radii.bottom_right_x, p_radii.bottom_left_x),
		radii_y(p_radii.top_left_y, p_radii.top_right_y, p_radii.bottom_right_y, p_radii.bottom_left_y) {
		for (int i = 0; i < 4; ++i) {
			if (radii_x[i] <= 0 || radii_y[i] <= 0 || !std::isfinite(radii_x[i]) || !std::isfinite(radii_y[i])) {
				radii_x[i] = radii_y[i] = 0;
			}
		}
		const auto ratio = [](real_t p_size, real_t p_sum) -> real_t { return p_sum > 0 ? p_size / p_sum : 1; };
		const real_t scale = std::clamp<real_t>(std::min({
			ratio(rect.size.x, radii_x.x + radii_x.y), ratio(rect.size.x, radii_x.z + radii_x.w),
			ratio(rect.size.y, radii_y.x + radii_y.w), ratio(rect.size.y, radii_y.y + radii_y.z) }), 0, 1);
		radii_x *= scale;
		radii_y *= scale;
	}

	RoundedRect RoundedRect::inset(const Rect2& p_rect) const {
		const Vector2 leading = p_rect.position - rect.position;
		const Vector2 trailing = rect.get_end() - p_rect.get_end();
		border_radiuses radii;
		radii.top_left_x = radii_x.x - leading.x;
		radii.top_right_x = radii_x.y - trailing.x;
		radii.bottom_right_x = radii_x.z - trailing.x;
		radii.bottom_left_x = radii_x.w - leading.x;
		radii.top_left_y = radii_y.x - leading.y;
		radii.top_right_y = radii_y.y - leading.y;
		radii.bottom_right_y = radii_y.z - trailing.y;
		radii.bottom_left_y = radii_y.w - trailing.y;
		return RoundedRect(p_rect, radii);
	}

	RoundedRect RoundedRect::spread(real_t p_distance) const {
		const auto expand_radius = [p_distance](real_t p_radius) {
			if (p_distance > p_radius) {
				const real_t ratio = p_radius / p_distance - 1;
				return p_radius + p_distance * (1 + ratio * ratio * ratio);
			}
			return std::max<real_t>(0, p_radius + p_distance);
		};
		border_radiuses radii;
		radii.top_left_x = expand_radius(radii_x.x);
		radii.top_right_x = expand_radius(radii_x.y);
		radii.bottom_right_x = expand_radius(radii_x.z);
		radii.bottom_left_x = expand_radius(radii_x.w);
		radii.top_left_y = expand_radius(radii_y.x);
		radii.top_right_y = expand_radius(radii_y.y);
		radii.bottom_right_y = expand_radius(radii_y.z);
		radii.bottom_left_y = expand_radius(radii_y.w);
		return RoundedRect(rect.grow(p_distance), radii);
	}

	bool RoundedRect::has_radius() const {
		return radii_x != Vector4();
	}

	bool RoundedRect::has_point(const Vector2& p_position) const {
		if (!rect.has_point(p_position)) {
			return false;
		}
		const Vector2 from_start = to_local_point(p_position, rect.position);
		const Vector2 from_end = rect.get_end() - p_position;
		for (int i = 0; i < 4; ++i) {
			const Vector2 radius(radii_x[i], radii_y[i]);
			const Vector2 edge(i == 0 || i == 3 ? from_start.x : from_end.x, i < 2 ? from_start.y : from_end.y);
			if (edge.x < radius.x && edge.y < radius.y) {
				return ((radius - edge) / radius).length_squared() <= 1.0;
			}
		}
		return true;
	}

	bool RenderGeometry::contains_visible_point(const Vector2& p_point) const {
		if (!inverse) return false;
		const Vector2 local = inverse->xform(p_point);
		const bool inside = std::any_of(fragments.begin(), fragments.end(), [&](const RoundedRect& p_fragment) {
			return p_fragment.has_point(local);
		});
		if (!inside) {
			return false;
		}
		int remaining = MAX_ROUNDED_CLIPS;
		for (auto* clip = overflow; clip; clip = clip->parent) {
			const bool precise = use_precise_clip(clip->shape, clip->matrix, remaining);
			if (!(precise ? clip->shape.has_point(clip->inverse.xform(p_point)) : clip->matrix.xform(clip->shape.rect).has_point(p_point))) {
				return false;
			}
		}
		return true;
	}

	bool RenderGeometry::has_visible_area(const Rect2& p_box) const {
		if (!inverse) return false;
		const auto transformed_clip = [&] {
			for (auto* clip = overflow; clip; clip = clip->parent)
				if (clip->matrix != Transform2D()) return true;
			return false;
		};
		if (matrix != Transform2D() || transformed_clip()) {
			std::vector<TransformedClip> shapes;
			for (auto* entry = overflow; entry; entry = entry->parent) shapes.push_back({ entry->shape, entry->matrix });
			std::reverse(shapes.begin(), shapes.end());
			return clipped_visual_bounds(p_box, matrix, shapes).has_area();
		}
		Rect2 visible = p_box;
		int remaining = MAX_ROUNDED_CLIPS;
		for (auto* entry = overflow; entry; entry = entry->parent) {
			const RoundedRect& clip = entry->shape;
			visible = visible.intersection(entry->matrix.xform(clip.rect));
			if (!visible.has_area()) {
				return false;
			}
			if (use_precise_clip(clip, entry->matrix, remaining)) {
				// A rectangle entirely inside a cut-off corner cannot be revealed by scrolling the label.
				for (int i = 0; i < 4; ++i) {
					const Vector2 inward(i == 0 || i == 3 ? visible.get_end().x : visible.position.x,
						i < 2 ? visible.get_end().y : visible.position.y);
					const Vector2 edge(i == 0 || i == 3 ? inward.x - clip.rect.position.x : clip.rect.get_end().x - inward.x,
						i < 2 ? inward.y - clip.rect.position.y : clip.rect.get_end().y - inward.y);
					const Vector2 radius(clip.radii_x[i], clip.radii_y[i]);
					if (edge.x < radius.x && edge.y < radius.y && ((radius - edge) / radius).length_squared() >= 1.0) {
						return false;
					}
				}
			}
		}
		return visible.has_area();
	}

	void HTMLGeometryIndex::rebuild(const std::shared_ptr<render_item>& p_root) {
		clear();
		if (p_root) {
			index(p_root, nullptr, Transform2D(), false);
		}
	}

	void HTMLGeometryIndex::clear() {
		elements.clear();
		renderers.clear();
		clips.clear();
		transforms = false;
	}

	const HTMLGeometryIndex::ElementGeometry* HTMLGeometryIndex::find_element(const element* p_element) const {
		return elements.getptr(p_element);
	}

	const RenderGeometry* HTMLGeometryIndex::find_renderer(const render_item* p_item) const {
		const auto found = renderers.find(p_item);
		return found != renderers.end() ? &found->second : nullptr;
	}

	void HTMLGeometryIndex::index(const std::shared_ptr<render_item>& p_item, const OverflowClip* p_overflow,
		const Transform2D& p_parent_matrix, bool p_viewport_fixed_subtree) {
		const auto& css = p_item->css();
		const auto display = css.get_display();
		const bool fixed = p_item->is_viewport_fixed();
		const bool fixed_subtree = p_viewport_fixed_subtree || fixed;
		Transform2D matrix = fixed ? Transform2D() : p_parent_matrix;
		position::vector boxes;
		if (display != display_inline_text) {
			p_item->get_rendering_boxes(boxes);
		}
		if (css.has_transform()) {
			transforms = true;
			Rect2 reference = boxes.empty() ? to_rect(p_item->get_placement()) : to_rect(boxes.front());
			for (std::size_t index = 1; index < boxes.size(); ++index) reference = reference.merge(to_rect(boxes[index]));
			matrix *= element_matrix(css, reference);
		}
		const auto inverse = inverse_of(matrix);
		auto& entry = renderers.emplace(p_item.get(),
			RenderGeometry{ p_item, {}, {}, std::nullopt, matrix, inverse, p_overflow, fixed_subtree }).first->second;
		if (display != display_inline_text) {
			elements[p_item->src_el().get()].push_back(&entry);
			entry.fragments.reserve(boxes.size());
			for (std::size_t i = 0; i < boxes.size(); ++i) {
				const Rect2 box = to_rect(boxes[i]);
				if (!box.is_finite() || box.size.x < 0 || box.size.y < 0) {
					continue;
				}
				border_radiuses radii;
				if (display != display_table_row) {
					radii = css.get_borders().radius.calc_percents(box.size.x, box.size.y);
					if (display == display_inline) {
						if (i > 0) {
							radii.top_left_x = radii.top_left_y = radii.bottom_left_x = radii.bottom_left_y = 0;
						}
						if (i + 1 < boxes.size()) {
							radii.top_right_x = radii.top_right_y = radii.bottom_right_x = radii.bottom_right_y = 0;
						}
					}
				}
				entry.fragments.emplace_back(box, radii);
				const Rect2 visual = matrix.xform(box);
				if (visual.is_finite()) entry.visual_boxes.push_back(visual);
			}
			if (boxes.empty()) {
				const Rect2 fallback = matrix.xform(to_rect(p_item->get_placement()));
				if (fallback.is_finite() && fallback.size.x >= 0 && fallback.size.y >= 0) {
					entry.anchor_fallback = fallback;
				}
			}
		}

		// An overflow clip applies to descendants, while a fragment's own
		// rounded border is tested independently for element hits.
		if (inverse && css.get_overflow() > overflow_visible && display != display_inline && !p_item->children().empty()) {
			const position box = p_item->get_placement();
			position border_box = box;
			border_box += p_item->get_paddings();
			border_box += p_item->get_borders();
			auto radii = css.get_borders().radius.calc_percents(border_box.width, border_box.height);
			radii -= p_item->get_borders();
			radii -= p_item->get_paddings();
			clips.push_back({ RoundedRect(to_rect(box), radii), matrix, *inverse, p_item.get(), p_overflow });
			p_overflow = &clips.back();
		}
		for (const auto& child : p_item->children()) {
			index(child, p_overflow, matrix, fixed_subtree);
		}
	}

	Vector2 HTMLGeometryIndex::extend_overflow(const Vector2& p_size) const {
		Vector2 extent = p_size;
		for (const auto& pair : renderers) {
			const auto& geometry = pair.second;
			if (!geometry.inverse || !geometry.item->is_visible() || geometry.viewport_fixed_subtree) continue;
			const auto* nearest_clip = geometry.overflow;
			const bool can_scroll_inside = nearest_clip &&
				is_one_of(nearest_clip->item->css().get_overflow(), overflow_scroll, overflow_auto) &&
				nearest_clip->matrix != geometry.matrix;
			if (geometry.matrix == Transform2D() && !can_scroll_inside) continue;
			std::vector<TransformedClip> ancestors;
			for (auto* clip = nearest_clip; clip; clip = clip->parent) ancestors.push_back({ clip->shape, clip->matrix });
			std::reverse(ancestors.begin(), ancestors.end());
			const auto include = [&](const Rect2& p_box) {
				if (!p_box.is_finite() || !p_box.has_area()) return;
				const Rect2 box = clipped_visual_bounds(p_box, geometry.matrix, ancestors);
				if (box.has_area()) extent = extent.max(box.get_end());
				if (can_scroll_inside) {
					const Rect2 local = (nearest_clip->inverse * geometry.matrix).xform(p_box);
					if (local.is_finite() && local.has_area() && local.get_end().x > nearest_clip->shape.rect.position.x &&
						local.get_end().y > nearest_clip->shape.rect.position.y) {
						auto* item = nearest_clip->item;
						const Vector2 offset(item->get_scroll_left(), item->get_scroll_top());
						const Vector2 end = local.get_end() - nearest_clip->shape.rect.position + offset;
						item->extend_scroll_size(size(end.x, end.y));
					}
				}
			};
			for (const auto& fragment : geometry.fragments) include(fragment.rect);
			if (geometry.item->css().get_overflow() == overflow_visible) {
				const auto extra = geometry.item->used_layout_overflow();
				if (extra.width > 0 && extra.height > 0) {
					const auto origin = to_rect(geometry.item->get_placement()).position;
					include(Rect2(origin, Vector2(extra.width, extra.height)));
				}
			}
		}
		return extent;
	}
}

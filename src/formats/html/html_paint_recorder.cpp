#include "html_paint_recorder.h"

#include "html_text_mask.h"

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector4_array.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <litehtml/document.h>
#include <litehtml/document_container.h>
#include <litehtml/html_tag.h>
#include <litehtml/render_item.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

using namespace godot;
using namespace litehtml;

namespace docrender {
	namespace {
		Ref<Shader> paint_shader;

		const char* PAINT_SHADER_CODE = R"SHADER(
shader_type canvas_item;

uniform int clip_count;
uniform vec4 clip_boxes[16];
uniform vec4 clip_radii_x[16];
uniform vec4 clip_radii_y[16];
uniform vec4 clip_bases[16];
uniform vec2 clip_offsets[16];
uniform bool affine_clips;

uniform bool has_shape;
uniform vec4 shape_box;
uniform vec4 shape_radii_x;
uniform vec4 shape_radii_y;

uniform bool draw_border;
uniform vec4 inner_box;
uniform vec4 inner_radii_x;
uniform vec4 inner_radii_y;
uniform vec4 border_widths;
uniform vec4 border_colors[4];
uniform ivec4 border_styles;
const int BORDER_DOTTED = 2;
const int BORDER_DASHED = 3;
const int BORDER_DOUBLE = 4;
uniform vec4 double_outer_box;
uniform vec4 double_outer_rx;
uniform vec4 double_outer_ry;
uniform vec4 double_inner_box;
uniform vec4 double_inner_rx;
uniform vec4 double_inner_ry;
uniform vec4 border_center_box;
uniform vec4 border_center_rx;
uniform vec4 border_center_ry;
uniform vec4 border_edge_starts;
uniform vec4 border_edge_lengths;
uniform vec4 border_arc_lengths;
uniform vec4 border_periods;
uniform vec4 border_pattern_starts;
uniform bool border_pattern_continuous;

uniform int shadow_type;
uniform vec4 shadow_box;
uniform vec4 shadow_rx;
uniform vec4 shadow_ry;
uniform float shadow_blur;
uniform sampler2D text_shadow_mask : filter_linear_mipmap, repeat_disable;
uniform vec4 text_shadow_box;
uniform int decoration_pattern;
uniform float decoration_thickness;
uniform float decoration_center;

uniform int gradient_type;
uniform int stop_count;
uniform sampler2D gradient_stops : filter_nearest, repeat_disable;
uniform vec2 gradient_start;
uniform vec2 gradient_parameters;
uniform vec4 tile;
uniform ivec2 repeat_axes;

varying vec2 document_point;

void vertex() {
	document_point = VERTEX;
}

int rounded_corner(vec2 from_start, vec2 from_end, vec4 rx, vec4 ry) {
	if (from_start.x < rx.x && from_start.y < ry.x) {
		return 0;
	}
	if (from_end.x < rx.y && from_start.y < ry.y) {
		return 1;
	}
	if (from_end.x < rx.z && from_end.y < ry.z) {
		return 2;
	}
	if (from_start.x < rx.w && from_end.y < ry.w) {
		return 3;
	}
	return -1;
}

float box_coverage(vec2 p, vec4 box, vec4 rx, vec4 ry, vec2 pixel_x, vec2 pixel_y) {
	vec2 from_start = p - box.xy;
	vec2 from_end = box.zw - p;
	vec2 near_edge = min(from_start, from_end);
	float distance_to_edge = min(near_edge.x, near_edge.y);
	vec2 normal = near_edge.x < near_edge.y ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
	int corner = rounded_corner(from_start, from_end, rx, ry);
	if (corner >= 0 && rx[corner] > 0.0 && ry[corner] > 0.0) {
		vec2 radius = vec2(rx[corner], ry[corner]);
		vec2 edge = vec2(
			corner == 0 || corner == 3 ? from_start.x : from_end.x,
			corner < 2 ? from_start.y : from_end.y);
		vec2 unit = (radius - edge) / radius;
		vec2 gradient = unit / radius;
		float normal_length = length(gradient);
		distance_to_edge = (1.0 - length(unit)) / max(normal_length, 0.000001);
		normal = gradient * vec2(corner == 0 || corner == 3 ? 1.0 : -1.0, corner < 2 ? 1.0 : -1.0) /
			max(normal_length, 0.000001);
	}
	float aa = max(abs(dot(normal, pixel_x)) + abs(dot(normal, pixel_y)), 0.001);
	return clamp(distance_to_edge / aa + 0.5, 0.0, 1.0);
}

// Approximation of the standard normal cumulative distribution.
float gaussian_cdf(float x) {
	float t = 1.0 / (1.0 + 0.2316419 * abs(x));
	float tail = 0.39894228 * exp(-0.5 * x * x) * t *
		(0.31938153 + t * (-0.35656378 + t * (1.78147794 + t * (-1.82125598 + t * 1.33027443))));
	return x < 0.0 ? tail : 1.0 - tail;
}

float ellipse_inset(float y, float rx, float ry) {
	if (y >= ry || ry <= 0.0) {
		return 0.0;
	}
	float relative = 1.0 - y / ry;
	return rx * (1.0 - sqrt(max(0.0, 1.0 - relative * relative)));
}

float shadow_mask(vec2 p, vec2 pixel_x, vec2 pixel_y) {
	vec2 size = shadow_box.zw - shadow_box.xy;
	if (size.x <= 0.0 || size.y <= 0.0) {
		return 0.0;
	}
	if (shadow_blur <= 0.0) {
		return box_coverage(p, shadow_box, shadow_rx, shadow_ry, pixel_x, pixel_y);
	}
	float sigma = shadow_blur * 0.5;
	if (sigma < max(length(pixel_x), length(pixel_y)) * 0.2) {
		return box_coverage(p, shadow_box, shadow_rx, shadow_ry, pixel_x, pixel_y);
	}
	vec2 point = p - shadow_box.xy;
	float low = max(0.0, point.y - 3.5 * sigma);
	float high = min(size.y, point.y + 3.5 * sigma);
	if (low >= high || point.x < -3.5 * sigma || point.x > size.x + 3.5 * sigma) {
		return 0.0;
	}
	float top_radius = max(shadow_ry.x, shadow_ry.y);
	float bottom_radius = max(shadow_ry.z, shadow_ry.w);
	float vertical_low = gaussian_cdf((low - point.y) / sigma);
	float left_radius = max(shadow_rx.x, shadow_rx.w);
	float right_radius = max(shadow_rx.y, shadow_rx.z);
	if ((low >= top_radius && high <= size.y - bottom_radius) ||
			(point.x - 3.5 * sigma >= left_radius && point.x + 3.5 * sigma <= size.x - right_radius)) {
		float vertical = gaussian_cdf((high - point.y) / sigma) - vertical_low;
		return vertical * (gaussian_cdf((size.x - point.x) / sigma) - gaussian_cdf(-point.x / sigma));
	}
	// Integrate each horizontal span analytically, then sample the rounded edges vertically.
	float step_size = (high - low) / 16.0;
	float total = 0.0;
	for (int i = 0; i < 16; i++) {
		float y = low + (float(i) + 0.5) * step_size;
		float left = max(ellipse_inset(y, shadow_rx.x, shadow_ry.x), ellipse_inset(size.y - y, shadow_rx.w, shadow_ry.w));
		float right = size.x - max(ellipse_inset(y, shadow_rx.y, shadow_ry.y), ellipse_inset(size.y - y, shadow_rx.z, shadow_ry.z));
		float horizontal = max(0.0, gaussian_cdf((right - point.x) / sigma) - gaussian_cdf((left - point.x) / sigma));
		float vertical_high = gaussian_cdf((y + step_size * 0.5 - point.y) / sigma);
		total += horizontal * (vertical_high - vertical_low);
		vertical_low = vertical_high;
	}
	return total;
}

float text_mask_sample(vec2 uv, float lod) {
	if (any(lessThan(uv, vec2(0.0))) || any(greaterThanEqual(uv, vec2(1.0)))) {
		return 0.0;
	}
	return textureLod(text_shadow_mask, uv, lod).a;
}

float blurred_text_mask(vec2 point) {
	vec2 uv = (point - text_shadow_box.xy) / text_shadow_box.zw;
	if (shadow_blur <= 0.0) {
		return text_mask_sample(uv, 0.0);
	}
	// Mip levels integrate fine glyph details when the taps are far apart.
	float step_size = shadow_blur / 3.0;
	vec2 step_uv = step_size / text_shadow_box.zw;
	float lod = log2(max(step_size, 1.0));
	const float weights[5] = { 1.0, 0.8007374, 0.4111123, 0.1353353, 0.0285655 };
	float total = 0.0;
	for (int y = -4; y <= 4; ++y) {
		for (int x = -4; x <= 4; ++x) {
			total += weights[abs(x)] * weights[abs(y)] * text_mask_sample(uv + vec2(float(x), float(y)) * step_uv, lod);
		}
	}
	return total * (1.0 / 14.07376);
}

// Approximate partial arc length with Simpson integration of the ellipse speed.
float ellipse_arc_fraction(vec2 axes, float angle, vec2 direction) {
	vec2 half_angle = vec2(sin(angle * 0.5), cos(angle * 0.5));
	float arc = angle * (axes.y + 4.0 * length(axes * half_angle) + length(axes * direction));
	float quarter_arc = (PI * 0.5) * (axes.y + 4.0 * length(axes * vec2(0.70710678)) + axes.x);
	return arc / quarter_arc;
}

float border_path_distance(vec2 p, int side, float perimeter) {
	vec2 start = p - border_center_box.xy;
	vec2 end = border_center_box.zw - p;
	int corner = rounded_corner(start, end, border_center_rx, border_center_ry);
	if (corner >= 0 && border_center_rx[corner] > 0.0 && border_center_ry[corner] > 0.0) {
		vec2 radius = vec2(border_center_rx[corner], border_center_ry[corner]);
		vec2 edge = vec2(corner == 0 || corner == 3 ? start.x : end.x, corner < 2 ? start.y : end.y);
		vec2 unit = (radius - edge) / radius;
		vec2 along = corner % 2 == 0 ? unit : unit.yx;
		float angle = atan(along.y, along.x);
		vec2 axes = corner % 2 == 0 ? radius : radius.yx;
		float fraction = ellipse_arc_fraction(axes, angle, normalize(along));
		int previous_side = (corner + 3) % 4;
		if (corner == 0) {
			return perimeter + (fraction - 1.0) * border_arc_lengths[previous_side];
		}
		return border_edge_starts[previous_side] + border_edge_lengths[previous_side] +
			fraction * border_arc_lengths[previous_side];
	}
	vec4 along = vec4(start.x, start.y, end.x, end.y);
	vec4 tangent = vec4(border_center_rx.x, border_center_ry.y, border_center_rx.z, border_center_ry.w);
	return border_edge_starts[side] + along[side] - tangent[side];
}

// Locate a dot's center on the rounded path, then measure the circle in document pixels.
vec2 border_path_point(float distance_along, float perimeter) {
	distance_along = mod(distance_along, max(perimeter, 0.001));
	int side = 0;
	for (int i = 1; i < 4; i++) {
		if (distance_along >= border_edge_starts[i]) {
			side = i;
		}
	}
	vec2 start = border_center_box.xy;
	vec2 end = border_center_box.zw;
	vec4 tangent = vec4(border_center_rx.x, border_center_ry.y, border_center_rx.z, border_center_ry.w);
	float edge = distance_along - border_edge_starts[side];
	if (edge <= border_edge_lengths[side] || border_arc_lengths[side] == 0.0) {
		float along = edge + tangent[side];
		if (side == 0) { return vec2(start.x + along, start.y); }
		if (side == 1) { return vec2(end.x, start.y + along); }
		if (side == 2) { return vec2(end.x - along, end.y); }
		return vec2(start.x, end.y - along);
	}
	int corner = (side + 1) % 4;
	vec2 radius = vec2(border_center_rx[corner], border_center_ry[corner]);
	vec2 axes = corner % 2 == 0 ? radius : radius.yx;
	float fraction = (edge - border_edge_lengths[side]) / border_arc_lengths[side];
	float low = 0.0;
	float high = PI * 0.5;
	for (int i = 0; i < 7; i++) {
		float angle = (low + high) * 0.5;
		vec2 angle_vector = vec2(sin(angle), cos(angle));
		if (ellipse_arc_fraction(axes, angle, angle_vector) < fraction) {
			low = angle;
		} else {
			high = angle;
		}
	}
	float angle = (low + high) * 0.5;
	vec2 unit = vec2(cos(angle), sin(angle));
	vec2 offset = radius * (vec2(1.0) - (corner % 2 == 0 ? unit : unit.yx));
	return vec2(corner == 0 || corner == 3 ? start.x + offset.x : end.x - offset.x,
		corner < 2 ? start.y + offset.y : end.y - offset.y);
}

vec4 sample_gradient(float t) {
	int first = 0;
	int last = stop_count;
	// Find the first stop strictly after t, preserving coincident stops.
	for (int step_index = 0; step_index < 13 && first < last; step_index++) {
		int middle = (first + last) / 2;
		float position = texelFetch(gradient_stops, ivec2(middle, 1), 0).r;
		if (position <= t) {
			first = middle + 1;
		} else {
			last = middle;
		}
	}
	int left = max(first - 1, 0);
	int right = min(first, stop_count - 1);
	vec4 left_color = texelFetch(gradient_stops, ivec2(left, 0), 0);
	vec4 right_color = texelFetch(gradient_stops, ivec2(right, 0), 0);
	vec2 left_stop = texelFetch(gradient_stops, ivec2(left, 1), 0).rg;
	float span = texelFetch(gradient_stops, ivec2(right, 1), 0).r - left_stop.x;
	float weight = span > 0.0 ? clamp((t - left_stop.x) / span, 0.0, 1.0) : 0.0;
	float hint = left_stop.y;
	if (hint > 0.0 && hint < 1.0) {
		weight = pow(weight, log(0.5) / log(hint));
	}
	left_color.rgb *= left_color.a;
	right_color.rgb *= right_color.a;
	vec4 color = mix(left_color, right_color, weight);
	color.rgb /= max(color.a, 0.000001);
	return color;
}

void fragment() {
	// Take derivatives before the corner and paint branches diverge.
	vec2 pixel_x = dFdx(document_point);
	vec2 pixel_y = dFdy(document_point);
	float coverage = 1.0;
	for (int i = 0; i < clip_count; i++) {
		vec2 point = document_point;
		vec2 dx = pixel_x;
		vec2 dy = pixel_y;
		if (affine_clips) {
			vec4 basis = clip_bases[i];
			mat2 relative = mat2(basis.xy, basis.zw);
			point = relative * point + clip_offsets[i];
			dx = relative * dx;
			dy = relative * dy;
		}
		coverage = min(coverage, box_coverage(point, clip_boxes[i], clip_radii_x[i], clip_radii_y[i], dx, dy));
	}
	if (has_shape) {
		coverage = min(coverage, box_coverage(document_point, shape_box, shape_radii_x, shape_radii_y, pixel_x, pixel_y));
	}
	if (decoration_pattern != 0) {
		float width = decoration_thickness;
		float y = document_point.y - decoration_center;
		float aa = max(length(abs(pixel_x) + abs(pixel_y)), 0.001);
		float distance_to_edge;
		if (decoration_pattern == 1) {
			float x = mod(document_point.x, 2.0 * width) - width;
			distance_to_edge = width * 0.5 - length(vec2(x, y));
		} else if (decoration_pattern == 2) {
			float x = mod(document_point.x, 5.0 * width) - 2.5 * width;
			distance_to_edge = min(width * 1.5 - abs(x), width * 0.5 - abs(y));
		} else {
			float phase = mod(document_point.x, 6.0 * width) / (6.0 * width) * TAU;
			float slope = cos(phase) * TAU / 6.0;
			distance_to_edge = width * 0.5 - abs(y - sin(phase) * width) / sqrt(1.0 + slope * slope);
		}
		coverage = min(coverage, clamp(distance_to_edge / aa + 0.5, 0.0, 1.0));
	} else if (shadow_type == 3) {
		coverage *= blurred_text_mask(document_point);
	} else if (shadow_type != 0) {
		float mask = shadow_mask(document_point, pixel_x, pixel_y);
		if (shadow_type == 1) {
			coverage *= mask * (1.0 - box_coverage(document_point, inner_box, inner_radii_x, inner_radii_y, pixel_x, pixel_y));
		} else {
			coverage *= 1.0 - mask;
		}
	} else if (draw_border) {
		if (inner_box.z > inner_box.x && inner_box.w > inner_box.y) {
			coverage = min(coverage, 1.0 - box_coverage(document_point, inner_box, inner_radii_x, inner_radii_y, pixel_x, pixel_y));
		}
		vec2 a = document_point - shape_box.xy;
		vec2 b = shape_box.zw - document_point;
		vec4 distance = vec4(a.y, b.x, b.y, a.x) / max(border_widths, vec4(0.000001));
		int side = -1;
		for (int i = 0; i < 4; i++) {
			if (border_widths[i] > 0.0 && (side < 0 || distance[i] < distance[side])) {
				side = i;
			}
		}
		int style = border_styles[side];
		if (style == BORDER_DOTTED || style == BORDER_DASHED) {
			int path_side = side;
			if (border_pattern_continuous) {
				vec2 center_start = document_point - border_center_box.xy;
				vec2 center_end = border_center_box.zw - document_point;
				vec4 center_distance = vec4(center_start.y, center_end.x, center_end.y, center_start.x);
				for (int i = 0; i < 4; i++) {
					if (center_distance[i] < center_distance[path_side]) {
						path_side = i;
					}
				}
			}
			float perimeter = border_edge_starts.w + border_edge_lengths.w + border_arc_lengths.w;
			float path = border_path_distance(document_point, path_side, perimeter);
			if (!border_pattern_continuous && side == 0 && path > border_edge_lengths.x + border_arc_lengths.x) {
				path -= perimeter;
			}
			float period = border_periods[side];
			float along = mod(path - border_pattern_starts[side], period) - period * 0.5;
			float aa = max(length(abs(pixel_x) + abs(pixel_y)), 0.001);
			float edge = period * 0.3 - abs(along);
			if (style == BORDER_DOTTED) {
				float radius = min(border_widths[side], period) * 0.5;
				float center = path - along;
				float distance_to_dot = length(document_point - border_path_point(center, perimeter));
				// An adjacent center can be closer around a sharp or elliptical corner.
				if (border_pattern_continuous && distance_to_dot > radius - aa) {
					float adjacent = center + (along < 0.0 ? -period : period);
					distance_to_dot = min(distance_to_dot, length(document_point - border_path_point(adjacent, perimeter)));
				}
				edge = radius - distance_to_dot;
			}
			coverage = min(coverage, clamp(edge / aa + 0.5, 0.0, 1.0));
		} else if (style == BORDER_DOUBLE) {
			float outside_gap = 1.0 - box_coverage(document_point, double_outer_box, double_outer_rx, double_outer_ry, pixel_x, pixel_y);
			float inside_gap = box_coverage(document_point, double_inner_box, double_inner_rx, double_inner_ry, pixel_x, pixel_y);
			coverage = min(coverage, max(outside_gap, inside_gap));
		}
		COLOR *= border_colors[side];
	} else if (gradient_type > 0) {
		vec2 point = document_point;
		if (repeat_axes.x != 0) {
			point.x = tile.x + mod(point.x - tile.x, tile.z);
		}
		if (repeat_axes.y != 0) {
			point.y = tile.y + mod(point.y - tile.y, tile.w);
		}
		vec2 delta = point - gradient_start;
		float t = 0.0;
		if (gradient_type == 1) {
			t = dot(delta, gradient_parameters) / max(dot(gradient_parameters, gradient_parameters), 0.000001);
		} else if (gradient_type == 2) {
			t = length(delta / max(gradient_parameters, vec2(0.000001)));
		} else {
			t = fract(atan(delta.x, -delta.y) / TAU - gradient_parameters.x);
		}
		COLOR *= sample_gradient(t);
	}
	COLOR.a *= coverage;
}
)SHADER";

		Color to_color(const web_color& p_color) {
			return Color(p_color.red / 255.0f, p_color.green / 255.0f, p_color.blue / 255.0f, p_color.alpha / 255.0f);
		}

		Rect2 get_background_clip(const background_layer& p_layer) {
			return to_rect(p_layer.border_box).intersection(to_rect(p_layer.clip_box));
		}

		Rect2 get_tile_bounds(const background_layer& p_layer, const Rect2& p_clip, bool p_repeat_x, bool p_repeat_y) {
			const Rect2 tile = to_rect(p_layer.origin_box);
			return p_clip.intersection(Rect2(p_repeat_x ? p_clip.position.x : tile.position.x,
				p_repeat_y ? p_clip.position.y : tile.position.y, p_repeat_x ? p_clip.size.x : tile.size.x,
				p_repeat_y ? p_clip.size.y : tile.size.y));
		}

		Vector4 box_uniform(const Rect2& p_rect) {
			return Vector4(p_rect.position.x, p_rect.position.y, p_rect.get_end().x, p_rect.get_end().y);
		}

		enum BorderPaint { BORDER_NONE, BORDER_SOLID, BORDER_DOTTED, BORDER_DASHED, BORDER_DOUBLE };

		BorderPaint get_border_paint(border_style p_style) {
			switch (p_style) {
				case border_style_solid: return BORDER_SOLID;
				case border_style_dotted: return BORDER_DOTTED;
				case border_style_dashed: return BORDER_DASHED;
				case border_style_double: return BORDER_DOUBLE;
				default: return BORDER_NONE;
			}
		}

		float quarter_ellipse_length(float p_x, float p_y) {
			const float sum = p_x + p_y;
			if (sum == 0.0f) {
				return 0.0f;
			}
			// Ramanujan's approximation, divided by four for each corner.
			const float ratio = (p_x - p_y) / sum;
			const float h = ratio * ratio;
			return Math_PI * 0.25f * sum * (1.0f + 3.0f * h / (10.0f + std::sqrt(4.0f - 3.0f * h)));
		}

		float border_pattern_period(float p_length, float p_width, BorderPaint p_style) {
			const float preferred = p_width * (p_style == BORDER_DOTTED ? 2.0f : 5.0f);
			const float count = std::max(1.0f, std::round(p_length / preferred));
			return std::max(0.001f, p_length / count);
		}

	} // namespace

	void HTMLPaintRecorder::clear() {
		recording_geometry = nullptr;
		active_geometry = nullptr;
		clips.clear();
		clip_material.unref();
		painted_elements.clear();
		painted_text.clear();
		painted_text_by_renderer.clear();
		paint_runs.clear();
		diagnostic_runs.clear();
		plain_text = String();
		text_runs.clear();
	}

	void HTMLPaintRecorder::begin(const HTMLGeometryIndex& p_geometry) {
		clear();
		recording_geometry = &p_geometry;
	}

	void HTMLPaintRecorder::finish(const element::ptr& p_root, const TextShaper& p_shape_text) {
		clips.clear();
		clip_material.unref();
		active_geometry = nullptr;
		for (HTMLPaintRun& run : paint_runs) {
			for (Command& command : run.commands) {
				if (Text* text = std::get_if<Text>(&command)) {
					const Rect2 rect(text->position, text->line->get_size());
					const Rect2 visible = run.clips.empty() && run.clip ? run.matrix.xform(rect).intersection(*run.clip) :
						clipped_visual_bounds(rect, run.matrix, run.clips);
					if (visible.has_area() && text->length > 0) {
						if (text->renderer) painted_text_by_renderer[text->renderer].push_back(text);
						else painted_text[text->position].push_back(text);
					}
				}
			}
		}
		String separator;
		index_text(p_root, p_shape_text, separator);
		// Paint order can differ from document order for positioned or floating text.
		// The DOM pass assigns offsets while the paint runs retain the exact clip geometry.
		painted_text.clear();
		painted_text_by_renderer.clear();
		for (const HTMLPaintRun& run : paint_runs) {
			for (const Command& command : run.commands) {
				if (const Text* text = std::get_if<Text>(&command); text && text->offset >= 0) {
					const Rect2 rect(text->position, text->line->get_size());
					const Rect2 visible = run.clips.empty() && run.clip ? run.matrix.xform(rect).intersection(*run.clip) :
						clipped_visual_bounds(rect, run.matrix, run.clips);
					text_runs.push_back({ text->line, rect, visible, text->offset, text->length,
						run.matrix, run.matrix.affine_inverse(), run.clips });
				}
			}
		}
		recording_geometry = nullptr;
	}

	std::shared_ptr<litehtml::element> HTMLPaintRecorder::hit_test_element(const Vector2& p_point) const {
		for (auto it = painted_elements.rbegin(); it != painted_elements.rend(); ++it) {
			const auto* geometry = *it;
			if (geometry->item->is_visible() && geometry->contains_visible_point(p_point)) {
				return geometry->item->src_el();
			}
		}
		return {};
	}

	void HTMLPaintRecorder::record_css_diagnostic(const RenderGeometry& p_geometry, const Rect2& p_box) {
		if (!p_geometry.inverse) return;
		active_geometry = &p_geometry;
		std::vector<const OverflowClip*> overflow;
		for (auto* clip = p_geometry.overflow; clip; clip = clip->parent) {
			overflow.push_back(clip);
		}
		for (auto it = overflow.rbegin(); it != overflow.rend(); ++it) {
			push_clip((*it)->shape, (*it)->matrix);
		}

		const Color color(1.0f, 0.56f, 0.10f, 0.95f);
		const Vector2 end = p_box.get_end();
		const real_t stroke = std::min<real_t>(2.0, std::min(p_box.size.x, p_box.size.y) * 0.5);
		for (const Rect2& edge : {
			Rect2(p_box.position.x, p_box.position.y, p_box.size.x, stroke),
			Rect2(p_box.position.x, end.y - stroke, p_box.size.x, stroke),
			Rect2(p_box.position.x, p_box.position.y + stroke, stroke, p_box.size.y - 2 * stroke),
			Rect2(end.x - stroke, p_box.position.y + stroke, stroke, p_box.size.y - 2 * stroke),
		}) {
			if (edge.has_area()) {
				add_command_to(diagnostic_runs, Fill{ edge, color });
			}
		}
		clips.clear();
		clip_material.unref();
		active_geometry = nullptr;
	}

	void HTMLPaintRecorder::index_text(const element::ptr& p_element, const TextShaper& p_shape_text, String& r_separator) {
		if (!p_element || p_element->css().get_display() == display_none) {
			return;
		}
		const auto display = p_element->css().get_display();
		const bool visible = p_element->css().get_visibility() == visibility_visible;
		if (p_element->is_break()) {
			if (visible && !plain_text.is_empty()) {
				if (r_separator == " " || r_separator == "\t") {
					r_separator = String();
				}
				r_separator += "\n";
			}
			return;
		}
		if (display == display_inline_text) {
			std::shared_ptr<render_item> item;
			p_element->run_on_renderers([&](const std::shared_ptr<render_item>& p_item) {
				item = p_item;
				return false;
			});
			if (!visible || !item) {
				return;
			}
			if (p_element->is_white_space()) {
				if (r_separator.is_empty() && !plain_text.is_empty()) {
					r_separator = " ";
				}
				return;
			}
			if (item->skip() || !p_element->parent()) {
				return;
			}
			std::string value;
			p_element->get_text(value);
			if (value == "\t") {
				value = "    ";
			}
			transform_html_text(value, p_element->css().get_text_transform());
			const String text_value = String::utf8(value.c_str());
			const HTMLTextLayout shaped = p_shape_text(*p_element, text_value);
			if (shaped.line.is_null()) {
				return;
			}
			position placement = item->get_placement();
			placement.round();
			const Vector2 point(placement.x, placement.y + shaped.ascent - shaped.line->get_line_ascent());
			auto* candidates = painted_text_by_renderer.getptr(item.get());
			if (!candidates) candidates = painted_text.getptr(point);
			if (candidates) {
				for (Text* text : *candidates) {
					if (text->offset < 0 && text->line == shaped.line) {
						plain_text += r_separator;
						r_separator = String();
						text->offset = plain_text.length();
						plain_text += text_value;
						break;
					}
				}
			}
			return;
		}
		const bool block = !litehtml::is_one_of(display, display_inline, display_inline_block, display_inline_flex,
			display_inline_grid, display_inline_table, display_table_cell);
		const int64_t start = plain_text.length();
		const String preceding_separator = r_separator;
		if (block && start > 0 && !r_separator.begins_with("\n")) {
			r_separator = "\n";
		}
		for (const auto& child : p_element->children()) {
			index_text(child, p_shape_text, r_separator);
		}
		if (plain_text.length() > start) {
			if (block && !r_separator.begins_with("\n")) {
				r_separator = "\n";
			}
			else if (display == display_table_cell && !r_separator.begins_with("\n")) {
				r_separator = "\t";
			}
		} else if (block) {
			r_separator = preceding_separator;
		}
	}

	const std::vector<HTMLPaintRun>& HTMLPaintRecorder::get_paint_runs() const {
		return paint_runs;
	}

	const String& HTMLPaintRecorder::get_plain_text() const {
		return plain_text;
	}

	int64_t HTMLPaintRecorder::hit_test_text(const Vector2& p_point) const {
		const TextRun* closest = nullptr;
		real_t best_y = std::numeric_limits<real_t>::infinity();
		real_t best_x = best_y;
		real_t top = best_y;
		real_t bottom = -best_y;
		for (const TextRun& run : text_runs) {
			if (!run.visible_rect.has_area()) continue;
			const Rect2& rect = run.visible_rect;
			const Vector2 end = rect.get_end();
			top = std::min(top, rect.position.y);
			bottom = std::max(bottom, end.y);
			real_t dy = std::max({ rect.position.y - p_point.y, real_t(0), p_point.y - end.y });
			real_t dx = std::max({ rect.position.x - p_point.x, real_t(0), p_point.x - end.x });
			if ((run.matrix != Transform2D() || !run.clips.empty()) && run.visible_rect.has_point(p_point)) {
				const Vector2 local = run.inverse.xform(p_point);
				if (!run.rect.has_point(local)) {
					const Vector2 edge = local.clamp(run.rect.position, run.rect.get_end());
					const Vector2 delta = (run.matrix.xform(edge) - p_point).abs();
					dx = delta.x;
					dy = delta.y;
				}
				int remaining = MAX_ROUNDED_CLIPS;
				for (auto clip = run.clips.rbegin(); clip != run.clips.rend(); ++clip) {
					if (!clip->contains_point(p_point, use_precise_clip(clip->shape, clip->matrix, remaining))) {
						dy = std::max(dy, real_t(0.01));
					}
				}
			}
			if (dy < best_y || (dy == best_y && dx <= best_x)) {
				closest = &run;
				best_y = dy;
				best_x = dx;
			}
		}
		if (!closest || p_point.y < top) {
			return 0;
		}
		if (p_point.y > bottom) {
			return plain_text.length();
		}
		const real_t x = CLAMP(p_point.x, closest->visible_rect.position.x, closest->visible_rect.get_end().x);
		const Vector2 point(x, CLAMP(p_point.y, closest->visible_rect.position.y, closest->visible_rect.get_end().y));
		const Vector2 local = to_local_point(closest->inverse.xform(point), closest->rect.position);
		return closest->offset + CLAMP(static_cast<int64_t>(closest->line->hit_test(local.x)), int64_t(0), closest->length);
	}

	TypedArray<Rect2> HTMLPaintRecorder::get_selection_rects(int64_t p_from, int64_t p_to) const {
		TypedArray<Rect2> result;
		const Ref<TextServer> text_server = TextServerManager::get_singleton()->get_primary_interface();
		if (text_server.is_null()) {
			return result;
		}
		for (const TextRun& run : text_runs) {
			const int64_t from = std::max<int64_t>(0, p_from - run.offset);
			const int64_t to = std::min(run.length, p_to - run.offset);
			if (from >= to) {
				continue;
			}
			const auto ranges = text_server->shaped_text_get_selection(run.line->get_rid(), from, to);
			for (int64_t i = 0; i < ranges.size(); ++i) {
				const Vector2 range = ranges[i];
				const Rect2 rect = translate_rect(Rect2(range.x, 0, range.y - range.x, run.rect.size.y), run.rect.position);
				const Rect2 visible = run.clips.empty() ? run.matrix.xform(rect).intersection(run.visible_rect) :
					clipped_visual_bounds(rect, run.matrix, run.clips);
				if (visible.has_area()) {
					result.push_back(visible);
				}
			}
		}
		return result;
	}

	Ref<ShaderMaterial> HTMLPaintRecorder::create_material(const RoundedRect* p_shape) {
		if (paint_shader.is_null()) {
			paint_shader.instantiate();
			paint_shader->set_code(PAINT_SHADER_CODE);
		}
		Ref<ShaderMaterial> material;
		material.instantiate();
		material->set_shader(paint_shader);
		PackedVector4Array boxes;
		PackedVector4Array radii_x;
		PackedVector4Array radii_y;
		PackedVector4Array bases;
		PackedVector2Array offsets;
		const Transform2D matrix = active_geometry ? active_geometry->matrix : Transform2D();
		bool affine = false;
		const auto append = [&](const RoundedRect& p_clip, const Transform2D& p_clip_matrix) {
			boxes.push_back(box_uniform(p_clip.rect));
			radii_x.push_back(p_clip.radii_x);
			radii_y.push_back(p_clip.radii_y);
			const Transform2D relative = p_clip_matrix == matrix ? Transform2D() : p_clip_matrix.affine_inverse() * matrix;
			affine |= relative != Transform2D();
			bases.push_back(Vector4(relative[0].x, relative[0].y, relative[1].x, relative[1].y));
			offsets.push_back(relative[2]);
		};
		int remaining = MAX_ROUNDED_CLIPS;
		for (auto clip = clips.rbegin(); clip != clips.rend() && remaining > 0; ++clip) {
			if (use_precise_clip(clip->shape, clip->matrix, remaining)) {
				append(clip->shape, clip->matrix);
			}
		}
		if (p_shape) {
			material->set_shader_parameter("has_shape", true);
			material->set_shader_parameter("shape_box", box_uniform(p_shape->rect));
			material->set_shader_parameter("shape_radii_x", p_shape->radii_x);
			material->set_shader_parameter("shape_radii_y", p_shape->radii_y);
		}
		material->set_shader_parameter("clip_count", boxes.size());
		if (!boxes.is_empty()) {
			boxes.resize(MAX_ROUNDED_CLIPS);
			radii_x.resize(MAX_ROUNDED_CLIPS);
			radii_y.resize(MAX_ROUNDED_CLIPS);
			material->set_shader_parameter("clip_boxes", boxes);
			material->set_shader_parameter("clip_radii_x", radii_x);
			material->set_shader_parameter("clip_radii_y", radii_y);
			if (affine) {
				bases.resize(MAX_ROUNDED_CLIPS);
				offsets.resize(MAX_ROUNDED_CLIPS);
				material->set_shader_parameter("affine_clips", true);
				material->set_shader_parameter("clip_bases", bases);
				material->set_shader_parameter("clip_offsets", offsets);
			}
		}
		return material;
	}

	void HTMLPaintRecorder::add_command(Command p_command, Ref<ShaderMaterial> p_material) {
		add_command_to(paint_runs, std::move(p_command), std::move(p_material));
	}

	void HTMLPaintRecorder::add_command_to(std::vector<HTMLPaintRun>& r_runs, Command p_command, Ref<ShaderMaterial> p_material) {
		if (active_geometry && !active_geometry->inverse) return;
		const Transform2D matrix = active_geometry ? active_geometry->matrix : Transform2D();
		std::optional<Rect2> clip;
		if (!clips.empty()) {
			if (!clips.back().bounds.has_area()) {
				return;
			}
			clip = clips.back().bounds;
			if (p_material.is_null()) {
				if (clips.back().has_precise_clip) {
					if (clip_material.is_null() || clip_material_matrix != matrix) {
						clip_material = create_material();
						clip_material_matrix = matrix;
					}
					p_material = clip_material;
				}
			}
		}
		if (r_runs.empty() || r_runs.back().clip != clip || r_runs.back().matrix != matrix || r_runs.back().material != p_material) {
			r_runs.push_back({ clip, matrix, {}, p_material, {} });
			if (matrix != Transform2D() || (!clips.empty() && clips.back().has_precise_clip)) {
				for (const auto& entry : clips) r_runs.back().clips.push_back({ entry.shape, entry.matrix });
			}
		}
		r_runs.back().commands.push_back(std::move(p_command));
	}

	void HTMLPaintRecorder::fill_rect(const Rect2& p_rect, const Color& p_color) {
		if (p_rect.has_area() && p_color.a > 0.0f) {
			add_command(Fill{ p_rect, p_color });
		}
	}

	void HTMLPaintRecorder::draw_decoration(const Rect2& p_rect, text_decoration_style p_style, const Color& p_color) {
		if (!p_rect.has_area() || !p_rect.is_finite() || p_color.a <= 0.0f) {
			return;
		}
		int pattern;
		switch (p_style) {
			case text_decoration_style_dotted: pattern = 1; break;
			case text_decoration_style_dashed: pattern = 2; break;
			case text_decoration_style_wavy: pattern = 3; break;
			default:
				fill_rect(p_rect, p_color);
				if (p_style == text_decoration_style_double) {
					fill_rect(Rect2(p_rect.position + Vector2(0, p_rect.size.y * 2), p_rect.size), p_color);
				}
				return;
		}
		const auto material = create_material();
		material->set_shader_parameter("decoration_pattern", pattern);
		material->set_shader_parameter("decoration_thickness", p_rect.size.y);
		material->set_shader_parameter("decoration_center", p_rect.get_center().y);
		const real_t margin = pattern == 3 ? p_rect.size.y * 1.5 : 1.0;
		add_command(Fill{ p_rect.grow_side(SIDE_TOP, margin).grow_side(SIDE_BOTTOM, margin), p_color }, material);
	}

	void HTMLPaintRecorder::draw_text(const HTMLShapedText& p_shaped, int64_t p_length, web_color p_color,
		const litehtml::position& p_position, const std::vector<box_shadow>& p_shadows) {
		const Ref<TextLine>& line = p_shaped.layout.line;
		const Vector2 top_left(p_position.x, p_position.y + p_shaped.layout.ascent - line->get_line_ascent());
		const Color color = to_color(p_color);
		const Text text{ line, top_left, color, -1, p_length,
			active_geometry && active_geometry->item->css().get_display() == display_inline_text ? active_geometry->item.get() : nullptr };

		const int decoration = p_shaped.decoration_line;
		if (decoration == text_decoration_line_none && p_shadows.empty()) {
			add_command(text);
			return;
		}
		const float thickness = p_shaped.decoration_thickness;
		const float width = line->get_line_width();
		const float baseline = top_left.y + line->get_line_ascent();
		const web_color css_color = p_shaped.decoration_color;
		const Color decoration_color = css_color.is_current_color ? color : to_color(css_color);
		std::vector<TextDecorationMask> decorations;
		const auto add_decoration = [&](int p_flag, real_t p_top) {
			const Rect2 rect(top_left.x, p_top, width, thickness);
			if ((decoration & p_flag) && rect.has_area() && rect.is_finite() && decoration_color.a > 0.0f) {
				decorations.push_back({ rect, p_shaped.decoration_style, decoration_color.a });
			}
		};
		if (decoration != text_decoration_line_none) {
			add_decoration(text_decoration_line_underline, baseline + line->get_line_underline_position());
			add_decoration(text_decoration_line_overline, top_left.y);
			add_decoration(text_decoration_line_line_through, baseline - line->get_line_ascent() / 3.0f);
		}
		HTMLTextMask mask;
		const bool mipmaps = std::any_of(p_shadows.begin(), p_shadows.end(), [](const box_shadow& p_shadow) {
			return p_shadow.blur.val() > 3.0f;
		});
		for (auto shadow = p_shadows.rbegin(); shadow != p_shadows.rend(); ++shadow) {
			const Color shadow_color = shadow->color.is_current_color ? color : to_color(shadow->color);
			const Vector2 offset(shadow->x.val(), shadow->y.val());
			const float blur = shadow->blur.val();
			if (shadow_color.a <= 0.0f || !offset.is_finite() || !std::isfinite(blur)) {
				continue;
			}
			if (mask.texture.is_null()) {
				mask = create_html_text_mask(*line.ptr(), top_left, decorations, mipmaps);
				if (mask.texture.is_null()) {
					break;
				}
			}
			const Rect2 box(mask.bounds.position + offset, mask.bounds.size);
			Rect2 destination = box.grow(blur * (4.0f / 3.0f));
			if (!destination.is_finite()) {
				continue;
			}
			if (!clips.empty()) {
				const Transform2D inverse = active_geometry && active_geometry->inverse ? *active_geometry->inverse : Transform2D();
				destination = destination.intersection(inverse.xform(clips.back().bounds));
			}
			if (!destination.has_area()) {
				continue;
			}
			const auto material = create_material();
			material->set_shader_parameter("shadow_type", 3);
			material->set_shader_parameter("shadow_blur", blur);
			material->set_shader_parameter("text_shadow_mask", mask.texture);
			material->set_shader_parameter("text_shadow_box", Vector4(box.position.x, box.position.y, box.size.x, box.size.y));
			add_command(Fill{ destination, shadow_color }, material);
		}
		add_command(text);
		for (const auto& line_decoration : decorations) {
			draw_decoration(line_decoration.rect, line_decoration.style, decoration_color);
		}
	}

	void HTMLPaintRecorder::draw_list_marker(const list_marker& p_marker, const Ref<Texture2D>& p_texture) {
		const Rect2 rect = to_rect(p_marker.pos);
		const Color color = to_color(p_marker.color);
		if (p_texture.is_valid()) {
			add_command(Image{ p_texture, rect, Rect2(Vector2(), p_texture->get_size()), false });
			return;
		}
		if (p_marker.marker_type == list_style_type_square) {
			fill_rect(rect, color);
		}
		else if (p_marker.marker_type == list_style_type_disc || p_marker.marker_type == list_style_type_circle) {
			const float radius = std::min(rect.size.x, rect.size.y) / 2.0f;
			add_command(Circle{ rect.get_center(), radius, color, p_marker.marker_type == list_style_type_disc });
		}
	}

	void HTMLPaintRecorder::draw_image(const background_layer& p_layer, const Ref<Texture2D>& p_texture) {
		const Rect2 tile = to_rect(p_layer.origin_box);
		if (p_texture.is_null() || !tile.has_area()) {
			return;
		}

		const bool repeat_x = p_layer.repeat == background_repeat_repeat || p_layer.repeat == background_repeat_repeat_x;
		const bool repeat_y = p_layer.repeat == background_repeat_repeat || p_layer.repeat == background_repeat_repeat_y;
		const Rect2 bounds = get_background_clip(p_layer);
		const Rect2 destination = get_tile_bounds(p_layer, bounds, repeat_x, repeat_y);
		if (destination.has_area()) {
			const Vector2 scale = p_texture->get_size() / tile.size;
			const Rect2 source((destination.position - tile.position) * scale, destination.size * scale);
			const RoundedRect shape = RoundedRect(to_rect(p_layer.border_box), p_layer.border_radius).inset(bounds);
			add_command(Image{ p_texture, destination, source, repeat_x || repeat_y },
				shape.has_radius() ? create_material(&shape) : Ref<ShaderMaterial>());
		}
	}

	void HTMLPaintRecorder::draw_solid_fill(const background_layer& p_layer, const web_color& p_color) {
		const Rect2 bounds = get_background_clip(p_layer);
		const Color color = to_color(p_color);
		if (!bounds.has_area() || color.a <= 0) {
			return;
		}
		const RoundedRect shape = RoundedRect(to_rect(p_layer.border_box), p_layer.border_radius).inset(bounds);
		add_command(Fill{ bounds, color }, shape.has_radius() ? create_material(&shape) : Ref<ShaderMaterial>());
	}

	void HTMLPaintRecorder::draw_gradient(const background_layer& p_layer, const background_layer::gradient_base& p_gradient,
		int p_type, const Vector2& p_start, const Vector2& p_parameters) {
		const Rect2 tile = to_rect(p_layer.origin_box);
		const Rect2 bounds = get_background_clip(p_layer);
		const bool repeat_x = p_layer.repeat == background_repeat_repeat || p_layer.repeat == background_repeat_repeat_x;
		const bool repeat_y = p_layer.repeat == background_repeat_repeat || p_layer.repeat == background_repeat_repeat_y;
		const Rect2 destination = get_tile_bounds(p_layer, bounds, repeat_x, repeat_y);
		const auto& points = p_gradient.color_points;
		if (!tile.has_area() || !destination.has_area() || !p_start.is_finite() || !p_parameters.is_finite() ||
			points.empty() || points.size() > 4096) {
			return;
		}
		const int count = static_cast<int>(points.size());
		Ref<godot::Image> stops = godot::Image::create_empty(count, 2, false, godot::Image::FORMAT_RGBAF);
		float offset = points.front().offset;
		for (int i = 0; i < count; ++i) {
			if (!std::isfinite(points[i].offset)) {
				return;
			}
			offset = std::max(offset, points[i].offset);
			const float span = i + 1 < count ? points[i + 1].offset - offset : 0.0f;
			const float hint = points[i].hint && span > 0 ? (*points[i].hint - offset) / span : 0.0f;
			stops->set_pixel(i, 0, to_color(points[i].color));
			stops->set_pixel(i, 1, Color(offset, hint, 0, 0));
		}
		const Ref<ImageTexture> texture = ImageTexture::create_from_image(stops);
		const RoundedRect shape = RoundedRect(to_rect(p_layer.border_box), p_layer.border_radius).inset(bounds);
		const Ref<ShaderMaterial> material = create_material(shape.has_radius() ? &shape : nullptr);
		material->set_shader_parameter("gradient_type", p_type);
		material->set_shader_parameter("stop_count", count);
		material->set_shader_parameter("gradient_stops", texture);
		material->set_shader_parameter("gradient_start", p_start);
		material->set_shader_parameter("gradient_parameters", p_parameters);
		material->set_shader_parameter("tile", Vector4(tile.position.x, tile.position.y, tile.size.x, tile.size.y));
		material->set_shader_parameter("repeat_axes", Vector2i(repeat_x, repeat_y));
		add_command(Fill{ destination, Color(1, 1, 1) }, material);
	}

	void HTMLPaintRecorder::draw_linear_gradient(const background_layer& p_layer, const background_layer::linear_gradient& p_gradient) {
		draw_gradient(p_layer, p_gradient, 1, Vector2(p_gradient.start.x, p_gradient.start.y),
			Vector2(p_gradient.end.x - p_gradient.start.x, p_gradient.end.y - p_gradient.start.y));
	}

	void HTMLPaintRecorder::draw_radial_gradient(const background_layer& p_layer, const background_layer::radial_gradient& p_gradient) {
		draw_gradient(p_layer, p_gradient, 2, Vector2(p_gradient.position.x, p_gradient.position.y),
			Vector2(p_gradient.radius.x, p_gradient.radius.y));
	}

	void HTMLPaintRecorder::draw_conic_gradient(const background_layer& p_layer, const background_layer::conic_gradient& p_gradient) {
		draw_gradient(p_layer, p_gradient, 3, Vector2(p_gradient.position.x, p_gradient.position.y), Vector2(p_gradient.angle / 360.0f, 0));
	}

	void HTMLPaintRecorder::draw_box_shadows(const html_tag& p_element, const litehtml::position& p_box,
		const borders& p_borders, bool p_inset, const std::vector<box_shadow>& p_shadows) {
		const RoundedRect outer(to_rect(p_box), p_borders.radius);
		const Vector2 leading(p_borders.left.width, p_borders.top.width);
		const Vector2 trailing(p_borders.right.width, p_borders.bottom.width);
		const RoundedRect base = p_inset ? outer.inset(Rect2(outer.rect.position + leading, outer.rect.size - leading - trailing)) : outer;
		if (p_inset && !base.rect.has_area()) {
			return;
		}
		const auto& css = p_element.css();
		const auto doc = p_element.get_document();
		const auto pixels = [&](const css_length& p_length) { return doc->to_pixels(p_length, css.get_font_metrics(), 0); };
		for (auto shadow = p_shadows.rbegin(); shadow != p_shadows.rend(); ++shadow) {
			if (shadow->inset != p_inset || shadow->color.alpha == 0) {
				continue;
			}
			const float blur = pixels(shadow->blur);
			const float spread = pixels(shadow->spread);
			const Vector2 offset(pixels(shadow->x), pixels(shadow->y));
			if (!offset.is_finite() || !std::isfinite(blur) || !std::isfinite(spread) ||
					(blur == 0.0f && spread == 0.0f && offset == Vector2())) {
				continue;
			}
			RoundedRect shape = base.spread(p_inset ? -spread : spread);
			shape.rect.position += offset;
			Rect2 destination = p_inset ? base.rect : shape.rect.grow(blur * 1.75f);
			const Color color = to_color(shadow->color.is_current_color ? css.get_color() : shadow->color);
			if (color.a <= 0.0f || !shape.rect.is_finite() || !destination.is_finite() || (!p_inset && !shape.rect.has_area())) {
				continue;
			}
			if (!clips.empty()) {
				const Transform2D inverse = active_geometry && active_geometry->inverse ? *active_geometry->inverse : Transform2D();
				destination = destination.intersection(inverse.xform(clips.back().bounds));
			}
			if (!destination.has_area()) {
				continue;
			}
			const Ref<ShaderMaterial> material = create_material(p_inset ? &base : nullptr);
			material->set_shader_parameter("shadow_type", p_inset ? 2 : 1);
			material->set_shader_parameter("shadow_box", box_uniform(shape.rect));
			material->set_shader_parameter("shadow_rx", shape.radii_x);
			material->set_shader_parameter("shadow_ry", shape.radii_y);
			material->set_shader_parameter("shadow_blur", blur);
			if (!p_inset) {
				material->set_shader_parameter("inner_box", box_uniform(base.rect));
				material->set_shader_parameter("inner_radii_x", base.radii_x);
				material->set_shader_parameter("inner_radii_y", base.radii_y);
			}
			add_command(Fill{ destination, color }, material);
		}
	}

	void HTMLPaintRecorder::draw_borders(const borders& p_borders, const litehtml::position& p_position) {
		const Rect2 rect = to_rect(p_position);
		if (!rect.has_area()) {
			return;
		}
		const std::array<const border*, 4> sides{ &p_borders.top, &p_borders.right, &p_borders.bottom, &p_borders.left };
		std::array<BorderPaint, 4> styles;
		for (int side = 0; side < 4; ++side) {
			styles[side] = sides[side]->width > 0.0f ? get_border_paint(sides[side]->style) : BORDER_NONE;
		}
		const auto width = [&](int p_side, float p_maximum) {
			return styles[p_side] == BORDER_NONE ? 0.0f : std::clamp(sides[p_side]->width, 0.0f, p_maximum);
		};
		const float top = width(0, rect.size.y);
		const float bottom = width(2, rect.size.y - top);
		const float left = width(3, rect.size.x);
		const float right = width(1, rect.size.x - left);
		if (top + right + bottom + left == 0.0f) {
			return;
		}
		const Vector4 widths(top, right, bottom, left);
		for (int side = 0; side < 4; ++side) {
			if (widths[side] == 0.0f) {
				styles[side] = BORDER_NONE;
			}
		}
		const float inner_height = rect.size.y - top - bottom;
		const Vector2 end = rect.get_end();
		const RoundedRect outer(rect, p_borders.radius);
		const bool patterned = std::any_of(styles.begin(), styles.end(),
			[](BorderPaint p_style) { return p_style == BORDER_DOTTED || p_style == BORDER_DASHED; });
		const bool double_line = std::find(styles.begin(), styles.end(), BORDER_DOUBLE) != styles.end();
		if (outer.has_radius() || patterned || double_line) {
			const auto inset = [&](float p_fraction) {
				const Vector2 leading(left * p_fraction, top * p_fraction);
				const Vector2 trailing(right * p_fraction, bottom * p_fraction);
				return outer.inset(Rect2(rect.position + leading, rect.size - leading - trailing));
			};
			const RoundedRect inner = inset(1.0f);
			const Ref<ShaderMaterial> material = create_material(&outer);
			material->set_shader_parameter("draw_border", true);
			material->set_shader_parameter("inner_box", box_uniform(inner.rect));
			material->set_shader_parameter("inner_radii_x", inner.radii_x);
			material->set_shader_parameter("inner_radii_y", inner.radii_y);
			material->set_shader_parameter("border_widths", widths);
			if (patterned || double_line) {
				material->set_shader_parameter("border_styles", Vector4i(styles[0], styles[1], styles[2], styles[3]));
			}
			if (double_line) {
				const RoundedRect gap_outer = inset(1.0f / 3.0f);
				const RoundedRect gap_inner = inset(2.0f / 3.0f);
				material->set_shader_parameter("double_outer_box", box_uniform(gap_outer.rect));
				material->set_shader_parameter("double_outer_rx", gap_outer.radii_x);
				material->set_shader_parameter("double_outer_ry", gap_outer.radii_y);
				material->set_shader_parameter("double_inner_box", box_uniform(gap_inner.rect));
				material->set_shader_parameter("double_inner_rx", gap_inner.radii_x);
				material->set_shader_parameter("double_inner_ry", gap_inner.radii_y);
			}
			if (patterned) {
				const RoundedRect center = inset(0.5f);
				const auto& rx = center.radii_x;
				const auto& ry = center.radii_y;
				const Vector4 edges = Vector4(center.rect.size.x - rx.x - rx.y, center.rect.size.y - ry.y - ry.z,
					center.rect.size.x - rx.z - rx.w, center.rect.size.y - ry.w - ry.x).max(Vector4());
				const Vector4 arcs(quarter_ellipse_length(rx.y, ry.y), quarter_ellipse_length(rx.z, ry.z),
					quarter_ellipse_length(rx.w, ry.w), quarter_ellipse_length(rx.x, ry.x));
				Vector4 starts;
				float perimeter = 0.0f;
				for (int side = 0; side < 4; ++side) {
					starts[side] = perimeter;
					perimeter += edges[side] + arcs[side];
				}
				const bool closed = outer.has_radius() && top > 0.0f && widths == Vector4(top, top, top, top) &&
					std::all_of(styles.begin() + 1, styles.end(), [&](BorderPaint p_style) { return p_style == styles[0]; }) &&
					std::all_of(sides.begin() + 1, sides.end(), [&](const border* p_side) { return p_side->color == sides[0]->color; });
				Vector4 periods;
				Vector4 origins;
				for (int side = 0; side < 4; ++side) {
					if (styles[side] == BORDER_DOTTED || styles[side] == BORDER_DASHED) {
						const int previous = (side + 3) % 4;
						const float preceding = arcs[previous] * 0.5f;
						const float length = preceding + edges[side] + arcs[side] * 0.5f;
						periods[side] = border_pattern_period(closed ? perimeter : length, widths[side], styles[side]);
						origins[side] = closed ? 0.0f : starts[side] - preceding;
					}
				}
				material->set_shader_parameter("border_center_box", box_uniform(center.rect));
				material->set_shader_parameter("border_center_rx", rx);
				material->set_shader_parameter("border_center_ry", ry);
				material->set_shader_parameter("border_edge_starts", starts);
				material->set_shader_parameter("border_edge_lengths", edges);
				material->set_shader_parameter("border_arc_lengths", arcs);
				material->set_shader_parameter("border_periods", periods);
				material->set_shader_parameter("border_pattern_starts", origins);
				material->set_shader_parameter("border_pattern_continuous", closed);
			}
			const auto color_vector = [](const border& p_border) {
				const Color color = to_color(p_border.color);
				return Vector4(color.r, color.g, color.b, color.a);
			};
			material->set_shader_parameter("border_colors", PackedVector4Array({
				color_vector(p_borders.top), color_vector(p_borders.right), color_vector(p_borders.bottom), color_vector(p_borders.left) }));
			add_command(Fill{ rect, Color(1, 1, 1) }, material);
			return;
		}
		fill_rect(Rect2(rect.position, Vector2(rect.size.x, top)), to_color(p_borders.top.color));
		fill_rect(Rect2(rect.position.x, end.y - bottom, rect.size.x, bottom), to_color(p_borders.bottom.color));
		fill_rect(Rect2(rect.position.x, rect.position.y + top, left, inner_height), to_color(p_borders.left.color));
		fill_rect(Rect2(end.x - right, rect.position.y + top, right, inner_height), to_color(p_borders.right.color));
	}

	void HTMLPaintRecorder::set_clip(const litehtml::position& p_position, const border_radiuses& p_radii) {
		push_clip(RoundedRect(to_rect(p_position), p_radii), active_geometry ? active_geometry->matrix : Transform2D());
	}

	void HTMLPaintRecorder::set_render_item_clip(const litehtml::position& p_position, const border_radiuses& p_radii,
		const render_item& p_item) {
		const auto* geometry = recording_geometry ? recording_geometry->find_renderer(&p_item) : nullptr;
		push_clip(RoundedRect(to_rect(p_position), p_radii), geometry && geometry->inverse ? geometry->matrix : Transform2D());
	}

	bool HTMLPaintRecorder::begin_render_item(const render_item& p_item, const litehtml::position*& r_clip) {
		active_geometry = recording_geometry ? recording_geometry->find_renderer(&p_item) : nullptr;
		if (!active_geometry) return true;
		if (!active_geometry->inverse) return false;
		if (recording_geometry->has_transforms() && !active_geometry->fragments.empty()) {
			painted_elements.push_back(active_geometry);
		}
		if (p_item.parent() && active_geometry->matrix != Transform2D()) r_clip = nullptr;
		return true;
	}

	void HTMLPaintRecorder::end_render_item() {
		active_geometry = nullptr;
	}

	void HTMLPaintRecorder::push_clip(const RoundedRect& p_shape, const Transform2D& p_matrix) {
		const Rect2 bounds = p_matrix.xform(p_shape.rect);
		const bool precise = p_shape.has_radius() || p_matrix != Transform2D() || (!clips.empty() && clips.back().has_precise_clip);
		const Clip clip{ clips.empty() ? bounds : clips.back().bounds.intersection(bounds), p_shape, p_matrix, precise };
		clips.push_back(std::move(clip));
		clip_material.unref();
	}

	void HTMLPaintRecorder::del_clip() {
		if (!clips.empty()) {
			clips.pop_back();
			clip_material.unref();
		}
	}
} // namespace docrender

void docrender::release_html_paint_shader() {
	paint_shader.unref();
}

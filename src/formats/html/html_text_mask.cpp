#include "html_text_mask.h"

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/rect2i.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace godot;
using namespace litehtml;

namespace docrender {
	namespace {
		constexpr int MAX_MASK_SIDE = 8190;
		constexpr int MAX_MASK_PIXELS = 4 * 1024 * 1024;

		struct GlyphMask {
			Rect2 rect;
			Rect2i uv;
			Ref<Image> atlas;
			float msdf_scale;
			bool lcd;
		};

		Rect2 decoration_bounds(const TextDecorationMask& p_decoration) {
			Rect2 bounds = p_decoration.rect;
			const real_t width = bounds.size.y;
			if (p_decoration.style == text_decoration_style_double) {
				bounds.size.y *= 3;
			}
			else if (p_decoration.style == text_decoration_style_wavy) {
				bounds = bounds.grow_side(SIDE_TOP, width * 1.5).grow_side(SIDE_BOTTOM, width * 1.5);
			}
			return bounds;
		}

		float decoration_coverage(const TextDecorationMask& p_decoration, const Vector2& p_position) {
			const float width = p_decoration.rect.size.y;
			const float y = p_position.y - p_decoration.rect.get_center().y;
			float distance = width * 0.5f - std::abs(y);
			switch (p_decoration.style) {
				case text_decoration_style_double:
					distance = width * 0.5f - std::min(std::abs(y), std::abs(y - 2.0f * width));
					break;
				case text_decoration_style_dotted: {
					const float x = Math::fposmod(p_position.x, 2.0f * width) - width;
					distance = width * 0.5f - std::hypot(x, y);
					break;
				}
				case text_decoration_style_dashed: {
					const float x = Math::fposmod(p_position.x, 5.0f * width) - 2.5f * width;
					distance = std::min(distance, 1.5f * width - std::abs(x));
					break;
				}
				case text_decoration_style_wavy: {
					const float phase = Math::fposmod(p_position.x, 6.0f * width) / (6.0f * width) * Math_TAU;
					const float slope = std::cos(phase) * Math_TAU / 6.0f;
					distance = width * 0.5f - std::abs(y - std::sin(phase) * width) / std::hypot(1.0f, slope);
					break;
				}
				default:
					break;
			}
			return std::clamp(distance + 0.5f, 0.0f, 1.0f) * p_decoration.alpha;
		}

		std::vector<GlyphMask> get_glyph_masks(const TextLine& p_line, const Vector2& p_position) {
			const auto server = TextServerManager::get_singleton()->get_primary_interface();
			const auto glyphs = server->shaped_text_get_glyphs(p_line.get_rid());
			std::vector<GlyphMask> masks;
			Vector2 baseline(0, p_line.get_line_ascent());
			for (int64_t index = 0; index < glyphs.size(); ++index) {
				const Dictionary glyph = glyphs[index];
				const RID font = glyph["font_rid"];
				const int64_t id = glyph["index"];
				const int32_t size = glyph["font_size"];
				const int32_t repeat = glyph["repeat"];
				const float advance = glyph["advance"];
				const Vector2 position = baseline + Vector2(glyph["offset"]);
				baseline.x += advance * repeat;
				if (!font.is_valid() || id == 0 || size <= 0) {
					continue;
				}
				const Vector2i cache_size(size, 0);
				const int64_t texture = server->font_get_glyph_texture_idx(font, cache_size, id);
				if (texture < 0) {
					continue;
				}
				const Ref<Image> atlas = server->font_get_texture_image(font, cache_size, texture);
				const Rect2i uv(server->font_get_glyph_uv_rect(font, cache_size, id));
				if (atlas.is_null() || !uv.has_area() || !Rect2i(Vector2i(), atlas->get_size()).encloses(uv)) {
					continue;
				}
				const bool msdf = server->font_is_multichannel_signed_distance_field(font);
				const bool lcd = server->font_get_antialiasing(font) == TextServer::FONT_ANTIALIASING_LCD &&
					atlas->get_format() == Image::FORMAT_RGBA8;
				const double oversampling = server->font_get_oversampling(font);
				const bool fixed_size = server->font_get_fixed_size(font) > 0;
				const double oversampling_factor = msdf || fixed_size || oversampling <= 0.0 ? 1.0 : oversampling;
				const float scale = msdf ? 1.0f : server->font_get_scale(font, size) / oversampling_factor;
				const Vector2 offset = server->font_get_glyph_offset(font, cache_size, id) * scale;
				const Vector2 dimensions = server->font_get_glyph_size(font, cache_size, id) * scale;
				if (!position.is_finite() || !offset.is_finite() || !dimensions.is_finite() || dimensions.x <= 0 || dimensions.y <= 0) {
					continue;
				}
				const float msdf_scale = msdf ? server->font_get_msdf_pixel_range(font) * dimensions.x / uv.size.x : 0.0f;
				for (int32_t i = 0; i < repeat; ++i) {
					Vector2 origin = p_position + position + Vector2(advance * i, 0);
					if (!msdf && scale == 1.0f) {
						origin = origin.floor();
					}
					const Rect2 rect(origin + offset, dimensions);
					if (rect.is_finite()) {
						masks.push_back({ rect, uv, atlas, msdf_scale, lcd });
					}
				}
			}
			return masks;
		}

		float sample_glyph(const GlyphMask& p_glyph, const Vector2& p_point) {
			const Vector2 uv = Vector2(p_glyph.uv.position) + (p_point - p_glyph.rect.position) / p_glyph.rect.size *
				Vector2(p_glyph.uv.size) - Vector2(0.5, 0.5);
			const Vector2i start(uv.floor());
			const Vector2 fraction = uv - Vector2(start);
			if (fraction == Vector2() && p_glyph.msdf_scale == 0.0f && !p_glyph.lcd) {
				return p_glyph.atlas->get_pixelv(start).a;
			}
			const auto pixel = [&](const Vector2i& p_pixel) {
				const Vector2i sample = p_pixel.clamp(p_glyph.uv.position, p_glyph.uv.get_end() - Vector2i(1, 1));
				return p_glyph.atlas->get_pixelv(sample);
			};
			const Color color = pixel(start).lerp(pixel(start + Vector2i(1, 0)), fraction.x).lerp(
				pixel(start + Vector2i(0, 1)).lerp(pixel(start + Vector2i(1, 1)), fraction.x), fraction.y);
			if (p_glyph.msdf_scale > 0.0f) {
				const float median = std::max(std::min(color.r, color.g), std::min(std::max(color.r, color.g), color.b));
				return std::clamp((median - 0.5f) * std::max(p_glyph.msdf_scale, 1.0f) + 0.5f, 0.0f, 1.0f);
			}
			return p_glyph.lcd && p_glyph.atlas->get_format() == Image::FORMAT_RGBA8
				? std::max({ color.r, color.g, color.b }) : color.a;
		}
	}

	HTMLTextMask create_html_text_mask(const TextLine& p_line, const Vector2& p_position,
		const std::vector<TextDecorationMask>& p_decorations, bool p_mipmaps) {
		const auto glyphs = get_glyph_masks(p_line, p_position);
		Rect2 bounds;
		const auto include = [&](const Rect2& p_rect) { bounds = bounds.has_area() ? bounds.merge(p_rect) : p_rect; };
		for (const auto& glyph : glyphs) {
			include(glyph.rect);
		}
		for (const auto& decoration : p_decorations) {
			include(decoration_bounds(decoration));
		}
		if (!bounds.is_finite() || !bounds.has_area() || bounds.size.x > MAX_MASK_SIDE || bounds.size.y > MAX_MASK_SIDE) {
			return {};
		}
		const Vector2 origin = bounds.position.floor() - Vector2(1, 1);
		const Vector2i size(bounds.get_end().ceil() + Vector2(1, 1) - origin);
		if (size.x <= 0 || size.y <= 0 || int64_t(size.x) * size.y > MAX_MASK_PIXELS) {
			return {};
		}
		PackedByteArray data;
		if (data.resize(int64_t(size.x) * size.y * 2) != OK) {
			return {};
		}
		uint8_t* pixels = data.ptrw();
		for (int64_t index = 0; index < data.size(); index += 2) {
			pixels[index] = 255;
			pixels[index + 1] = 0;
		}
		const auto paint = [&](const Rect2& p_rect, const auto& p_coverage) {
			const Vector2i first = Vector2i((p_rect.position - origin).floor()).max(Vector2i());
			const Vector2i last = Vector2i((p_rect.get_end() - origin).ceil()).min(size);
			for (int y = first.y; y < last.y; ++y) {
				for (int x = first.x; x < last.x; ++x) {
					const Vector2 sample = origin + Vector2(x + 0.5f, y + 0.5f);
					if (p_rect.has_point(sample)) {
						uint8_t& alpha = pixels[(y * size.x + x) * 2 + 1];
						alpha = std::lround(alpha + (255 - alpha) * p_coverage(sample));
					}
				}
			}
		};
		for (const auto& glyph : glyphs) {
			paint(glyph.rect, [&](const Vector2& p_sample) { return sample_glyph(glyph, p_sample); });
		}
		for (const auto& decoration : p_decorations) {
			paint(decoration_bounds(decoration), [&](const Vector2& p_sample) { return decoration_coverage(decoration, p_sample); });
		}
		const Ref<Image> image = Image::create_from_data(size.x, size.y, false, Image::FORMAT_LA8, data);
		if (p_mipmaps) {
			image->generate_mipmaps();
		}
		return { ImageTexture::create_from_image(image), Rect2(origin, Vector2(size)) };
	}
}

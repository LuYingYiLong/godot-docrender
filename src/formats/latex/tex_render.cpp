#include "tex_render.h"

#include "gen/microtex_fonts.gen.h"

#include "core/formula.h"
#include "core/macro.h"
#include "fonts/fonts.h"
#include "latex.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/font_file.hpp>
#include <godot_cpp/classes/font_variation.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/text_line.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>
#include <godot_cpp/classes/theme_db.hpp>
#include <godot_cpp/classes/viewport.hpp>

using namespace godot;

namespace {
	std::map<std::string, Ref<FontFile>> math_fonts;
	bool renderer_initialized = false;

	// MicroTeX stores command definitions globally. Keep edits local to each formula.
	class FormulaCommands : private tex::NewCommandMacro {
	public:
		FormulaCommands() {
			std::vector<std::unique_ptr<tex::MacroInfo>> copies;
			copies.reserve(tex::MacroInfo::_commands.size());
			for (const auto& [name, command] : tex::MacroInfo::_commands) {
				if (command == nullptr) {
					commands.emplace(name, nullptr);
					continue;
				}
				const int argument_count = command->_argc;
				const int options_position = command->_posOpts;
				const auto* predefined = dynamic_cast<const tex::PreDefMacro*>(command);
				if (predefined != nullptr) {
					copies.push_back(std::make_unique<tex::PreDefMacro>(*predefined));
				}
				else {
					copies.push_back(std::make_unique<tex::InflationMacroInfo>(_instance, argument_count, options_position));
				}
				commands.emplace(name, copies.back().get());
			}
			commands.swap(tex::MacroInfo::_commands);
			for (auto& copy : copies) {
				copy.release();
			}
		}

		~FormulaCommands() override {
			tex::MacroInfo::_free_();
			commands.swap(tex::MacroInfo::_commands);
			codes.swap(_codes);
			replacements.swap(_replacements);
			_errIfConflict = error_on_conflict;
		}

		FormulaCommands(const FormulaCommands&) = delete;
		FormulaCommands& operator=(const FormulaCommands&) = delete;

	private:
		decltype(tex::MacroInfo::_commands) commands;
		decltype(_codes) codes = _codes;
		decltype(_replacements) replacements = _replacements;
		bool error_on_conflict = _errIfConflict;
	};

	class GodotTexFont : public tex::Font {
	public:
		Ref<godot::Font> resource;
		float size;
		RID glyph_font;

		GodotTexFont(const Ref<godot::Font>& p_resource, float p_size, const RID& p_glyph_font = RID()) :
			resource(p_resource), size(p_size), glyph_font(p_glyph_font) {
		}

		float getSize() const override { return size; }

		tex::sptr<tex::Font> deriveFont(int p_style) const override {
			if (p_style == tex::PLAIN || glyph_font.is_valid()) {
				return std::make_shared<GodotTexFont>(*this);
			}
			Ref<FontVariation> variation;
			variation.instantiate();
			variation->set_base_font(resource);
			if (p_style & tex::BOLD) {
				variation->set_variation_embolden(0.8);
			}
			if (p_style & tex::ITALIC) {
				variation->set_variation_transform(Transform2D(1, 0, -0.2, 1, 0, 0));
			}
			return std::make_shared<GodotTexFont>(variation, size);
		}

		bool operator==(const tex::Font& p_other) const override {
			const auto& other = static_cast<const GodotTexFont&>(p_other);
			return resource == other.resource && size == other.size;
		}

		bool operator!=(const tex::Font& p_other) const override { return !(*this == p_other); }
	};

	class GodotGraphics : public tex::Graphics2D {
	public:
		GodotGraphics(CanvasItem& p_canvas, const Vector2& p_position, float p_oversampling) :
			canvas(p_canvas), transform(0.0f, p_position), oversampling(p_oversampling) {
		}

		void setColor(tex::color p_color) override {
			color = Color(tex::color_r(p_color) / 255.0f, tex::color_g(p_color) / 255.0f,
				tex::color_b(p_color) / 255.0f, tex::color_a(p_color) / 255.0f);
		}

		tex::color getColor() const override { return color.to_argb32(); }
		void setStroke(const tex::Stroke& p_stroke) override { stroke = p_stroke; }
		const tex::Stroke& getStroke() const override { return stroke; }
		void setStrokeWidth(float p_width) override { stroke.lineWidth = p_width; }
		const tex::Font* getFont() const override { return font; }
		void setFont(const tex::Font* p_font) override { font = static_cast<const GodotTexFont*>(p_font); }

		void translate(float p_x, float p_y) override { transform.translate_local(p_x, p_y); }
		void scale(float p_x, float p_y) override { transform = transform.scaled_local(Vector2(p_x, p_y)); }
		void rotate(float p_angle) override { transform = transform * Transform2D(p_angle, Vector2()); }

		void rotate(float p_angle, float p_x, float p_y) override {
			translate(p_x, p_y);
			rotate(p_angle);
			translate(-p_x, -p_y);
		}

		void reset() override { transform = Transform2D(); }
		float sx() const override { return transform[0].length(); }
		float sy() const override { return transform[1].length(); }

		void drawChar(wchar_t p_character, float p_x, float p_y) override {
			if (font == nullptr) {
				return;
			}
			if (!font->glyph_font.is_valid()) {
				drawText(std::wstring(1, p_character), p_x, p_y);
				return;
			}
			const int pixel_size = std::max(1, static_cast<int>(std::lround(font->size * sy())));
			const float glyph_scale = font->size / pixel_size;
			const Ref<TextServer> server = TextServerManager::get_singleton()->get_primary_interface();
			const int64_t glyph = server->font_get_glyph_index(font->glyph_font, pixel_size, p_character, 0);
			canvas.draw_set_transform_matrix(transform.translated_local(Vector2(p_x, p_y)).scaled_local(
				Vector2(glyph_scale, glyph_scale)));
			server->font_draw_glyph(font->glyph_font, canvas.get_canvas_item(), pixel_size, Vector2(), glyph, color, oversampling);
		}

		void drawText(const std::wstring& p_text, float p_x, float p_y) override;

		void draw_text_line(const Ref<TextLine>& p_line, float p_scale, float p_x, float p_y) {
			canvas.draw_set_transform_matrix(transform.translated_local(Vector2(p_x, p_y)).scaled_local(
				Vector2(p_scale, p_scale)));
			p_line->draw(canvas.get_canvas_item(), Vector2(0, -p_line->get_line_ascent()), color, oversampling);
		}

		void drawLine(float p_x1, float p_y1, float p_x2, float p_y2) override {
			const Vector2 from(p_x1, p_y1);
			const Vector2 to(p_x2, p_y2);
			const Vector2 normal = (to - from).orthogonal().normalized();
			const float width = stroke.lineWidth * transform.basis_xform(normal).length();
			canvas.draw_set_transform_matrix(Transform2D());
			canvas.draw_line(transform.xform(from), transform.xform(to), color, width, true);
		}

		void drawRect(float p_x, float p_y, float p_width, float p_height) override {
			canvas.draw_set_transform_matrix(transform);
			canvas.draw_rect(Rect2(p_x, p_y, p_width, p_height), color, false, stroke.lineWidth);
		}

		void fillRect(float p_x, float p_y, float p_width, float p_height) override {
			canvas.draw_set_transform_matrix(transform);
			canvas.draw_rect(Rect2(p_x, p_y, p_width, p_height), color);
		}

		void drawRoundRect(float p_x, float p_y, float p_width, float p_height, float p_rx, float p_ry) override {
			PackedVector2Array points = rounded_rect(Rect2(p_x, p_y, p_width, p_height), Vector2(p_rx, p_ry));
			points.push_back(points[0]);
			canvas.draw_set_transform_matrix(Transform2D());
			canvas.draw_polyline(transform.xform(points), color, stroke.lineWidth * sy(), true);
		}

		void fillRoundRect(float p_x, float p_y, float p_width, float p_height, float p_rx, float p_ry) override {
			canvas.draw_set_transform_matrix(transform);
			canvas.draw_colored_polygon(rounded_rect(Rect2(p_x, p_y, p_width, p_height), Vector2(p_rx, p_ry)), color);
		}

	private:
		CanvasItem& canvas;
		Color color = Color(1, 1, 1);
		tex::Stroke stroke;
		const GodotTexFont* font = nullptr;
		Transform2D transform;
		float oversampling;

		static PackedVector2Array rounded_rect(const Rect2& p_rect, const Vector2& p_radius) {
			const Vector2 radius = p_radius.clamp(Vector2(), p_rect.size * 0.5);
			const Vector2 start = p_rect.position + radius;
			const Vector2 end = p_rect.get_end() - radius;
			const Vector2 centers[] = {
				Vector2(end.x, end.y), Vector2(start.x, end.y),
				Vector2(start.x, start.y), Vector2(end.x, start.y),
			};
			PackedVector2Array points;
			constexpr int arc_segments = 8;
			for (int corner = 0; corner < 4; corner++) {
				for (int segment = 0; segment <= arc_segments; segment++) {
					const double angle = Math_PI * 0.5 * (corner + double(segment) / arc_segments);
					points.push_back(centers[corner] + Vector2(std::cos(angle), std::sin(angle)) * radius);
				}
			}
			return points;
		}
	};

	class GodotTextLayout : public tex::TextLayout {
	public:
		GodotTextLayout(const std::wstring& p_text, const GodotTexFont& p_font) :
			scale(p_font.size / layout_font_size) {
			line.instantiate();
			line->add_string(String(p_text.c_str()), p_font.resource, layout_font_size);
		}

		void getBounds(tex::Rect& r_bounds) override {
			r_bounds = tex::Rect(0, -line->get_line_ascent() * scale, line->get_line_width() * scale,
				(line->get_line_ascent() + line->get_line_descent()) * scale);
		}

		void draw(tex::Graphics2D& p_graphics, float p_x, float p_y) override {
			static_cast<GodotGraphics&>(p_graphics).draw_text_line(line, scale, p_x, p_y);
		}

	private:
		// Shape at a larger size before scaling down to MicroTeX's layout units.
		static constexpr int layout_font_size = 64;
		Ref<TextLine> line;
		float scale;
	};

	void GodotGraphics::drawText(const std::wstring& p_text, float p_x, float p_y) {
		if (font != nullptr) {
			GodotTextLayout(p_text, *font).draw(*this, p_x, p_y);
		}
	}

	void initialize_renderer() {
		if (renderer_initialized) {
			return;
		}
		// Use the built-in symbol tables. Other Unicode text uses Godot's font fallback.
		tex::NewCommandMacro::_init_();
		tex::DefaultTeXFont::_init_();
		tex::DefaultTeXFont::enableMagnification(false);
		tex::TextRenderingBox::_init_();
		renderer_initialized = true;
	}

} // namespace

namespace tex {
	Font* Font::create(const std::string& p_path, float p_size) {
		const std::string filename = p_path.substr(p_path.find_last_of("/\\") + 1);
		auto cached = math_fonts.find(filename);
		if (cached == math_fonts.end()) {
			const auto* font = std::find_if(std::begin(embedded_math_fonts), std::end(embedded_math_fonts),
				[&filename](const EmbeddedMathFont& p_font) { return filename == p_font.name; });
			if (font == std::end(embedded_math_fonts)) {
				throw std::runtime_error("Unknown MicroTeX font: " + filename);
			}
			PackedByteArray bytes;
			bytes.resize(font->size);
			std::memcpy(bytes.ptrw(), font->data, font->size);
			Ref<FontFile> resource;
			resource.instantiate();
			resource->set_data(bytes);
			resource->set_hinting(TextServer::HINTING_NONE);
			resource->set_allow_system_fallback(false);
			cached = math_fonts.emplace(filename, resource).first;
		}
		return new GodotTexFont(cached->second, p_size, cached->second->get_rids()[0]);
	}

	sptr<Font> Font::_create(const std::string&, int p_style, float p_size) {
		return GodotTexFont(ThemeDB::get_singleton()->get_fallback_font(), p_size).deriveFont(p_style);
	}

	sptr<TextLayout> TextLayout::create(const std::wstring& p_text, const sptr<Font>& p_font) {
		return std::make_shared<GodotTextLayout>(p_text, static_cast<const GodotTexFont&>(*p_font));
	}
} // namespace tex

TeXRender::TeXRender() = default;
TeXRender::~TeXRender() = default;

void TeXRender::_bind_methods() {
	BIND_ENUM_CONSTANT(STYLE_DISPLAY);
	BIND_ENUM_CONSTANT(STYLE_DISPLAY_CRAMPED);
	BIND_ENUM_CONSTANT(STYLE_TEXT);
	BIND_ENUM_CONSTANT(STYLE_TEXT_CRAMPED);
	BIND_ENUM_CONSTANT(STYLE_SCRIPT);
	BIND_ENUM_CONSTANT(STYLE_SCRIPT_CRAMPED);
	BIND_ENUM_CONSTANT(STYLE_SCRIPT_SCRIPT);
	BIND_ENUM_CONSTANT(STYLE_SCRIPT_SCRIPT_CRAMPED);

	ClassDB::bind_method(D_METHOD("parse", "text", "text_size", "style", "foreground"), &TeXRender::parse,
		DEFVAL(16.0f), DEFVAL(STYLE_DISPLAY), DEFVAL(Color(1, 1, 1)));
	ClassDB::bind_method(D_METHOD("clear"), &TeXRender::clear);
	ClassDB::bind_method(D_METHOD("is_valid"), &TeXRender::is_valid);
	ClassDB::bind_method(D_METHOD("get_render_error"), &TeXRender::get_render_error);
	ClassDB::bind_method(D_METHOD("get_text_size"), &TeXRender::get_text_size);
	ClassDB::bind_method(D_METHOD("set_text_size", "text_size"), &TeXRender::set_text_size);
	ClassDB::bind_method(D_METHOD("get_foreground"), &TeXRender::get_foreground);
	ClassDB::bind_method(D_METHOD("set_foreground", "color"), &TeXRender::set_foreground);
	ClassDB::bind_method(D_METHOD("get_size"), &TeXRender::get_size);
	ClassDB::bind_method(D_METHOD("get_width"), &TeXRender::get_width);
	ClassDB::bind_method(D_METHOD("get_height"), &TeXRender::get_height);
	ClassDB::bind_method(D_METHOD("get_depth"), &TeXRender::get_depth);
	ClassDB::bind_method(D_METHOD("get_baseline"), &TeXRender::get_baseline);
	ClassDB::bind_method(D_METHOD("get_baseline_offset"), &TeXRender::get_baseline_offset);
	ClassDB::bind_method(D_METHOD("get_insets"), &TeXRender::get_insets);
	ClassDB::bind_method(D_METHOD("set_insets", "insets", "true_values"), &TeXRender::set_insets, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("set_width", "width", "alignment"), &TeXRender::set_width, DEFVAL(HORIZONTAL_ALIGNMENT_LEFT));
	ClassDB::bind_method(D_METHOD("set_height", "height", "alignment"), &TeXRender::set_height, DEFVAL(VERTICAL_ALIGNMENT_TOP));
	ClassDB::bind_method(D_METHOD("draw", "canvas", "position"), &TeXRender::draw, DEFVAL(Vector2()));
}

Error TeXRender::parse(const String& p_text, float p_text_size, Style p_style, const Color& p_foreground) {
	const OS* os = OS::get_singleton();
	ERR_FAIL_COND_V_MSG(os->get_thread_caller_id() != os->get_main_thread_id(), ERR_UNAVAILABLE,
		"TeXRender.parse() must be called on the main thread.");

	clear();
	if (!std::isfinite(p_text_size) || p_text_size <= 0.0f) {
		render_error = "Text size must be a finite positive number.";
		return ERR_INVALID_PARAMETER;
	}
	if (p_style < STYLE_DISPLAY || p_style > STYLE_SCRIPT_SCRIPT_CRAMPED) {
		render_error = "Invalid TeX style.";
		return ERR_INVALID_PARAMETER;
	}
	if (p_text.strip_edges().is_empty()) {
		return OK;
	}

	try {
		initialize_renderer();
		const FormulaCommands commands;
		const CharWideString source = p_text.wide_string();
		tex::Formula formula;
		tex::TeXParser parser(false, std::wstring(source.get_data(), source.length()), &formula);
		parser.parse();
		render.reset(tex::TeXRenderBuilder()
			.setStyle(static_cast<tex::TexStyle>(p_style))
			.setTextSize(p_text_size)
			.setForeground(p_foreground.to_argb32())
			.build(formula));
		foreground = p_foreground;
		return OK;
	}
	catch (const std::exception& error) {
		render_error = String::utf8(error.what());
		return ERR_PARSE_ERROR;
	}
}

void TeXRender::clear() {
	render.reset();
	render_error = String();
	foreground = Color(1, 1, 1);
	minimum_size = Vector2i();
	horizontal_alignment = HORIZONTAL_ALIGNMENT_LEFT;
	vertical_alignment = VERTICAL_ALIGNMENT_TOP;
}

bool TeXRender::is_valid() const {
	return render != nullptr;
}

String TeXRender::get_render_error() const {
	return render_error;
}

float TeXRender::get_text_size() const {
	return render ? render->getTextSize() : 0.0f;
}

void TeXRender::set_text_size(float p_text_size) {
	ERR_FAIL_COND_MSG(!std::isfinite(p_text_size) || p_text_size <= 0.0f, "Text size must be a finite positive number.");
	if (render) {
		render->setTextSize(p_text_size);
	}
}

Color TeXRender::get_foreground() const {
	return foreground;
}

void TeXRender::set_foreground(const Color& p_color) {
	if (render) {
		foreground = p_color;
		render->setForeground(p_color.to_argb32());
	}
}

Vector2i TeXRender::get_size() const {
	return Vector2i(get_width(), get_height());
}

int TeXRender::get_width() const {
	return render ? std::max(render->getWidth(), minimum_size.x) : 0;
}

int TeXRender::get_height() const {
	return render ? std::max(render->getHeight(), minimum_size.y) : 0;
}

int TeXRender::get_depth() const {
	if (!render) {
		return 0;
	}
	const int extra_height = get_height() - render->getHeight();
	const int top_padding = static_cast<int>(int64_t(extra_height) * vertical_alignment / 2);
	return render->getDepth() + (extra_height - top_padding);
}

float TeXRender::get_baseline() const {
	const int height = get_height();
	return height > 0 ? get_baseline_offset() / height : 0.0f;
}

float TeXRender::get_baseline_offset() const {
	if (!render) {
		return 0.0f;
	}
	const int native_height = render->getHeight();
	const float baseline = native_height > 0 ? render->getBaseline() * native_height : 0.0f;
	return baseline + get_layout_offset().y;
}

Vector4i TeXRender::get_insets() const {
	if (!render) {
		return Vector4i();
	}
	const tex::Insets insets = render->getInsets();
	return Vector4i(insets.left, insets.top, insets.right, insets.bottom);
}

void TeXRender::set_insets(const Vector4i& p_insets, bool p_true_values) {
	if (render) {
		// Insets::set keeps the left and top values independent.
		tex::Insets insets;
		insets.set(p_insets.y, p_insets.x, p_insets.w, p_insets.z);
		render->setInsets(insets, p_true_values);
	}
}

void TeXRender::set_width(int p_width, HorizontalAlignment p_alignment) {
	ERR_FAIL_COND_MSG(p_alignment < HORIZONTAL_ALIGNMENT_LEFT || p_alignment > HORIZONTAL_ALIGNMENT_RIGHT,
		"TeXRender width alignment must be left, center, or right.");
	if (render && p_width > get_width()) {
		minimum_size.x = p_width;
		horizontal_alignment = p_alignment;
	}
}

void TeXRender::set_height(int p_height, VerticalAlignment p_alignment) {
	ERR_FAIL_COND_MSG(p_alignment < VERTICAL_ALIGNMENT_TOP || p_alignment > VERTICAL_ALIGNMENT_BOTTOM,
		"TeXRender height alignment must be top, center, or bottom.");
	if (render && p_height > get_height()) {
		minimum_size.y = p_height;
		vertical_alignment = p_alignment;
	}
}

Vector2 TeXRender::get_layout_offset() const {
	const float extra_width = get_width() - render->getWidth();
	const float extra_height = get_height() - render->getHeight();
	// Godot's three supported alignments are ordered start, center, end.
	return Vector2(extra_width * horizontal_alignment * 0.5f, extra_height * vertical_alignment * 0.5f);
}

void TeXRender::draw(CanvasItem* p_canvas, const Vector2& p_position) const {
	draw_internal(p_canvas, p_position, 0.0f);
}

void TeXRender::draw_oversampled(CanvasItem* p_canvas, const Vector2& p_position) const {
	ERR_FAIL_NULL(p_canvas);
	// Sample glyphs at least twice as densely while keeping MicroTeX's layout units.
	const Viewport* viewport = p_canvas->get_viewport();
	const float oversampling = viewport ? std::max(2.0f, viewport->get_oversampling()) : 2.0f;
	draw_internal(p_canvas, p_position, oversampling);
}

void TeXRender::draw_internal(CanvasItem* p_canvas, const Vector2& p_position, float p_oversampling) const {
	ERR_FAIL_NULL(p_canvas);
	if (render) {
		try {
			GodotGraphics graphics(*p_canvas, p_position + get_layout_offset(), p_oversampling);
			render->draw(graphics, 0, 0);
		}
		catch (const std::exception& error) {
			ERR_PRINT(String::utf8(error.what()));
		}
		p_canvas->draw_set_transform_matrix(Transform2D());
	}
}

void TeXRender::shutdown_renderer() {
	if (renderer_initialized) {
		tex::Formula::_predefinedTeXFormulas.clear();
		tex::LaTeX::release();
		math_fonts.clear();
		renderer_initialized = false;
	}
}


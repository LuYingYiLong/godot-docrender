#pragma once

#include <memory>

#include <godot_cpp/classes/ref_counted.hpp>

namespace godot {
	class CanvasItem;
}

namespace tex {
	class TeXRender;
}

class TeXRender : public godot::RefCounted {
	GDCLASS(TeXRender, godot::RefCounted)

public:
	enum Style {
		STYLE_DISPLAY,
		STYLE_DISPLAY_CRAMPED,
		STYLE_TEXT,
		STYLE_TEXT_CRAMPED,
		STYLE_SCRIPT,
		STYLE_SCRIPT_CRAMPED,
		STYLE_SCRIPT_SCRIPT,
		STYLE_SCRIPT_SCRIPT_CRAMPED,
	};

	TeXRender();
	~TeXRender() override;

	godot::Error parse(
		const godot::String& p_text,
		float p_text_size = 16.0f,
		Style p_style = STYLE_DISPLAY,
		const godot::Color& p_foreground = godot::Color(1, 1, 1)
	);																				// 按指定字号、数学样式和前景色解析公式并生成布局
	void clear();																	// 清空公式布局、绘制设置及错误信息
	bool is_valid() const;															// 检查当前是否有可用的公式布局
	godot::String get_render_error() const;											// 返回最近一次解析失败的诊断信息

	float get_text_size() const;													// 返回当前公式字号，单位为像素
	void set_text_size(float p_text_size);											// 调整公式字号，保留现有边距的像素值
	godot::Color get_foreground() const;											// 返回当前设置的公式前景色
	void set_foreground(const godot::Color& p_color);								// 设置公式前景色，公式内显式指定的颜色优先

	godot::Vector2i get_size() const;												// 返回包含边距和对齐留白的布局像素尺寸
	int get_width() const;															// 返回布局总宽度，单位为像素
	int get_height() const;															// 返回布局总高度，单位为像素
	int get_depth() const;															// 返回基线下方的高度，单位为像素
	float get_baseline() const;														// 返回基线位置占布局高度的比例
	float get_baseline_offset() const;												// 返回从布局顶部到基线的像素距离
	godot::Vector4i get_insets() const;												// 按左、上、右、下顺序返回实际边距
	void set_insets(const godot::Vector4i& p_insets, bool p_true_values = false);	// 设置四边边距，可指定是否叠加 MicroTeX 的自动边距
	void set_width(
		int p_width,
		godot::HorizontalAlignment p_alignment = godot::HORIZONTAL_ALIGNMENT_LEFT
	);																				// 设置布局最小宽度和公式的水平对齐方式
	void set_height(
		int p_height,
		godot::VerticalAlignment p_alignment = godot::VERTICAL_ALIGNMENT_TOP
	);																				// 设置布局最小高度和公式的垂直对齐方式

	void draw(
		godot::CanvasItem* p_canvas,
		const godot::Vector2& p_position = godot::Vector2()
	) const;																		// 在画布局部坐标中的指定位置绘制当前公式

	void draw_oversampled(
		godot::CanvasItem* p_canvas,
		const godot::Vector2& p_position
	) const;																		// 以超采样方式绘制公式，输出尺寸与当前布局一致

	static void shutdown_renderer();												// 释放 MicroTeX 的共享渲染资源

protected:
	static void _bind_methods();

private:
	std::unique_ptr<tex::TeXRender> render;
	godot::String render_error;
	godot::Color foreground = godot::Color(1, 1, 1);
	godot::Vector2i minimum_size;
	godot::HorizontalAlignment horizontal_alignment = godot::HORIZONTAL_ALIGNMENT_LEFT;
	godot::VerticalAlignment vertical_alignment = godot::VERTICAL_ALIGNMENT_TOP;

	void draw_internal(godot::CanvasItem* p_canvas, const godot::Vector2& p_position, float p_oversampling) const;
	godot::Vector2 get_layout_offset() const;
};

VARIANT_ENUM_CAST(TeXRender::Style);

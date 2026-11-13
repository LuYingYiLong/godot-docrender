#pragma once

#include <godot_cpp/classes/image_texture.hpp>

class PDFTexture : public godot::ImageTexture {
	GDCLASS(PDFTexture, godot::ImageTexture)

public:
	static constexpr int32_t DEFAULT_DPI = 144;

	int32_t get_page() const;														// 返回纹理对应的 PDF 页码，从 1 开始
	int32_t get_dpi() const;														// 返回生成纹理时使用的 DPI
	void set_page(int32_t p_page);													// 记录导入页码，从 1 开始
	void set_dpi(int32_t p_dpi);													// 记录导入时使用的 DPI

protected:
	static void _bind_methods();

private:
	int32_t page = 1;
	int32_t dpi = DEFAULT_DPI;
};

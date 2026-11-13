#pragma once

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/typed_array.hpp>

#include <memory>

namespace godot {
	class PDFReader : public RefCounted {
		GDCLASS(PDFReader, RefCounted)

	public:
		enum RenderFlags {
			RENDER_ANNOTATIONS = 1 << 0,
			RENDER_GRAYSCALE = 1 << 1,
			RENDER_FORM_WIDGETS = 1 << 2,
			RENDER_PRINTING = 1 << 3,
			RENDER_LCD_TEXT = 1 << 4,
			RENDER_NO_NATIVE_TEXT = 1 << 5,
			RENDER_LIMITED_IMAGE_CACHE = 1 << 6,
			RENDER_FORCE_HALFTONE = 1 << 7,
			RENDER_NO_SMOOTH_TEXT = 1 << 8,
			RENDER_NO_SMOOTH_IMAGE = 1 << 9,
			RENDER_NO_SMOOTH_PATH = 1 << 10,
		};

		enum FindFlags {
			FIND_MATCH_CASE = 1 << 0,
			FIND_WHOLE_WORD = 1 << 1,
			FIND_CONSECUTIVE = 1 << 2,
		};

		PDFReader();
		~PDFReader() override;

		static bool is_supported();													// 检查当前构建是否编入 PDFium 后端
		static void shutdown_backend();												// 释放 PDFium 后端的共享实例

		Error open_file(const String& p_path, const String& p_password = {});		// 按路径和可选密码打开 PDF，失败时保留原文档
		Error open_buffer(
			const PackedByteArray& p_bytes,
			const String& p_password = {}
		);																			// 从字节缓冲打开 PDF，文档打开期间保留输入字节
		void close();																// 关闭当前文档并清除错误状态
		bool is_open() const;														// 检查当前实例是否已打开文档
		static bool is_available();													// 检查是否可以加载 PDFium 核心库
		static PackedStringArray get_capabilities();								// 返回当前 PDFium 后端可用的读取能力标识
		Error get_last_error() const;												// 返回本实例最近一次操作的错误码
		String get_last_error_message() const;										// 返回最近一次操作的诊断信息
		Dictionary get_document_info();												// 返回 PDF 版本、权限、文件标识和查看器偏好等信息
		Dictionary get_metadata();													// 读取标题、作者和创建时间等文档元数据
		int64_t get_page_count();													// 返回当前文档的页数
		Dictionary get_page_info(int64_t p_page);									// 按从 0 开始的索引返回页面尺寸、旋转、标签和页面框
		Ref<Image> render_page(
			int64_t p_page,
			int64_t p_dpi = 144,
			int64_t p_flags = RENDER_ANNOTATIONS,
			int64_t p_rotation = 0,
			const Color& p_background = Color(1, 1, 1, 1)
		);																			// 按指定 DPI、旋转和背景色将整页渲染为 RGBA8 图像
		Ref<Image> render_region(
			int64_t p_page,
			const Rect2i& p_region,
			int64_t p_dpi = 144,
			int64_t p_flags = RENDER_ANNOTATIONS,
			int64_t p_rotation = 0,
			const Color& p_background = Color(1, 1, 1, 1)
		);																			// 按整页位图左上角为原点的像素坐标渲染指定区域
		Ref<Image> get_page_thumbnail(int64_t p_page);								// 读取页面自带的缩略图，缺失时返回空引用
		Vector2i page_to_pixel(
			int64_t p_page,
			const Vector2& p_position,
			int64_t p_dpi = 144,
			int64_t p_rotation = 0
		);																			// 按给定 DPI 和旋转将 PDF 点坐标转换为整页位图像素坐标
		Vector2 pixel_to_page(
			int64_t p_page,
			const Vector2i& p_position,
			int64_t p_dpi = 144,
			int64_t p_rotation = 0
		);																			// 按给定 DPI 和旋转将整页位图像素坐标转换为 PDF 点坐标
		int64_t get_page_char_count(int64_t p_page);								// 统计页面中的 PDFium 字符索引，包括生成的分隔符
		String get_page_text(
			int64_t p_page,
			int64_t p_start = 0,
			int64_t p_count = -1
		);																			// 按字符范围读取页面文本，数量为 -1 时读取到末尾
		String get_page_text_in_rect(int64_t p_page, const Rect2& p_rect);			// 读取指定 PDF 点坐标矩形内的 Unicode 文本
		Dictionary get_char_info(int64_t p_page, int64_t p_index);					// 读取指定字符的 Unicode、字体、边界和变换等信息
		TypedArray<Rect2> get_text_rects(
			int64_t p_page,
			int64_t p_start = 0,
			int64_t p_count = -1
		);																			// 返回字符范围在 PDF 点坐标中的文本矩形
		int64_t hit_test_text(
			int64_t p_page,
			const Vector2& p_position,
			const Vector2& p_tolerance = Vector2(2, 2)
		);																			// 在给定容差内查找字符索引，未命中或出错时返回 -1
		TypedArray<Dictionary> find_text(
			int64_t p_page,
			const String& p_query,
			int64_t p_flags = 0
		);																			// 搜索页面文本并返回匹配的字符范围和矩形
		TypedArray<Dictionary> get_outline();										// 返回文档书签树以及对应的跳转目标和动作
		TypedArray<Dictionary> get_named_destinations();							// 读取文档中的命名跳转目标
		TypedArray<Dictionary> get_page_links(int64_t p_page);						// 返回页面显式链接的区域、跳转目标和动作
		TypedArray<Dictionary> get_page_web_links(int64_t p_page);					// 提取页面文本中识别到的 URL、字符范围和矩形
		TypedArray<Dictionary> get_page_annotations(int64_t p_page);				// 返回页面批注的类型、内容、颜色和几何信息
		TypedArray<Dictionary> get_form_fields(int64_t p_page);						// 读取页面上的 AcroForm 字段、值和选项
		String get_annotation_string(
			int64_t p_page,
			int64_t p_index,
			const String& p_key
		);																			// 按键读取指定批注的字符串或名称值
		TypedArray<Dictionary> get_attachments();									// 列出文档附件及其名称、类型和大小等信息
		PackedByteArray get_attachment_data(int64_t p_index);						// 返回指定文档附件解码后的字节
		TypedArray<Dictionary> get_signatures();									// 列出签名及其字节范围、时间、原因和权限
		PackedByteArray get_signature_contents(int64_t p_index);					// 返回指定签名的原始内容字节
		TypedArray<Dictionary> get_page_structure(int64_t p_page);					// 返回页面的标签结构树及其属性
		TypedArray<Dictionary> get_page_objects(int64_t p_page);					// 返回页面对象树，包含对象路径、类型、几何和样式
		Ref<Image> get_page_object_image(
			int64_t p_page,
			const PackedInt32Array& p_path,
			bool p_rendered = false
		);																			// 按对象路径读取 RGBA8 图像，可应用图像矩阵和蒙版
		PackedByteArray get_page_object_image_data(
			int64_t p_page,
			const PackedInt32Array& p_path,
			bool p_decoded = false
		);																			// 按对象路径读取图像流字节，可应用 PDF 流过滤器解码
		PackedByteArray get_page_object_font_data(
			int64_t p_page,
			const PackedInt32Array& p_path
		);																			// 读取指定文本对象使用的字体程序字节
		TypedArray<Dictionary> get_javascript_actions();							// 读取文档 JavaScript 动作的名称和脚本文本
		TypedArray<Dictionary> get_xfa_packets();									// 读取内嵌 XFA 数据包的名称与字节

	protected:
		static void _bind_methods();

	private:
		friend class PDFWriter;
		struct State;
		std::unique_ptr<State> state;
		Error open_bytes(PackedByteArray p_bytes, const String& p_password, const String& p_source);

		Ref<Image> render(int64_t p_page, const Rect2i* p_region, int64_t p_dpi, int64_t p_flags,
			int64_t p_rotation, const Color& p_background);
	};
} // namespace godot

VARIANT_ENUM_CAST(godot::PDFReader::RenderFlags);
VARIANT_ENUM_CAST(godot::PDFReader::FindFlags);

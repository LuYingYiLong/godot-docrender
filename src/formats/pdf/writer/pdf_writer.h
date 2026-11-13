#pragma once

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>

#include <memory>

namespace godot {
	class PDFReader;

	class PDFWriter : public RefCounted {
		GDCLASS(PDFWriter, RefCounted)

	public:
		enum SaveFlags {
			SAVE_FULL = 0,
			SAVE_INCREMENTAL = 1,
			SAVE_REMOVE_SECURITY = 4,
			SAVE_SUBSET_NEW_FONTS = 8,
		};

		enum PageBox {
			BOX_MEDIA,
			BOX_CROP,
			BOX_BLEED,
			BOX_TRIM,
			BOX_ART,
		};

		enum FontType {
			FONT_TYPE1 = 1,
			FONT_TRUETYPE = 2,
		};

		enum FillMode {
			FILL_NONE,
			FILL_EVEN_ODD,
			FILL_WINDING,
		};

		enum LineCap {
			CAP_BUTT,
			CAP_ROUND,
			CAP_SQUARE,
		};

		enum LineJoin {
			JOIN_MITER,
			JOIN_ROUND,
			JOIN_BEVEL,
		};

		enum TextRenderMode {
			TEXT_FILL,
			TEXT_STROKE,
			TEXT_FILL_STROKE,
			TEXT_INVISIBLE,
			TEXT_FILL_CLIP,
			TEXT_STROKE_CLIP,
			TEXT_FILL_STROKE_CLIP,
			TEXT_CLIP,
		};

		enum AnnotationSubtype {
			ANNOT_TEXT = 1,
			ANNOT_LINK = 2,
			ANNOT_FREE_TEXT = 3,
			ANNOT_LINE = 4,
			ANNOT_SQUARE = 5,
			ANNOT_CIRCLE = 6,
			ANNOT_HIGHLIGHT = 9,
			ANNOT_UNDERLINE = 10,
			ANNOT_SQUIGGLY = 11,
			ANNOT_STRIKEOUT = 12,
			ANNOT_STAMP = 13,
			ANNOT_INK = 15,
			ANNOT_POPUP = 16,
			ANNOT_FILE_ATTACHMENT = 17,
			ANNOT_WIDGET = 20,
		};

		enum AnnotationFlags {
			ANNOT_FLAG_INVISIBLE = 1,
			ANNOT_FLAG_HIDDEN = 2,
			ANNOT_FLAG_PRINT = 4,
			ANNOT_FLAG_NO_ZOOM = 8,
			ANNOT_FLAG_NO_ROTATE = 16,
			ANNOT_FLAG_NO_VIEW = 32,
			ANNOT_FLAG_READ_ONLY = 64,
			ANNOT_FLAG_LOCKED = 128,
			ANNOT_FLAG_TOGGLE_NO_VIEW = 256,
		};

		enum AppearanceMode {
			APPEARANCE_NORMAL,
			APPEARANCE_ROLLOVER,
			APPEARANCE_DOWN,
		};

		enum FlattenMode {
			FLATTEN_DISPLAY,
			FLATTEN_PRINT,
		};

		PDFWriter();
		~PDFWriter() override;

		Error create_document();													// 创建空白 PDF，成功后替换当前文档
		Error open_file(const String& p_path, const String& p_password = {});		// 按路径和可选密码打开待编辑的 PDF，失败时保留原文档
		Error open_buffer(
			const PackedByteArray& p_bytes,
			const String& p_password = {}
		);																			// 从字节缓冲打开待编辑的 PDF，并保留输入供增量保存使用
		void close();																// 关闭文档并丢弃未保存的修改
		bool is_open() const;														// 检查当前实例是否已打开文档
		static bool is_available();													// 检查 PDFium 后端是否支持创建、打开和保存文档
		static PackedStringArray get_capabilities();								// 返回当前 PDFium 后端可用的写入能力标识
		Error get_last_error() const;												// 返回本实例最近一次操作的错误码
		String get_last_error_message() const;										// 返回最近一次操作的诊断信息
		int64_t get_page_count();													// 返回当前文档的页数
		int64_t insert_page(const Vector2& p_size_pt, int64_t p_at = -1);			// 按指定点尺寸插入页面并返回页索引，位置为 -1 时追加
		Error delete_page(int64_t p_page_index);									// 删除指定索引的页面
		Error move_pages(const PackedInt32Array& p_indices, int64_t p_at);			// 按给定顺序移动页面，目标位置以移出后的页面列表为准
		Error import_pages(
			const Ref<PDFReader>& p_reader,
			const PackedInt32Array& p_indices = {},
			int64_t p_at = -1
		);																			// 从 PDFReader 按索引复制页面，空数组表示复制全部页面
		Error import_page_range(
			const Ref<PDFReader>& p_reader,
			const String& p_range,
			int64_t p_at = -1
		);																			// 按从 1 开始的 PDFium 页码范围复制页面
		Error create_n_up(
			const Ref<PDFReader>& p_reader,
			const Vector2& p_size_pt,
			int64_t p_columns,
			int64_t p_rows
		);																			// 将来源页面按指定行列拼版，成功后替换当前文档
		int64_t create_page_stamp(
			const Ref<PDFReader>& p_reader,
			int64_t p_page_index
		);																			// 从来源页创建未插入的矢量 Form 对象并返回对象 ID
		Error set_page_box(
			int64_t p_page_index,
			int64_t p_box,
			const Rect2& p_rect
		);																			// 设置指定页面框在 PDF 点坐标中的矩形
		Error set_page_rotation(int64_t p_page_index, int64_t p_degrees);			// 设置页面顺时针旋转角度，支持 0、90、180 和 270 度
		Error transform_page(
			int64_t p_page_index,
			const Transform2D& p_transform,
			const Variant& p_clip = {}
		);																			// 变换页面内容，可同时应用矩形裁剪
		Error clip_page(int64_t p_page_index, const Rect2& p_rect);					// 将页面内容裁剪到指定矩形
		Error transform_page_annotations(
			int64_t p_page_index,
			const Transform2D& p_transform
		);																			// 对指定页面的所有批注应用变换
		Error generate_page_content(int64_t p_page_index);							// 为指定页面生成 PDF 内容流
		int64_t get_object_id(
			int64_t p_page_index,
			const PackedInt32Array& p_object_path
		);																			// 按从 0 开始的页面及嵌套对象索引路径获取对象 ID
		int64_t get_object_child_count(int64_t p_object_id);						// 返回指定 Form 对象的子对象数量
		int64_t get_object_child_id(int64_t p_object_id, int64_t p_index);			// 按索引获取 Form 子对象的 ID
		Error insert_object(
			int64_t p_page_index,
			int64_t p_object_id,
			int64_t p_at = -1
		);																			// 将未附着的对象插入页面，位置为 -1 时追加
		Error detach_object(int64_t p_object_id);									// 将对象从页面或父 Form 分离，保留其 ID 供重新插入
		Error remove_object(int64_t p_object_id);									// 移除并销毁指定页面对象，也可销毁尚未插入的对象
		Error set_object_matrix(
			int64_t p_object_id,
			const Transform2D& p_transform
		);																			// 替换指定对象的变换矩阵
		Error transform_object(int64_t p_object_id, const Transform2D& p_transform);// 对指定对象应用变换
		Error transform_object_clip(
			int64_t p_object_id,
			const Transform2D& p_transform
		);																			// 对指定对象的裁剪路径应用变换
		Error set_object_active(int64_t p_object_id, bool p_active);				// 设置指定对象是否参与绘制
		Error set_object_blend_mode(int64_t p_object_id, const String& p_mode);		// 按 PDF 混合模式名称设置对象的混合方式
		Error set_object_fill_color(int64_t p_object_id, const Color& p_color);		// 设置对象的填充颜色
		Error set_object_stroke_color(int64_t p_object_id, const Color& p_color);	// 设置对象的描边颜色
		Error set_object_stroke_width(int64_t p_object_id, float p_width);			// 设置对象的描边宽度，单位为点
		Error set_object_line_cap(int64_t p_object_id, int64_t p_cap);				// 设置对象描边的端点样式
		Error set_object_line_join(int64_t p_object_id, int64_t p_join);			// 设置对象描边的拐角连接样式
		Error set_object_dash(
			int64_t p_object_id,
			const PackedFloat32Array& p_lengths,
			float p_phase = 0.0f
		);																			// 设置虚线段长度和相位，空数组表示实线
		int64_t create_path(const Vector2& p_start);								// 从指定起点创建未插入的路径对象并返回 ID
		int64_t create_rectangle(const Rect2& p_rect);								// 按指定矩形创建未插入的路径对象并返回 ID
		Error path_move_to(int64_t p_object_id, const Vector2& p_point);			// 移动到指定点并开始新的子路径
		Error path_line_to(int64_t p_object_id, const Vector2& p_point);			// 从当前路径端点添加一段直线
		Error path_bezier_to(
			int64_t p_object_id,
			const Vector2& p_control_a,
			const Vector2& p_control_b,
			const Vector2& p_end
		);																			// 按两个控制点和终点添加三次贝塞尔曲线
		Error path_close(int64_t p_object_id);										// 闭合当前子路径
		Error set_path_draw_mode(
			int64_t p_object_id,
			int64_t p_fill_mode,
			bool p_stroke
		);																			// 设置路径填充规则和是否描边
		int64_t add_object_mark(int64_t p_object_id, const String& p_name);			// 为对象添加命名内容标记并返回标记索引
		Error share_object_mark(
			int64_t p_object_id,
			int64_t p_source_object_id,
			int64_t p_mark_index
		);																			// 将来源对象的指定内容标记共享给目标对象
		Error remove_object_mark(int64_t p_object_id, int64_t p_mark_index);		// 移除对象的指定内容标记，后续标记索引可能变化
		Error set_object_mark_param(
			int64_t p_object_id,
			int64_t p_mark_index,
			const String& p_key,
			const Variant& p_value
		);																			// 设置内容标记的整数、浮点数、字符串或字节数组参数
		Error remove_object_mark_param(
			int64_t p_object_id,
			int64_t p_mark_index,
			const String& p_key
		);																			// 按键移除内容标记参数
		int64_t load_standard_font(const String& p_name);							// 按名称加载 PDF 标准字体并返回字体 ID
		int64_t load_font(
			const PackedByteArray& p_data,
			int64_t p_type,
			bool p_cid = false
		);																			// 从字节加载指定类型的字体，可创建 CID 复合字体
		int64_t load_cid_type2_font(
			const PackedByteArray& p_data,
			const String& p_to_unicode,
			const PackedByteArray& p_cid_to_gid
		);																			// 用字体字节、ToUnicode CMap 和 CID 到 GID 映射加载 CID Type 2 字体
		Error close_font(int64_t p_font_id);										// 释放字体 ID，已有文本对象继续持有字体
		int64_t create_text(
			int64_t p_font_id,
			float p_font_size,
			const String& p_text
		);																			// 按字体、字号和文本创建未插入的文本对象并返回 ID
		Error set_text(int64_t p_object_id, const String& p_text);					// 用非空字符串替换文本对象的内容
		Error set_text_charcodes(
			int64_t p_object_id,
			const PackedInt64Array& p_charcodes
		);																			// 用字体专用的无符号 32 位字符编码替换文本
		Error set_text_positions(
			int64_t p_object_id,
			const PackedFloat32Array& p_positions
		);																			// 设置首字符之后各字符的相对位置，单位为点
		Error set_text_font_size(int64_t p_object_id, float p_size);				// 设置文本对象的字号，单位为点
		Error set_text_render_mode(int64_t p_object_id, int64_t p_mode);			// 设置文本对象的 PDF 渲染模式
		int64_t create_image(const Ref<Image>& p_image);							// 从 Godot Image 创建未插入的图像对象并返回 ID
		int64_t create_jpeg(const PackedByteArray& p_bytes);						// 从 JPEG 字节创建未插入的图像对象并返回 ID
		Error set_image(int64_t p_object_id, const Ref<Image>& p_image);			// 替换图像对象的像素并保留现有变换
		Error set_jpeg(int64_t p_object_id, const PackedByteArray& p_bytes);		// 用 JPEG 字节替换图像对象内容并保留现有变换
		PackedInt32Array get_supported_annotation_subtypes();						// 返回当前 PDFium 后端支持创建的批注子类型
		int64_t create_annotation(int64_t p_page_index, int64_t p_subtype);			// 在指定页面创建对应子类型的批注并返回批注 ID
		int64_t get_annotation_id(int64_t p_page_index, int64_t p_annotation_index);// 按页面和批注索引获取批注 ID
		Error remove_annotation(int64_t p_annotation_id);							// 移除指定批注并使其外观对象 ID 失效
		Error set_annotation_rect(int64_t p_annotation_id, const Rect2& p_rect);	// 设置批注在 PDF 点坐标中的矩形
		Error set_annotation_color(
			int64_t p_annotation_id,
			const Color& p_color,
			bool p_interior = false
		);																			// 设置批注颜色，可指定修改内部填充色
		Error set_annotation_border(
			int64_t p_annotation_id,
			const Vector2& p_radii,
			float p_width
		);																			// 设置批注边框的水平半径、垂直半径和宽度
		Error set_annotation_flags(int64_t p_annotation_id, int64_t p_flags);		// 设置批注的可见性、打印和锁定等标志
		Error set_annotation_string(
			int64_t p_annotation_id,
			const String& p_key,
			const String& p_value
		);																			// 按键写入批注字典的字符串字段
		Error set_annotation_quad(
			int64_t p_annotation_id,
			int64_t p_index,
			const PackedVector2Array& p_points
		);																			// 用四个点替换批注的指定四边形
		Error append_annotation_quad(
			int64_t p_annotation_id,
			const PackedVector2Array& p_points
		);																			// 给批注追加由四个点定义的四边形
		int64_t add_annotation_ink_stroke(
			int64_t p_annotation_id,
			const PackedVector2Array& p_points
		);																			// 为墨迹批注添加笔划并返回笔划索引
		Error clear_annotation_ink(int64_t p_annotation_id);						// 清空批注的墨迹笔划
		Error set_annotation_uri(int64_t p_annotation_id, const String& p_uri);		// 为链接批注设置 ASCII URI 跳转动作
		Error set_annotation_file_attachment(
			int64_t p_annotation_id,
			const String& p_name,
			const PackedByteArray& p_bytes
		);																			// 为批注添加附件，已有附件时替换其字节并保留名称
		Error set_annotation_appearance(
			int64_t p_annotation_id,
			int64_t p_mode,
			const Variant& p_appearance
		);																			// 设置指定模式的批注外观流，传入 null 时清除
		Error set_annotation_font_color(
			int64_t p_annotation_id,
			const Color& p_color
		);																			// 设置自由文本批注的字体颜色
		int64_t get_annotation_object_count(int64_t p_annotation_id);				// 返回批注外观中的对象数量
		int64_t get_annotation_object_id(int64_t p_annotation_id, int64_t p_index); // 按索引获取批注外观对象的 ID
		Error append_annotation_object(
			int64_t p_annotation_id,
			int64_t p_object_id
		);																			// 将未附着的路径、文本或图像追加到批注外观
		Error remove_annotation_object(int64_t p_annotation_id, int64_t p_index);	// 按索引移除并销毁批注外观对象
		Error set_form_text(int64_t p_annotation_id, const String& p_text);			// 替换现有 AcroForm 字段的文本并提交修改
		Error set_form_option_selected(
			int64_t p_annotation_id,
			int64_t p_option_index,
			bool p_selected = true
		);																			// 设置 AcroForm 选择字段中指定选项的选中状态
		Error set_form_checked(int64_t p_annotation_id, bool p_checked);			// 设置现有 AcroForm 复选框的选中状态
		Error select_form_radio(int64_t p_annotation_id);							// 选中指定的 AcroForm 单选按钮
		Error set_form_field_flags(int64_t p_annotation_id, int64_t p_flags);		// 设置 AcroForm 字段标志
		Error flatten_page(int64_t p_page_index, int64_t p_mode = 0);				// 按显示或打印模式将支持的批注和表单外观合并到页面内容
		int64_t add_attachment(
			const String& p_name,
			const PackedByteArray& p_bytes
		);																			// 添加命名文档附件并返回附件索引
		Error delete_attachment(int64_t p_index);									// 按索引删除文档附件
		Error set_attachment_data(int64_t p_index, const PackedByteArray& p_bytes); // 替换文档附件的字节内容
		Error set_attachment_description(
			int64_t p_index,
			const String& p_description
		);																			// 设置文档附件的描述
		Error set_attachment_string(
			int64_t p_index,
			const String& p_key,
			const String& p_value
		);																			// 设置文档附件中 PDFium 支持的字符串属性
		Error set_document_language(const String& p_language);						// 设置 PDF 文档目录的语言标记
		Error copy_viewer_preferences(const Ref<PDFReader>& p_reader);				// 从打开的 PDFReader 复制查看器偏好
		PackedByteArray save_to_buffer(int64_t p_flags = 0, int64_t p_version = 0); // 按指定保存标志和版本输出 PDF 字节，失败时返回空数组
		Error save_to_file(
			const String& p_path,
			int64_t p_flags = 0,
			int64_t p_version = 0
		);																			// 保存 PDF，写入成功后替换目标文件

	protected:
		static void _bind_methods();

	private:
		struct State;
		std::unique_ptr<State> state;

		Error open_bytes(PackedByteArray p_bytes, const String& p_password, const String& p_source);
	};
} // namespace godot

VARIANT_ENUM_CAST(godot::PDFWriter::SaveFlags);
VARIANT_ENUM_CAST(godot::PDFWriter::PageBox);
VARIANT_ENUM_CAST(godot::PDFWriter::FontType);
VARIANT_ENUM_CAST(godot::PDFWriter::FillMode);
VARIANT_ENUM_CAST(godot::PDFWriter::LineCap);
VARIANT_ENUM_CAST(godot::PDFWriter::LineJoin);
VARIANT_ENUM_CAST(godot::PDFWriter::TextRenderMode);
VARIANT_ENUM_CAST(godot::PDFWriter::AnnotationSubtype);
VARIANT_ENUM_CAST(godot::PDFWriter::AnnotationFlags);
VARIANT_ENUM_CAST(godot::PDFWriter::AppearanceMode);
VARIANT_ENUM_CAST(godot::PDFWriter::FlattenMode);

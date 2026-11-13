#pragma once

#include "pdf_writer.h"
#include "formats/pdf/pdfium/pdfium_backend.h"

#ifdef DOCRENDER_PDFIUM_ENABLED
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/char_string.hpp>

#include <unordered_map>
#include <vector>
#endif

namespace godot {
	struct PDFWriter::State {
		Error error = OK;
		String message;

		Error fail(Error p_error, const String& p_message);							// 记录并返回错误码，同时保存诊断信息
		Error unavailable();														// 记录并返回当前构建缺少 PDFium 支持的错误
		void clear_error();															// 清除上一次操作的错误状态

#ifdef DOCRENDER_PDFIUM_ENABLED
		struct EditablePage {
			FPDF_PAGE handle;
			bool dirty = false;
		};

		struct Object {
			FPDF_PAGEOBJECT handle;
			int page = -1;
			int64_t parent = 0;
			int64_t annotation = 0;

			bool is_detached() const {
				return page < 0 && parent == 0 && annotation == 0;
			}																		// 检查对象是否独立于页面、父 Form 和批注外观
		};

		struct Annotation {
			FPDF_ANNOTATION handle;
			int page;
		};

		std::shared_ptr<pdfium::Api> api;
		PackedByteArray input;
		FPDF_DOCUMENT document = nullptr;
		bool uncertain = false;
		std::unordered_map<int, EditablePage> pages;
		std::unordered_map<int64_t, Object> objects;
		std::unordered_map<FPDF_PAGEOBJECT, int64_t> object_ids;
		std::unordered_map<int64_t, Annotation> annotations;
		std::unordered_map<int64_t, FPDF_FONT> fonts;

		~State();

		void reset();																// 释放文档及编辑过程中持有的原生句柄
		void replace(
			std::shared_ptr<pdfium::Api> p_api,
			FPDF_DOCUMENT p_document,
			PackedByteArray p_input = {}
		);																			// 释放旧文档后接管新的 PDFium 文档及输入缓冲
		bool require(pdfium::Group p_group);										// 检查当前文档是否可编辑且具备指定能力
		bool capability(pdfium::Group p_group);										// 检查已加载的 PDFium 是否提供指定能力
		bool invalid(const String& p_message);										// 记录参数错误并返回 false
		Error checked(bool p_result, const String& p_message);						// 将原生调用的布尔结果转换为 Godot 错误码
		FPDF_DOCUMENT source_document(const Ref<PDFReader>& p_reader);				// 获取打开的 PDFReader 所持有的源文档句柄
		int page_count() const;														// 查询当前文档的页数
		bool has_page(int64_t p_page);												// 检查页索引是否有效，越界时记录错误
		bool resolve_insert(int64_t& r_at);											// 将 -1 解析为末尾索引并校验插入位置
		FPDF_PAGE page(int64_t p_index);											// 按索引加载并缓存可编辑的页面句柄
		void dirty(int p_page);														// 将已加载的页面标记为需要重新生成内容流
		bool flush_page(EditablePage& p_page);										// 为有修改的页面重新生成内容流
		bool flush_pages();															// 生成所有有修改的页面内容流，成功后关闭页面句柄
		bool flush_and_close_page(int p_page);										// 生成指定页面的内容流，成功后关闭页面句柄
		void close_pages();															// 关闭已加载的页面并使关联的对象和批注 ID 失效
		void close_page(int p_page);												// 关闭指定页面并清除其对象和批注 ID
		bool prepare_save(int64_t p_flags, int64_t p_version);						// 校验保存参数并提交页面修改
		bool save(FPDF_FILEWRITE& p_sink, int64_t p_flags, int64_t p_version);		// 通过 PDFium 将当前文档写入指定输出回调

		int64_t remember_object(
			FPDF_PAGEOBJECT p_handle,
			int p_page = -1,
			int64_t p_parent = 0,
			int64_t p_annotation = 0
		);																			// 登记原生页面对象并返回可复用的对象 ID
		Object* object(int64_t p_id, int p_type = 0);								// 按 ID 查找对象并检查可选的对象类型
		Object* editable_object(int64_t p_id, int p_type = 0);						// 查找能被原地编辑并保存的对象
		void erase_object(int64_t p_id);											// 从句柄映射中注销指定对象 ID
		void erase_descendants(int64_t p_id);										// 使对象的全部后代 ID 失效
		Error finish_object(Object& p_object, bool p_result);						// 检查对象编辑结果，并更新批注外观或标记所属页面已修改
		FPDF_PAGEOBJECTMARK mark(Object& p_object, int64_t p_index);				// 校验索引并读取对象的内容标记句柄
		int64_t remember_font(FPDF_FONT p_font);									// 登记原生字体句柄并分配字体 ID
		int64_t remember_annotation(FPDF_ANNOTATION p_handle, int p_page);			// 登记页面批注句柄并返回批注 ID
		Annotation* annotation(int64_t p_id);										// 按批注 ID 查找编辑记录
		void forget_annotation_objects(int64_t p_id);								// 使指定批注的外观对象 ID 失效
		Error refresh_annotation(int64_t p_id);										// 重新打开编辑后的批注句柄并清除旧外观对象 ID
		FPDF_ATTACHMENT attachment(int64_t p_index);								// 按索引查找文档附件句柄
		Error edit_button(Annotation& p_annotation, int p_type, bool p_checked);	// 修改现有 AcroForm 按钮的选中状态并校验结果

		struct FormSession {
			State& state;
			pdfium::FormInfo info;
			FPDF_FORMHANDLE handle = nullptr;
			bool focused = false;

			FormSession(State& p_state, const Annotation& p_annotation);
			~FormSession();
			bool focus(const Annotation& p_annotation, int p_type, bool p_editable);// 按字段类型和编辑要求定位并聚焦表单控件
			bool commit();															// 结束表单焦点以提交当前修改
		};
#endif
	};
} // namespace godot

#ifdef DOCRENDER_PDFIUM_ENABLED
#define PDF_WRITE_OR_RETURN(group, result) \
	std::lock_guard guard(pdfium::mutex()); \
	if (!state->require(pdfium::Group::writer_##group)) { return result; } \
	[[maybe_unused]] const pdfium::Api& api = *state->api

#define PDF_WRITE_OBJECT_OR_RETURN(type, result) \
	auto* object = state->object(p_object_id, type); \
	if (object == nullptr) { return result; }

#define PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(type, result) \
	auto* object = state->editable_object(p_object_id, type); \
	if (object == nullptr) { return result; }

#define PDF_WRITE_ANNOTATION_OR_RETURN(result) \
	auto* annotation = state->annotation(p_annotation_id); \
	if (annotation == nullptr) { return result; }

namespace godot::pdfium {
	// 将 Godot 的二维变换转换为 PDFium 矩阵
	inline FS_MATRIX native_matrix(const Transform2D& p_transform) {
		return { float(p_transform[0].x), float(p_transform[0].y), float(p_transform[1].x),
			float(p_transform[1].y), float(p_transform[2].x), float(p_transform[2].y) };
	}

	// 将 Godot 矩形转换为 PDFium 的左、上、右、下边界
	inline FS_RECTF native_rect(const Rect2& p_rect) {
		return { float(p_rect.position.x), float(p_rect.get_end().y), float(p_rect.get_end().x), float(p_rect.position.y) };
	}

	// 检查矩形坐标是否有限且宽高均为正
	inline bool valid_rect(const Rect2& p_rect) {
		return p_rect.position.is_finite() && p_rect.get_end().is_finite() && p_rect.size.x > 0 && p_rect.size.y > 0;
	}

	// 校验颜色通道，将其限制在有效范围内并转换为 32 位 RGBA
	inline bool native_color(const Color& p_color, uint32_t& r_rgba) {
		if (!Math::is_finite(p_color.r) || !Math::is_finite(p_color.g) || !Math::is_finite(p_color.b) || !Math::is_finite(p_color.a)) {
			return false;
		}
		r_rgba = p_color.clamp().to_rgba32();
		return true;
	}

	// 检查字节数能否无损转换为 unsigned long
	inline bool fits_unsigned_long(int64_t p_size) {
		return p_size >= 0 && uint64_t(p_size) <= std::numeric_limits<unsigned long>::max();
	}

	// 将 UTF-16 字符串缓冲区作为 PDFium 宽字符指针传递
	inline const FPDF_WCHAR* wide(const Char16String& p_value) {
		return reinterpret_cast<const FPDF_WCHAR*>(p_value.get_data());
	}
} // namespace godot::pdfium
#endif

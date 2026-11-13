#pragma once

#include "pdf_reader.h"
#include "formats/pdf/pdfium/pdfium_backend.h"

namespace godot {
	struct PDFReader::State {
		Error error = OK;
		String message;

		Error fail(Error p_error, const String& p_message);							// 记录并返回错误码，同时保存诊断信息
		void clear_error();															// 清除上一次操作的错误状态

#ifdef DOCRENDER_PDFIUM_ENABLED
		std::shared_ptr<pdfium::Api> api;
		PackedByteArray bytes;
		FPDF_DOCUMENT document = nullptr;
		FPDF_FORMHANDLE form = nullptr;
		pdfium::FormInfo form_info{};

		~State();
		void release_document();													// 释放表单环境、PDF 文档及其输入缓冲
		bool require(pdfium::Group p_group);										// 检查文档是否打开且支持指定的 PDFium 能力
		pdfium::Page page(int64_t p_page);											// 校验页索引并返回自动关闭的 PDFium 页面句柄
		pdfium::TextPage text_page(FPDF_PAGE p_page);								// 加载页面的文本句柄并在离开作用域时自动关闭
		bool ensure_form();															// 按需初始化用于读取或渲染表单的 PDFium 环境

		struct FormPage {
			State& state;
			FPDF_PAGE page;

			FormPage(State& p_state, FPDF_PAGE p_page, int p_index);
			~FormPage();
		};
#endif
	};
} // namespace godot

#ifdef DOCRENDER_PDFIUM_ENABLED
#define PDF_QUERY_OR_RETURN(group, result) \
	std::lock_guard guard(pdfium::mutex()); \
	if (!state->require(Group::group)) { return result; } \
	const Api& api = *state->api
#define PDF_PAGE_OR_RETURN(result) \
	const auto page = state->page(p_page); \
	if (!page) { return result; }
#else
#define PDF_READER_UNAVAILABLE(type, name, args, result) \
	type PDFReader::name args { \
		std::lock_guard guard(pdfium::mutex()); \
		state->fail(ERR_UNAVAILABLE, "PDFium is unavailable in this build."); \
		return result; \
	}
#endif

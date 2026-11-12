#pragma once

#include "pdf_reader.h"
#include "formats/pdf/pdfium/pdfium_backend.h"

namespace godot {
	struct PDFReader::State {
		Error error = OK;
		String message;

		Error fail(Error p_error, const String& p_message);
		void clear_error();

#ifdef DOCRENDER_PDFIUM_ENABLED
		std::shared_ptr<pdfium::Api> api;
		PackedByteArray bytes;
		FPDF_DOCUMENT document = nullptr;
		FPDF_FORMHANDLE form = nullptr;
		pdfium::FormInfo form_info{};

		~State();
		void release_document();
		bool require(pdfium::Group p_group);
		pdfium::Page page(int64_t p_page);
		pdfium::TextPage text_page(FPDF_PAGE p_page);
		bool ensure_form();

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

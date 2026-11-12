#include "pdf_writer_internal.h"

#ifdef DOCRENDER_PDFIUM_ENABLED
#include "formats/pdf/reader/pdf_reader.h"

#include <godot_cpp/variant/char_string.hpp>

#include <unordered_set>

namespace godot {
	using namespace pdfium;

	namespace {
		bool valid_page_size(const Vector2& p_size) {
			return p_size.is_finite() && p_size.x > 0 && p_size.y > 0;
		}

		bool valid_range(const String& p_range, int p_count) {
			if (p_range.is_empty()) {
				return true;
			}
			for (const String& item : p_range.split(",", true)) {
				const PackedStringArray endpoints = item.split("-", true);
				if (endpoints.size() > 2) {
					return false;
				}
				int64_t previous = 1;
				for (const String& number : endpoints) {
					if (number.is_empty() || number.length() > 10 || !number.is_valid_int() || number.begins_with("+")) {
						return false;
					}
					const int64_t page = number.to_int();
					if (page < previous || page > p_count) {
						return false;
					}
					previous = page;
				}
			}
			return true;
		}
	}

	int64_t PDFWriter::get_page_count() {
		std::lock_guard guard(pdfium::mutex());
		return state->require(Group::core) ? state->page_count() : 0;
	}

	int64_t PDFWriter::insert_page(const Vector2& p_size_pt, int64_t p_at) {
		PDF_WRITE_OR_RETURN(pages, -1);
		if (!valid_page_size(p_size_pt)) {
			state->invalid("PDF page dimensions must be finite and positive.");
			return -1;
		}
		if (!state->resolve_insert(p_at) || !state->flush_pages()) {
			return -1;
		}
		const auto page = scoped(api.FPDFPage_New(state->document, int(p_at), p_size_pt.x, p_size_pt.y), api.FPDF_ClosePage);
		if (!page) {
			state->fail(ERR_CANT_CREATE, "Could not insert the PDF page.");
			return -1;
		}
		return p_at;
	}

	Error PDFWriter::delete_page(int64_t p_page_index) {
		PDF_WRITE_OR_RETURN(pages, state->error);
		if (!state->has_page(p_page_index) || !state->flush_pages()) {
			return state->error;
		}
		const int count = state->page_count();
		api.FPDFPage_Delete(state->document, int(p_page_index));
		return state->checked(state->page_count() == count - 1, "Could not delete the PDF page.");
	}

	Error PDFWriter::move_pages(const PackedInt32Array& p_indices, int64_t p_at) {
		PDF_WRITE_OR_RETURN(pages, state->error);
		const int count = state->page_count();
		if (p_indices.is_empty() || p_indices.size() > count || p_at < 0 || p_at > count - p_indices.size()) {
			return state->fail(ERR_INVALID_PARAMETER, "Invalid PDF page move range or destination.");
		}
		std::unordered_set<int> seen;
		for (int64_t i = 0; i < p_indices.size(); ++i) {
			const int index = p_indices[i];
			if (index < 0 || index >= count || !seen.insert(index).second) {
				return state->fail(ERR_INVALID_PARAMETER, "PDF page move indices must be unique and in bounds.");
			}
		}
		if (!state->flush_pages()) {
			return state->error;
		}
		if (!api.FPDF_MovePages(state->document, p_indices.ptr(), static_cast<unsigned long>(p_indices.size()), int(p_at))) {
			state->uncertain = true;
			return state->fail(FAILED, "PDFium could not move the pages. Reopen the document before editing or saving it.");
		}
		return OK;
	}

	Error PDFWriter::import_pages(const Ref<PDFReader>& p_reader, const PackedInt32Array& p_indices, int64_t p_at) {
		PDF_WRITE_OR_RETURN(page_import, state->error);
		const FPDF_DOCUMENT source = state->source_document(p_reader);
		if (source == nullptr || !state->resolve_insert(p_at)) {
			return state->error;
		}
		const int count = api.FPDF_GetPageCount(source);
		const int64_t selected = p_indices.is_empty() ? count : p_indices.size();
		if (selected > std::numeric_limits<int>::max() - state->page_count()) {
			return state->fail(ERR_INVALID_PARAMETER, "The resulting PDF would have too many pages.");
		}
		for (int64_t i = 0; i < p_indices.size(); ++i) {
			if (p_indices[i] < 0 || p_indices[i] >= count) {
				return state->fail(ERR_INVALID_PARAMETER, "PDF source page index is out of bounds.");
			}
		}
		if (selected == 0) {
			return OK;
		}
		if (!state->flush_pages()) {
			return state->error;
		}
		const int previous = state->page_count();
		const bool imported = api.FPDF_ImportPagesByIndex(state->document, source,
			p_indices.is_empty() ? nullptr : p_indices.ptr(), static_cast<unsigned long>(p_indices.size()), int(p_at));
		state->uncertain = !imported && state->page_count() != previous;
		return state->checked(imported, "Could not import the selected PDF pages.");
	}

	Error PDFWriter::import_page_range(const Ref<PDFReader>& p_reader, const String& p_range, int64_t p_at) {
		PDF_WRITE_OR_RETURN(page_import, state->error);
		const FPDF_DOCUMENT source = state->source_document(p_reader);
		if (source == nullptr || !state->resolve_insert(p_at)) {
			return state->error;
		}
		const String range = p_range.replace(" ", "");
		if (!valid_range(range, api.FPDF_GetPageCount(source))) {
			return state->fail(ERR_INVALID_PARAMETER, "Invalid one-based PDF page range.");
		}
		if (range.is_empty() && api.FPDF_GetPageCount(source) == 0) {
			return OK;
		}
		if (!state->flush_pages()) {
			return state->error;
		}
		const int previous = state->page_count();
		const CharString native_range = range.utf8();
		const bool imported = api.FPDF_ImportPages(state->document, source,
			range.is_empty() ? nullptr : native_range.get_data(), int(p_at));
		state->uncertain = !imported && state->page_count() != previous;
		return state->checked(imported, "Could not import the PDF page range.");
	}

	Error PDFWriter::create_n_up(const Ref<PDFReader>& p_reader, const Vector2& p_size_pt, int64_t p_columns, int64_t p_rows) {
		std::lock_guard guard(pdfium::mutex());
		String message;
		const auto api = pdfium::acquire(message);
		if (!api || !api->has(Group::writer_page_templates)) {
			return state->fail(ERR_UNAVAILABLE, api ? String("This PDFium build cannot create N-up documents.") : message);
		}
		const FPDF_DOCUMENT source = state->source_document(p_reader);
		if (source == nullptr) {
			return state->error;
		}
		if (!valid_page_size(p_size_pt) || p_columns <= 0 || p_rows <= 0 ||
			p_columns > std::numeric_limits<int>::max() / p_rows) {
			return state->fail(ERR_INVALID_PARAMETER, "Invalid PDF N-up page dimensions or grid.");
		}
		const FPDF_DOCUMENT document = api->FPDF_ImportNPagesToOne(source, p_size_pt.x, p_size_pt.y,
			size_t(p_columns), size_t(p_rows));
		if (document == nullptr) {
			return state->fail(ERR_CANT_CREATE, "Could not create the PDF N-up document.");
		}
		state->replace(api, document);
		return OK;
	}

	int64_t PDFWriter::create_page_stamp(const Ref<PDFReader>& p_reader, int64_t p_page_index) {
		PDF_WRITE_OR_RETURN(page_templates, 0);
		const FPDF_DOCUMENT source = state->source_document(p_reader);
		if (source == nullptr) {
			return 0;
		}
		if (p_page_index < 0 || p_page_index >= api.FPDF_GetPageCount(source)) {
			state->invalid("PDF source page index is out of bounds.");
			return 0;
		}
		const auto page_template = scoped(api.FPDF_NewXObjectFromPage(state->document, source, int(p_page_index)), api.FPDF_CloseXObject);
		if (!page_template) {
			state->fail(ERR_CANT_CREATE, "Could not create a PDF template from the source page.");
			return 0;
		}
		return state->remember_object(api.FPDF_NewFormObjectFromXObject(page_template.get()));
	}

	Error PDFWriter::set_page_box(int64_t p_page_index, int64_t p_box, const Rect2& p_rect) {
		PDF_WRITE_OR_RETURN(page_geometry, state->error);
		const decltype(&::FPDFPage_SetMediaBox) setters[] = {
			api.FPDFPage_SetMediaBox, api.FPDFPage_SetCropBox, api.FPDFPage_SetBleedBox,
			api.FPDFPage_SetTrimBox, api.FPDFPage_SetArtBox,
		};
		if (p_box < BOX_MEDIA || p_box > BOX_ART || !valid_rect(p_rect)) {
			return state->fail(ERR_INVALID_PARAMETER, "Invalid PDF page box or rectangle.");
		}
		const FPDF_PAGE page = state->page(p_page_index);
		if (page == nullptr) {
			return state->error;
		}
		const FS_RECTF bounds = native_rect(p_rect);
		setters[p_box](page, bounds.left, bounds.bottom, bounds.right, bounds.top);
		return OK;
	}

	Error PDFWriter::set_page_rotation(int64_t p_page_index, int64_t p_degrees) {
		PDF_WRITE_OR_RETURN(page_geometry, state->error);
		if (p_degrees < 0 || p_degrees > 270 || p_degrees % 90 != 0) {
			return state->fail(ERR_INVALID_PARAMETER, "PDF page rotation must be 0, 90, 180, or 270 degrees.");
		}
		const FPDF_PAGE page = state->page(p_page_index);
		if (page == nullptr) {
			return state->error;
		}
		api.FPDFPage_SetRotation(page, int(p_degrees / 90));
		return OK;
	}

	Error PDFWriter::transform_page(int64_t p_page_index, const Transform2D& p_transform, const Variant& p_clip) {
		PDF_WRITE_OR_RETURN(page_geometry, state->error);
		const bool with_clip = p_clip.get_type() != Variant::NIL;
		const Rect2 clip = p_clip.get_type() == Variant::RECT2 ? Rect2(p_clip) : Rect2();
		if (!p_transform.is_finite() || (with_clip && !valid_rect(clip))) {
			return state->fail(ERR_INVALID_PARAMETER, "Invalid PDF page transform or clip rectangle.");
		}
		if (!state->has_page(p_page_index) || !state->flush_and_close_page(int(p_page_index))) {
			return state->error;
		}
		const auto page = scoped(api.FPDF_LoadPage(state->document, int(p_page_index)), api.FPDF_ClosePage);
		if (!page) {
			return state->fail(ERR_FILE_CORRUPT, "Could not load the PDF page.");
		}
		const FS_MATRIX matrix = native_matrix(p_transform);
		const FS_RECTF bounds = native_rect(clip);
		return state->checked(api.FPDFPage_TransFormWithClip(page.get(), &matrix, with_clip ? &bounds : nullptr),
			"Could not transform the PDF page.");
	}

	Error PDFWriter::clip_page(int64_t p_page_index, const Rect2& p_rect) {
		PDF_WRITE_OR_RETURN(page_geometry, state->error);
		if (!valid_rect(p_rect)) {
			return state->fail(ERR_INVALID_PARAMETER, "Invalid PDF page clip rectangle.");
		}
		if (!state->has_page(p_page_index) || !state->flush_and_close_page(int(p_page_index))) {
			return state->error;
		}
		const auto page = scoped(api.FPDF_LoadPage(state->document, int(p_page_index)), api.FPDF_ClosePage);
		if (!page) {
			return state->fail(ERR_FILE_CORRUPT, "Could not load the PDF page.");
		}
		const FS_RECTF bounds = native_rect(p_rect);
		const auto clip = scoped(api.FPDF_CreateClipPath(bounds.left, bounds.bottom, bounds.right, bounds.top), api.FPDF_DestroyClipPath);
		if (!clip) {
			return state->fail(ERR_CANT_CREATE, "Could not create the PDF clip path.");
		}
		api.FPDFPage_InsertClipPath(page.get(), clip.get());
		return OK;
	}

	Error PDFWriter::transform_page_annotations(int64_t p_page_index, const Transform2D& p_transform) {
		PDF_WRITE_OR_RETURN(page_geometry, state->error);
		if (!p_transform.is_finite()) {
			return state->fail(ERR_INVALID_PARAMETER, "Invalid PDF annotation transform.");
		}
		const FPDF_PAGE page = state->page(p_page_index);
		if (page == nullptr) {
			return state->error;
		}
		const FS_MATRIX m = native_matrix(p_transform);
		api.FPDFPage_TransformAnnots(page, m.a, m.b, m.c, m.d, m.e, m.f);
		return OK;
	}

	Error PDFWriter::generate_page_content(int64_t p_page_index) {
		PDF_WRITE_OR_RETURN(objects, state->error);
		const FPDF_PAGE page = state->page(p_page_index);
		if (page == nullptr) {
			return state->error;
		}
		state->dirty(int(p_page_index));
		return state->flush_page(state->pages.at(int(p_page_index))) ? OK : state->error;
	}

	Error PDFWriter::flatten_page(int64_t p_page_index, int64_t p_mode) {
		PDF_WRITE_OR_RETURN(flatten, state->error);
		if (p_mode != FLAT_NORMALDISPLAY && p_mode != FLAT_PRINT) {
			return state->fail(ERR_INVALID_PARAMETER, "Invalid PDF flatten mode.");
		}
		if (!state->has_page(p_page_index) || !state->flush_and_close_page(int(p_page_index))) {
			return state->error;
		}
		const auto page = scoped(api.FPDF_LoadPage(state->document, int(p_page_index)), api.FPDF_ClosePage);
		if (!page) {
			return state->fail(ERR_FILE_CORRUPT, "Could not load the PDF page.");
		}
		return state->checked(api.FPDFPage_Flatten(page.get(), int(p_mode)) != FLATTEN_FAIL, "Could not flatten the PDF page.");
	}
} // namespace godot
#endif

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

		Error fail(Error p_error, const String& p_message);
		Error unavailable();
		void clear_error();

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

			bool is_detached() const { return page < 0 && parent == 0 && annotation == 0; }
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
		void reset();
		void replace(std::shared_ptr<pdfium::Api> p_api, FPDF_DOCUMENT p_document, PackedByteArray p_input = {});
		bool require(pdfium::Group p_group);
		bool capability(pdfium::Group p_group);
		bool invalid(const String& p_message);
		Error checked(bool p_result, const String& p_message);
		FPDF_DOCUMENT source_document(const Ref<PDFReader>& p_reader);
		int page_count() const;
		bool has_page(int64_t p_page);
		bool resolve_insert(int64_t& r_at);
		FPDF_PAGE page(int64_t p_index);
		void dirty(int p_page);
		bool flush_page(EditablePage& p_page);
		bool flush_pages();
		bool flush_and_close_page(int p_page);
		void close_pages();
		void close_page(int p_page);
		bool prepare_save(int64_t p_flags, int64_t p_version);
		bool save(FPDF_FILEWRITE& p_sink, int64_t p_flags, int64_t p_version);

		int64_t remember_object(FPDF_PAGEOBJECT p_handle, int p_page = -1, int64_t p_parent = 0, int64_t p_annotation = 0);
		Object* object(int64_t p_id, int p_type = 0);
		Object* editable_object(int64_t p_id, int p_type = 0);
		void erase_object(int64_t p_id);
		void erase_descendants(int64_t p_id);
		Error finish_object(Object& p_object, bool p_result);
		FPDF_PAGEOBJECTMARK mark(Object& p_object, int64_t p_index);
		int64_t remember_font(FPDF_FONT p_font);
		int64_t remember_annotation(FPDF_ANNOTATION p_handle, int p_page);
		Annotation* annotation(int64_t p_id);
		void forget_annotation_objects(int64_t p_id);
		Error refresh_annotation(int64_t p_id);
		FPDF_ATTACHMENT attachment(int64_t p_index);
		Error edit_button(Annotation& p_annotation, int p_type, bool p_checked);

		struct FormSession {
			State& state;
			pdfium::FormInfo info;
			FPDF_FORMHANDLE handle = nullptr;
			bool focused = false;

			FormSession(State& p_state, const Annotation& p_annotation);
			~FormSession();
			bool focus(const Annotation& p_annotation, int p_type, bool p_editable);
			bool commit();
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
	inline FS_MATRIX native_matrix(const Transform2D& p_transform) {
		return { float(p_transform[0].x), float(p_transform[0].y), float(p_transform[1].x),
			float(p_transform[1].y), float(p_transform[2].x), float(p_transform[2].y) };
	}

	inline FS_RECTF native_rect(const Rect2& p_rect) {
		return { float(p_rect.position.x), float(p_rect.get_end().y), float(p_rect.get_end().x), float(p_rect.position.y) };
	}

	inline bool valid_rect(const Rect2& p_rect) {
		return p_rect.position.is_finite() && p_rect.get_end().is_finite() && p_rect.size.x > 0 && p_rect.size.y > 0;
	}

	inline bool native_color(const Color& p_color, uint32_t& r_rgba) {
		if (!Math::is_finite(p_color.r) || !Math::is_finite(p_color.g) || !Math::is_finite(p_color.b) || !Math::is_finite(p_color.a)) {
			return false;
		}
		r_rgba = p_color.clamp().to_rgba32();
		return true;
	}

	inline bool fits_unsigned_long(int64_t p_size) {
		return p_size >= 0 && uint64_t(p_size) <= std::numeric_limits<unsigned long>::max();
	}

	inline const FPDF_WCHAR* wide(const Char16String& p_value) {
		return reinterpret_cast<const FPDF_WCHAR*>(p_value.get_data());
	}
} // namespace godot::pdfium
#endif

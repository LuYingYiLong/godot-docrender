#include "pdf_writer_internal.h"

#ifdef DOCRENDER_PDFIUM_ENABLED
#include <godot_cpp/variant/char_string.hpp>

#include <cmath>

namespace godot {
	using namespace pdfium;

	int64_t PDFWriter::get_object_id(int64_t p_page_index, const PackedInt32Array& p_object_path) {
		PDF_WRITE_OR_RETURN(objects, 0);
		const FPDF_PAGE page = state->page(p_page_index);
		if (page == nullptr) {
			return 0;
		}
		if (p_object_path.is_empty() || p_object_path[0] < 0 || p_object_path[0] >= api.FPDFPage_CountObjects(page)) {
			state->invalid("Invalid PDF object path.");
			return 0;
		}
		int64_t id = state->remember_object(api.FPDFPage_GetObject(page, p_object_path[0]), int(p_page_index));
		for (int64_t i = 1; id != 0 && i < p_object_path.size(); ++i) {
			const auto* parent = state->object(id, FPDF_PAGEOBJ_FORM);
			const int index = p_object_path[i];
			if (parent == nullptr || index < 0 || index >= api.FPDFFormObj_CountObjects(parent->handle)) {
				state->invalid("Invalid PDF object path.");
				return 0;
			}
			id = state->remember_object(api.FPDFFormObj_GetObject(parent->handle, index), int(p_page_index), id);
		}
		return id;
	}

	int64_t PDFWriter::get_object_child_count(int64_t p_object_id) {
		PDF_WRITE_OR_RETURN(objects, 0);
		PDF_WRITE_OBJECT_OR_RETURN(FPDF_PAGEOBJ_FORM, 0)
		return api.FPDFFormObj_CountObjects(object->handle);
	}

	int64_t PDFWriter::get_object_child_id(int64_t p_object_id, int64_t p_index) {
		PDF_WRITE_OR_RETURN(objects, 0);
		PDF_WRITE_OBJECT_OR_RETURN(FPDF_PAGEOBJ_FORM, 0)
		if (p_index < 0 || p_index >= api.FPDFFormObj_CountObjects(object->handle)) {
			state->invalid("PDF child object index is out of bounds.");
			return 0;
		}
		return state->remember_object(api.FPDFFormObj_GetObject(object->handle, unsigned(p_index)), object->page,
			p_object_id, object->annotation);
	}

	Error PDFWriter::insert_object(int64_t p_page_index, int64_t p_object_id, int64_t p_at) {
		PDF_WRITE_OR_RETURN(objects, state->error);
		PDF_WRITE_OBJECT_OR_RETURN(0, state->error)
		if (!object->is_detached()) {
			return state->fail(ERR_INVALID_PARAMETER, "Only a detached object can be inserted into a page.");
		}
		const FPDF_PAGE page = state->page(p_page_index);
		if (page == nullptr) {
			return state->error;
		}
		const int count = api.FPDFPage_CountObjects(page);
		if (p_at == -1) {
			p_at = count;
		}
		if (p_at < 0 || p_at > count) {
			return state->fail(ERR_INVALID_PARAMETER, "PDF object insertion index is out of bounds.");
		}
		if (!api.FPDFPage_InsertObjectAtIndex(page, object->handle, size_t(p_at))) {
			// PDFium takes ownership when called, including on failure.
			state->erase_descendants(p_object_id);
			state->erase_object(p_object_id);
			return state->fail(FAILED, "Could not insert the PDF object.");
		}
		state->erase_descendants(p_object_id);
		object->page = int(p_page_index);
		state->dirty(object->page);
		return OK;
	}

	Error PDFWriter::detach_object(int64_t p_object_id) {
		PDF_WRITE_OR_RETURN(objects, state->error);
		PDF_WRITE_OBJECT_OR_RETURN(0, state->error)
		if (object->is_detached()) {
			return OK;
		}
		if (object->annotation != 0) {
			return state->fail(ERR_INVALID_PARAMETER, "An annotation appearance object cannot be detached.");
		}
		if (object->parent != 0) {
			auto& parent = state->objects.at(object->parent);
			if (parent.parent != 0) {
				return state->fail(ERR_UNAVAILABLE, "Detach the containing Form first. PDFium cannot regenerate an inner Form in place.");
			}
			if (!api.FPDFFormObj_RemoveObject(parent.handle, object->handle)) {
				return state->fail(FAILED, "Could not detach the PDF Form child object.");
			}
			state->finish_object(parent, true);
		} else if (!api.FPDFPage_RemoveObject(state->pages.at(object->page).handle, object->handle)) {
			return state->fail(FAILED, "Could not detach the PDF page object.");
		}
		state->dirty(object->page);
		state->erase_descendants(p_object_id);
		object->page = -1;
		object->parent = 0;
		return OK;
	}

	Error PDFWriter::remove_object(int64_t p_object_id) {
		PDF_WRITE_OR_RETURN(objects, state->error);
		if (detach_object(p_object_id) != OK) {
			return state->error;
		}
		api.FPDFPageObj_Destroy(state->objects.at(p_object_id).handle);
		state->erase_object(p_object_id);
		return OK;
	}

	Error PDFWriter::set_object_matrix(int64_t p_object_id, const Transform2D& p_transform) {
		PDF_WRITE_OR_RETURN(objects, state->error);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(0, state->error)
		if (!p_transform.is_finite()) {
			return state->fail(ERR_INVALID_PARAMETER, "Invalid PDF object matrix.");
		}
		const FS_MATRIX matrix = native_matrix(p_transform);
		return state->finish_object(*object, api.FPDFPageObj_SetMatrix(object->handle, &matrix));
	}

	Error PDFWriter::transform_object(int64_t p_object_id, const Transform2D& p_transform) {
		PDF_WRITE_OR_RETURN(objects, state->error);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(0, state->error)
		if (!p_transform.is_finite()) {
			return state->fail(ERR_INVALID_PARAMETER, "Invalid PDF object transform.");
		}
		const FS_MATRIX matrix = native_matrix(p_transform);
		return state->finish_object(*object, api.FPDFPageObj_TransformF(object->handle, &matrix));
	}

	Error PDFWriter::transform_object_clip(int64_t p_object_id, const Transform2D& p_transform) {
		PDF_WRITE_OR_RETURN(objects, state->error);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(0, state->error)
		if (!p_transform.is_finite()) {
			return state->fail(ERR_INVALID_PARAMETER, "Invalid PDF object clip transform.");
		}
		const FS_MATRIX m = native_matrix(p_transform);
		api.FPDFPageObj_TransformClipPath(object->handle, m.a, m.b, m.c, m.d, m.e, m.f);
		return state->finish_object(*object, true);
	}

	Error PDFWriter::set_object_blend_mode(int64_t p_object_id, const String& p_mode) {
		PDF_WRITE_OR_RETURN(objects, state->error);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(0, state->error)
		constexpr const char* names[] = {
			"Normal", "Compatible", "Multiply", "Screen", "Overlay", "Darken", "Lighten", "ColorDodge",
			"ColorBurn", "HardLight", "SoftLight", "Difference", "Exclusion", "Hue", "Saturation", "Color", "Luminosity",
		};
		for (const char* mode : names) {
			if (p_mode == mode) {
				api.FPDFPageObj_SetBlendMode(object->handle, mode);
				return state->finish_object(*object, true);
			}
		}
		return state->fail(ERR_INVALID_PARAMETER, "Unknown PDF object blend mode.");
	}

	Error PDFWriter::set_object_fill_color(int64_t p_object_id, const Color& p_color) {
		PDF_WRITE_OR_RETURN(objects, state->error);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(0, state->error)
		uint32_t rgba;
		if (!native_color(p_color, rgba)) {
			return state->fail(ERR_INVALID_PARAMETER, "PDF color components must be finite.");
		}
		return state->finish_object(*object, api.FPDFPageObj_SetFillColor(object->handle,
			rgba >> 24, (rgba >> 16) & 255, (rgba >> 8) & 255, rgba & 255));
	}

	Error PDFWriter::set_object_stroke_color(int64_t p_object_id, const Color& p_color) {
		PDF_WRITE_OR_RETURN(objects, state->error);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(0, state->error)
		uint32_t rgba;
		if (!native_color(p_color, rgba)) {
			return state->fail(ERR_INVALID_PARAMETER, "PDF color components must be finite.");
		}
		return state->finish_object(*object, api.FPDFPageObj_SetStrokeColor(object->handle,
			rgba >> 24, (rgba >> 16) & 255, (rgba >> 8) & 255, rgba & 255));
	}

	Error PDFWriter::set_object_dash(int64_t p_object_id, const PackedFloat32Array& p_lengths, float p_phase) {
		PDF_WRITE_OR_RETURN(objects, state->error);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(0, state->error)
		bool positive = false;
		for (int64_t i = 0; i < p_lengths.size(); ++i) {
			const float length = p_lengths[i];
			if (!std::isfinite(length) || length < 0) {
				return state->fail(ERR_INVALID_PARAMETER, "PDF dash lengths must be finite and nonnegative.");
			}
			positive |= length > 0;
		}
		if (!std::isfinite(p_phase) || (!p_lengths.is_empty() && !positive)) {
			return state->fail(ERR_INVALID_PARAMETER, "Invalid PDF dash pattern or phase.");
		}
		return state->finish_object(*object, api.FPDFPageObj_SetDashArray(object->handle, p_lengths.ptr(), size_t(p_lengths.size()), p_phase));
	}

	int64_t PDFWriter::create_path(const Vector2& p_start) {
		PDF_WRITE_OR_RETURN(paths, 0);
		if (!p_start.is_finite()) {
			state->invalid("Invalid PDF path origin.");
			return 0;
		}
		return state->remember_object(api.FPDFPageObj_CreateNewPath(p_start.x, p_start.y));
	}

	int64_t PDFWriter::create_rectangle(const Rect2& p_rect) {
		PDF_WRITE_OR_RETURN(paths, 0);
		if (!valid_rect(p_rect)) {
			state->invalid("Invalid PDF rectangle.");
			return 0;
		}
		return state->remember_object(api.FPDFPageObj_CreateNewRect(p_rect.position.x, p_rect.position.y, p_rect.size.x, p_rect.size.y));
	}

	Error PDFWriter::set_object_active(int64_t p_object_id, bool p_active) {
		PDF_WRITE_OR_RETURN(objects, state->error);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(0, state->error)
		return state->finish_object(*object, api.FPDFPageObj_SetIsActive(object->handle, p_active));
	}

	Error PDFWriter::set_object_stroke_width(int64_t p_object_id, float p_width) {
		PDF_WRITE_OR_RETURN(objects, state->error);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(0, state->error)
		if (!(std::isfinite(p_width) && p_width >= 0)) {
			return state->fail(ERR_INVALID_PARAMETER, "Invalid PDF stroke width.");
		}
		return state->finish_object(*object, api.FPDFPageObj_SetStrokeWidth(object->handle, p_width));
	}

	Error PDFWriter::set_object_line_cap(int64_t p_object_id, int64_t p_cap) {
		PDF_WRITE_OR_RETURN(objects, state->error);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(0, state->error)
		if (!(p_cap >= CAP_BUTT && p_cap <= CAP_SQUARE)) {
			return state->fail(ERR_INVALID_PARAMETER, "Invalid PDF line cap.");
		}
		return state->finish_object(*object, api.FPDFPageObj_SetLineCap(object->handle, int(p_cap)));
	}

	Error PDFWriter::set_object_line_join(int64_t p_object_id, int64_t p_join) {
		PDF_WRITE_OR_RETURN(objects, state->error);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(0, state->error)
		if (!(p_join >= JOIN_MITER && p_join <= JOIN_BEVEL)) {
			return state->fail(ERR_INVALID_PARAMETER, "Invalid PDF line join.");
		}
		return state->finish_object(*object, api.FPDFPageObj_SetLineJoin(object->handle, int(p_join)));
	}

	Error PDFWriter::path_move_to(int64_t p_object_id, const Vector2& p_point) {
		PDF_WRITE_OR_RETURN(paths, state->error);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(FPDF_PAGEOBJ_PATH, state->error)
		if (!(p_point.is_finite())) {
			return state->fail(ERR_INVALID_PARAMETER, "Invalid PDF move to.");
		}
		return state->finish_object(*object, api.FPDFPath_MoveTo(object->handle, p_point.x, p_point.y));
	}

	Error PDFWriter::path_line_to(int64_t p_object_id, const Vector2& p_point) {
		PDF_WRITE_OR_RETURN(paths, state->error);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(FPDF_PAGEOBJ_PATH, state->error)
		if (!(p_point.is_finite())) {
			return state->fail(ERR_INVALID_PARAMETER, "Invalid PDF line to.");
		}
		return state->finish_object(*object, api.FPDFPath_LineTo(object->handle, p_point.x, p_point.y));
	}

	Error PDFWriter::path_bezier_to(int64_t p_object_id, const Vector2& p_control_a, const Vector2& p_control_b, const Vector2& p_end) {
		PDF_WRITE_OR_RETURN(paths, state->error);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(FPDF_PAGEOBJ_PATH, state->error)
		if (!(p_control_a.is_finite() && p_control_b.is_finite() && p_end.is_finite())) {
			return state->fail(ERR_INVALID_PARAMETER, "Invalid PDF bezier to.");
		}
		return state->finish_object(*object, api.FPDFPath_BezierTo(object->handle, p_control_a.x, p_control_a.y, p_control_b.x, p_control_b.y, p_end.x, p_end.y));
	}

	Error PDFWriter::path_close(int64_t p_object_id) {
		PDF_WRITE_OR_RETURN(paths, state->error);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(FPDF_PAGEOBJ_PATH, state->error)
		return state->finish_object(*object, api.FPDFPath_Close(object->handle));
	}

	Error PDFWriter::set_path_draw_mode(int64_t p_object_id, int64_t p_fill_mode, bool p_stroke) {
		PDF_WRITE_OR_RETURN(paths, state->error);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(FPDF_PAGEOBJ_PATH, state->error)
		if (!(p_fill_mode >= FILL_NONE && p_fill_mode <= FILL_WINDING)) {
			return state->fail(ERR_INVALID_PARAMETER, "Invalid PDF draw mode.");
		}
		return state->finish_object(*object, api.FPDFPath_SetDrawMode(object->handle, int(p_fill_mode), p_stroke));
	}

	int64_t PDFWriter::add_object_mark(int64_t p_object_id, const String& p_name) {
		PDF_WRITE_OR_RETURN(content_marks, -1);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(0, -1)
		if (p_name.is_empty()) {
			state->invalid("A PDF content mark name is required.");
			return -1;
		}
		const int index = api.FPDFPageObj_CountMarks(object->handle);
		const CharString name = p_name.utf8();
		return state->finish_object(*object, index >= 0 && api.FPDFPageObj_AddMark(object->handle, name.get_data())) == OK ? index : -1;
	}

	Error PDFWriter::share_object_mark(int64_t p_object_id, int64_t p_source_object_id, int64_t p_mark_index) {
		PDF_WRITE_OR_RETURN(content_marks, state->error);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(0, state->error)
		auto* source = state->object(p_source_object_id);
		if (source == nullptr) {
			return state->error;
		}
		const auto mark = state->mark(*source, p_mark_index);
		if (mark == nullptr) {
			return state->error;
		}
		return state->finish_object(*object, api.FPDFPageObj_AddExistingMark(object->handle, mark));
	}

	Error PDFWriter::remove_object_mark(int64_t p_object_id, int64_t p_mark_index) {
		PDF_WRITE_OR_RETURN(content_marks, state->error);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(0, state->error)
		const auto mark = state->mark(*object, p_mark_index);
		return mark != nullptr ? state->finish_object(*object, api.FPDFPageObj_RemoveMark(object->handle, mark)) : state->error;
	}

	Error PDFWriter::set_object_mark_param(int64_t p_object_id, int64_t p_mark_index, const String& p_key, const Variant& p_value) {
		PDF_WRITE_OR_RETURN(content_marks, state->error);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(0, state->error)
		const auto mark = state->mark(*object, p_mark_index);
		if (mark == nullptr) {
			return state->error;
		}
		const CharString key = p_key.utf8();
		bool changed = false;
		switch (p_value.get_type()) {
			case Variant::INT: {
				const int64_t value = p_value;
				if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max()) {
					return state->fail(ERR_INVALID_PARAMETER, "PDF content mark integer must fit in 32 bits.");
				}
				changed = api.FPDFPageObjMark_SetIntParam(state->document, object->handle, mark, key.get_data(), int(value));
			} break;
			case Variant::FLOAT: {
				const float value = p_value;
				if (!std::isfinite(value)) {
					return state->fail(ERR_INVALID_PARAMETER, "PDF content mark number must be finite.");
				}
				changed = api.FPDFPageObjMark_SetFloatParam(state->document, object->handle, mark, key.get_data(), value);
			} break;
			case Variant::STRING: {
				const CharString value = String(p_value).utf8();
				changed = api.FPDFPageObjMark_SetStringParam(state->document, object->handle, mark, key.get_data(), value.get_data());
			} break;
			case Variant::PACKED_BYTE_ARRAY: {
				const PackedByteArray value = p_value;
				if (!fits_unsigned_long(value.size())) {
					return state->fail(ERR_INVALID_PARAMETER, "PDF content mark data is too large.");
				}
				changed = api.FPDFPageObjMark_SetBlobParam(state->document, object->handle, mark, key.get_data(),
					value.ptr(), static_cast<unsigned long>(value.size()));
			} break;
			default:
				return state->fail(ERR_INVALID_PARAMETER, "PDF content mark values must be integers, floats, strings, or byte arrays.");
		}
		return state->finish_object(*object, changed);
	}

	Error PDFWriter::remove_object_mark_param(int64_t p_object_id, int64_t p_mark_index, const String& p_key) {
		PDF_WRITE_OR_RETURN(content_marks, state->error);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(0, state->error)
		const auto mark = state->mark(*object, p_mark_index);
		const CharString key = p_key.utf8();
		return mark != nullptr ? state->finish_object(*object,
			api.FPDFPageObjMark_RemoveParam(object->handle, mark, key.get_data())) : state->error;
	}
} // namespace godot
#endif

#include "pdf_writer_internal.h"

#ifdef DOCRENDER_PDFIUM_ENABLED
#include <cmath>

namespace godot {
	using namespace pdfium;

	namespace {
		bool make_quad(const PackedVector2Array& p_points, FS_QUADPOINTSF& r_quad) {
			if (p_points.size() != 4) {
				return false;
			}
			for (int i = 0; i < 4; ++i) {
				if (!p_points[i].is_finite()) {
					return false;
				}
			}
			r_quad = { float(p_points[0].x), float(p_points[0].y), float(p_points[1].x), float(p_points[1].y),
				float(p_points[2].x), float(p_points[2].y), float(p_points[3].x), float(p_points[3].y) };
			return true;
		}

		String form_value(const Api& p_api, FPDF_FORMHANDLE p_form, FPDF_ANNOTATION p_annotation) {
			return utf16(read_buffer([&](void* p_buffer, unsigned long p_length) {
				return p_api.FPDFAnnot_GetFormFieldValue(p_form, p_annotation, static_cast<FPDF_WCHAR*>(p_buffer), p_length);
			}));
		}

		String normalized_form_text(const String& p_text) {
			return p_text.replace("\r\n", "\n").replace("\r", "\n");
		}
	}

	PackedInt32Array PDFWriter::get_supported_annotation_subtypes() {
		std::lock_guard guard(pdfium::mutex());
		state->clear_error();
		String message;
		const auto api = pdfium::acquire(message);
		if (!api || !api->has(Group::writer_annotations)) {
			state->fail(ERR_UNAVAILABLE, api ? String("This PDFium build cannot create PDF annotations.") : message);
			return {};
		}
		PackedInt32Array result;
		for (int i = FPDF_ANNOT_UNKNOWN + 1; i <= FPDF_ANNOT_REDACT; ++i) {
			if (api->FPDFAnnot_IsSupportedSubtype(i)) {
				result.append(i);
			}
		}
		return result;
	}

	int64_t PDFWriter::create_annotation(int64_t p_page_index, int64_t p_subtype) {
		PDF_WRITE_OR_RETURN(annotations, 0);
		if (p_subtype <= FPDF_ANNOT_UNKNOWN || p_subtype > FPDF_ANNOT_REDACT || !api.FPDFAnnot_IsSupportedSubtype(int(p_subtype))) {
			state->invalid("PDFium cannot create this annotation subtype.");
			return 0;
		}
		const FPDF_PAGE page = state->page(p_page_index);
		return page != nullptr ? state->remember_annotation(api.FPDFPage_CreateAnnot(page, int(p_subtype)), int(p_page_index)) : 0;
	}

	int64_t PDFWriter::get_annotation_id(int64_t p_page_index, int64_t p_annotation_index) {
		PDF_WRITE_OR_RETURN(annotations, 0);
		const FPDF_PAGE page = state->page(p_page_index);
		if (page == nullptr) {
			return 0;
		}
		if (p_annotation_index < 0 || p_annotation_index >= api.FPDFPage_GetAnnotCount(page)) {
			state->invalid("PDF annotation index is out of bounds.");
			return 0;
		}
		for (const auto& [id, annotation] : state->annotations) {
			if (annotation.page == p_page_index && api.FPDFPage_GetAnnotIndex(page, annotation.handle) == p_annotation_index) {
				return id;
			}
		}
		return state->remember_annotation(api.FPDFPage_GetAnnot(page, int(p_annotation_index)), int(p_page_index));
	}

	Error PDFWriter::remove_annotation(int64_t p_annotation_id) {
		PDF_WRITE_OR_RETURN(annotations, state->error);
		PDF_WRITE_ANNOTATION_OR_RETURN(state->error)
		const FPDF_PAGE page = state->pages.at(annotation->page).handle;
		const int index = api.FPDFPage_GetAnnotIndex(page, annotation->handle);
		if (index < 0 || !api.FPDFPage_RemoveAnnot(page, index)) {
			return state->fail(FAILED, "Could not remove the PDF annotation.");
		}
		state->forget_annotation_objects(p_annotation_id);
		api.FPDFPage_CloseAnnot(annotation->handle);
		state->annotations.erase(p_annotation_id);
		return OK;
	}

	Error PDFWriter::State::refresh_annotation(int64_t p_id) {
		Annotation& annotation = annotations.at(p_id);
		const FPDF_PAGE page = pages.at(annotation.page).handle;
		const int index = api->FPDFPage_GetAnnotIndex(page, annotation.handle);
		forget_annotation_objects(p_id);
		api->FPDFPage_CloseAnnot(annotation.handle);
		annotation.handle = index >= 0 ? api->FPDFPage_GetAnnot(page, index) : nullptr;
		if (annotation.handle == nullptr) {
			annotations.erase(p_id);
			return fail(ERR_CANT_ACQUIRE_RESOURCE, "Could not reopen the edited PDF annotation.");
		}
		return OK;
	}

	Error PDFWriter::set_annotation_rect(int64_t p_annotation_id, const Rect2& p_rect) {
		PDF_WRITE_OR_RETURN(annotations, state->error);
		PDF_WRITE_ANNOTATION_OR_RETURN(state->error)
		if (!valid_rect(p_rect)) {
			return state->fail(ERR_INVALID_PARAMETER, "Invalid PDF annotation rectangle.");
		}
		const FS_RECTF bounds = native_rect(p_rect);
		return state->checked(api.FPDFAnnot_SetRect(annotation->handle, &bounds), "Could not set the PDF annotation rect.");
	}

	Error PDFWriter::set_annotation_color(int64_t p_annotation_id, const Color& p_color, bool p_interior) {
		PDF_WRITE_OR_RETURN(annotations, state->error);
		PDF_WRITE_ANNOTATION_OR_RETURN(state->error)
		uint32_t rgba;
		if (!native_color(p_color, rgba)) {
			return state->fail(ERR_INVALID_PARAMETER, "PDF color components must be finite.");
		}
		return state->checked(api.FPDFAnnot_SetColor(annotation->handle, p_interior ? FPDFANNOT_COLORTYPE_InteriorColor : FPDFANNOT_COLORTYPE_Color,
			rgba >> 24, (rgba >> 16) & 255, (rgba >> 8) & 255, rgba & 255), "Could not set the PDF annotation color.");
	}

	Error PDFWriter::set_annotation_flags(int64_t p_annotation_id, int64_t p_flags) {
		PDF_WRITE_OR_RETURN(annotations, state->error);
		PDF_WRITE_ANNOTATION_OR_RETURN(state->error)
		if (p_flags < 0 || p_flags > std::numeric_limits<int>::max()) {
			return state->fail(ERR_INVALID_PARAMETER, "Invalid PDF annotation flags.");
		}
		return state->checked(api.FPDFAnnot_SetFlags(annotation->handle, int(p_flags)), "Could not set the PDF annotation flags.");
	}

	Error PDFWriter::set_annotation_string(int64_t p_annotation_id, const String& p_key, const String& p_value) {
		PDF_WRITE_OR_RETURN(annotations, state->error);
		PDF_WRITE_ANNOTATION_OR_RETURN(state->error)
		if (p_key.is_empty()) {
			return state->fail(ERR_INVALID_PARAMETER, "A PDF annotation key is required.");
		}
		const CharString key = p_key.utf8();
		const Char16String value = p_value.utf16();
		return state->checked(api.FPDFAnnot_SetStringValue(annotation->handle, key.get_data(), wide(value)), "Could not set the PDF annotation string.");
	}

	Error PDFWriter::clear_annotation_ink(int64_t p_annotation_id) {
		PDF_WRITE_OR_RETURN(annotations, state->error);
		PDF_WRITE_ANNOTATION_OR_RETURN(state->error)
		return state->checked(api.FPDFAnnot_RemoveInkList(annotation->handle), "Could not clear the PDF annotation ink.");
	}

	Error PDFWriter::set_annotation_uri(int64_t p_annotation_id, const String& p_uri) {
		PDF_WRITE_OR_RETURN(annotations, state->error);
		PDF_WRITE_ANNOTATION_OR_RETURN(state->error)
		for (int64_t i = 0; i < p_uri.length(); ++i) {
			if (p_uri[i] < 0x20 || p_uri[i] >= 0x7f) {
				return state->fail(ERR_INVALID_PARAMETER, "A PDF link URI must use printable ASCII.");
			}
		}
		if (p_uri.is_empty()) {
			return state->fail(ERR_INVALID_PARAMETER, "A PDF link URI is required.");
		}
		const CharString uri = p_uri.utf8();
		return state->checked(api.FPDFAnnot_SetURI(annotation->handle, uri.get_data()), "Could not set the PDF annotation uri.");
	}

	Error PDFWriter::set_annotation_border(int64_t p_annotation_id, const Vector2& p_radii, float p_width) {
		PDF_WRITE_OR_RETURN(annotations, state->error);
		PDF_WRITE_ANNOTATION_OR_RETURN(state->error)
		if (!p_radii.is_finite() || p_radii.x < 0 || p_radii.y < 0 || !std::isfinite(p_width) || p_width < 0) {
			return state->fail(ERR_INVALID_PARAMETER, "Invalid PDF annotation border dimensions.");
		}
		if (!api.FPDFAnnot_SetBorder(annotation->handle, p_radii.x, p_radii.y, p_width)) {
			return state->fail(FAILED, "Could not set the PDF annotation border.");
		}
		return state->refresh_annotation(p_annotation_id);
	}

	Error PDFWriter::set_annotation_quad(int64_t p_annotation_id, int64_t p_index, const PackedVector2Array& p_points) {
		PDF_WRITE_OR_RETURN(annotations, state->error);
		PDF_WRITE_ANNOTATION_OR_RETURN(state->error)
		FS_QUADPOINTSF quad;
		if (p_index < 0 || !make_quad(p_points, quad)) {
			return state->fail(ERR_INVALID_PARAMETER, "A nonnegative quad index and four finite PDF points are required.");
		}
		return state->checked(api.FPDFAnnot_SetAttachmentPoints(annotation->handle, size_t(p_index), &quad),
			"Could not set the PDF annotation quad.");
	}

	Error PDFWriter::append_annotation_quad(int64_t p_annotation_id, const PackedVector2Array& p_points) {
		PDF_WRITE_OR_RETURN(annotations, state->error);
		PDF_WRITE_ANNOTATION_OR_RETURN(state->error)
		FS_QUADPOINTSF quad;
		if (!make_quad(p_points, quad)) {
			return state->fail(ERR_INVALID_PARAMETER, "Four finite PDF points are required.");
		}
		return state->checked(api.FPDFAnnot_AppendAttachmentPoints(annotation->handle, &quad), "Could not append the PDF annotation quad.");
	}

	int64_t PDFWriter::add_annotation_ink_stroke(int64_t p_annotation_id, const PackedVector2Array& p_points) {
		PDF_WRITE_OR_RETURN(annotations, -1);
		PDF_WRITE_ANNOTATION_OR_RETURN(-1)
		if (p_points.is_empty() || p_points.size() > std::numeric_limits<int32_t>::max()) {
			state->invalid("A PDF ink stroke requires at least one point and a 32-bit point count.");
			return -1;
		}
		std::vector<FS_POINTF> points;
		points.reserve(size_t(p_points.size()));
		for (int64_t i = 0; i < p_points.size(); ++i) {
			const Vector2 point = p_points[i];
			if (!point.is_finite()) {
				state->invalid("PDF ink stroke points must be finite.");
				return -1;
			}
			points.push_back({ float(point.x), float(point.y) });
		}
		const int index = api.FPDFAnnot_AddInkStroke(annotation->handle, points.data(), points.size());
		state->checked(index >= 0, "Could not add the PDF annotation ink stroke.");
		return index;
	}

	Error PDFWriter::set_annotation_file_attachment(int64_t p_annotation_id, const String& p_name, const PackedByteArray& p_bytes) {
		PDF_WRITE_OR_RETURN(annotations, state->error);
		PDF_WRITE_ANNOTATION_OR_RETURN(state->error)
		if (!fits_unsigned_long(p_bytes.size()) || p_name.is_empty() ||
			api.FPDFAnnot_GetSubtype(annotation->handle) != FPDF_ANNOT_FILEATTACHMENT) {
			return state->fail(ERR_INVALID_PARAMETER, "A file attachment annotation, file name, and supported byte count are required.");
		}
		FPDF_ATTACHMENT attachment = api.FPDFAnnot_GetFileAttachment(annotation->handle);
		if (attachment == nullptr) {
			const Char16String name = p_name.utf16();
			attachment = api.FPDFAnnot_AddFileAttachment(annotation->handle, wide(name));
		}
		return state->checked(attachment != nullptr && api.FPDFAttachment_SetFile(attachment, state->document, p_bytes.ptr(),
			static_cast<unsigned long>(p_bytes.size())), "Could not write the PDF annotation file attachment.");
	}

	Error PDFWriter::set_annotation_appearance(int64_t p_annotation_id, int64_t p_mode, const Variant& p_appearance) {
		PDF_WRITE_OR_RETURN(annotation_appearance, state->error);
		PDF_WRITE_ANNOTATION_OR_RETURN(state->error)
		const bool clear = p_appearance.get_type() == Variant::NIL;
		if (p_mode < APPEARANCE_NORMAL || p_mode > APPEARANCE_DOWN || (!clear && p_appearance.get_type() != Variant::STRING)) {
			return state->fail(ERR_INVALID_PARAMETER, "Invalid PDF annotation appearance mode or stream.");
		}
		const Char16String appearance = clear ? Char16String() : String(p_appearance).utf16();
		if (!api.FPDFAnnot_SetAP(annotation->handle, int(p_mode), clear ? nullptr : wide(appearance))) {
			return state->fail(FAILED, "Could not set the PDF annotation appearance stream.");
		}
		return state->refresh_annotation(p_annotation_id);
	}

	Error PDFWriter::set_annotation_font_color(int64_t p_annotation_id, const Color& p_color) {
		PDF_WRITE_OR_RETURN(annotation_appearance, state->error);
		PDF_WRITE_ANNOTATION_OR_RETURN(state->error)
		if (!state->capability(Group::writer_forms)) {
			return state->error;
		}
		if (api.FPDFAnnot_GetSubtype(annotation->handle) != FPDF_ANNOT_FREETEXT) {
			return state->fail(ERR_INVALID_PARAMETER, "PDFium only sets annotation font color on FreeText annotations.");
		}
		State::FormSession form(*state, *annotation);
		if (form.handle == nullptr) {
			return state->error;
		}
		uint32_t rgba;
		if (!native_color(p_color, rgba)) {
			return state->fail(ERR_INVALID_PARAMETER, "PDF color components must be finite.");
		}
		return state->checked(api.FPDFAnnot_SetFontColor(form.handle, annotation->handle, rgba >> 24, (rgba >> 16) & 255,
			(rgba >> 8) & 255), "Could not set the PDF annotation font color.");
	}

	int64_t PDFWriter::get_annotation_object_count(int64_t p_annotation_id) {
		PDF_WRITE_OR_RETURN(annotation_appearance, 0);
		PDF_WRITE_ANNOTATION_OR_RETURN(0)
		if (!api.FPDFAnnot_IsObjectSupportedSubtype(api.FPDFAnnot_GetSubtype(annotation->handle))) {
			state->invalid("PDFium supports editing appearance objects only on Ink and Stamp annotations.");
			return 0;
		}
		return api.FPDFAnnot_GetObjectCount(annotation->handle);
	}

	int64_t PDFWriter::get_annotation_object_id(int64_t p_annotation_id, int64_t p_index) {
		PDF_WRITE_OR_RETURN(annotation_appearance, 0);
		PDF_WRITE_ANNOTATION_OR_RETURN(0)
		if (!api.FPDFAnnot_IsObjectSupportedSubtype(api.FPDFAnnot_GetSubtype(annotation->handle)) ||
			p_index < 0 || p_index >= api.FPDFAnnot_GetObjectCount(annotation->handle)) {
			state->invalid("Invalid PDF annotation appearance subtype or object index.");
			return 0;
		}
		return state->remember_object(api.FPDFAnnot_GetObject(annotation->handle, int(p_index)), annotation->page, 0, p_annotation_id);
	}

	Error PDFWriter::append_annotation_object(int64_t p_annotation_id, int64_t p_object_id) {
		PDF_WRITE_OR_RETURN(annotation_appearance, state->error);
		PDF_WRITE_ANNOTATION_OR_RETURN(state->error)
		PDF_WRITE_OBJECT_OR_RETURN(0, state->error)
		if (!state->capability(Group::writer_objects)) {
			return state->error;
		}
		const int type = api.FPDFPageObj_GetType(object->handle);
		if (!object->is_detached() || (type != FPDF_PAGEOBJ_TEXT && type != FPDF_PAGEOBJ_PATH && type != FPDF_PAGEOBJ_IMAGE) ||
			!api.FPDFAnnot_IsObjectSupportedSubtype(api.FPDFAnnot_GetSubtype(annotation->handle))) {
			return state->fail(ERR_INVALID_PARAMETER, "A detached path, text, or image and an Ink or Stamp annotation are required.");
		}
		if (!api.FPDFAnnot_AppendObject(annotation->handle, object->handle)) {
			return state->fail(FAILED, "Could not append the PDF annotation appearance object.");
		}
		object->annotation = p_annotation_id;
		object->page = annotation->page;
		return OK;
	}

	Error PDFWriter::remove_annotation_object(int64_t p_annotation_id, int64_t p_index) {
		PDF_WRITE_OR_RETURN(annotation_appearance, state->error);
		PDF_WRITE_ANNOTATION_OR_RETURN(state->error)
		if (!api.FPDFAnnot_IsObjectSupportedSubtype(api.FPDFAnnot_GetSubtype(annotation->handle)) ||
			p_index < 0 || p_index >= api.FPDFAnnot_GetObjectCount(annotation->handle)) {
			return state->fail(ERR_INVALID_PARAMETER, "Invalid PDF annotation appearance subtype or object index.");
		}
		const FPDF_PAGEOBJECT object = api.FPDFAnnot_GetObject(annotation->handle, int(p_index));
		const auto found = state->object_ids.find(object);
		const int64_t id = found != state->object_ids.end() ? found->second : 0;
		if (!api.FPDFAnnot_RemoveObject(annotation->handle, int(p_index))) {
			return state->fail(FAILED, "Could not remove the PDF annotation appearance object.");
		}
		if (id != 0) {
			state->erase_descendants(id);
			state->erase_object(id);
		}
		return OK;
	}

	PDFWriter::State::FormSession::FormSession(State& p_state, const Annotation& p_annotation) : state(p_state) {
		info.index = p_annotation.page;
		info.page = state.pages.at(info.index).handle;
		handle = state.api->FPDFDOC_InitFormFillEnvironment(state.document, &info);
		if (handle == nullptr) {
			state.fail(ERR_CANT_CREATE, "Could not initialize the PDF form environment.");
			return;
		}
		state.api->FORM_OnAfterLoadPage(info.page, handle);
	}

	PDFWriter::State::FormSession::~FormSession() {
		if (handle != nullptr) {
			if (focused) {
				state.api->FORM_ForceToKillFocus(handle);
			}
			state.api->FORM_OnBeforeClosePage(info.page, handle);
			state.api->FPDFDOC_ExitFormFillEnvironment(handle);
		}
	}

	bool PDFWriter::State::FormSession::focus(const Annotation& p_annotation, int p_type, bool p_editable) {
		const Api& api = *state.api;
		if (api.FPDF_GetFormType(state.document) != FORMTYPE_ACRO_FORM ||
			api.FPDFAnnot_GetSubtype(p_annotation.handle) != FPDF_ANNOT_WIDGET ||
			(p_type >= 0 && api.FPDFAnnot_GetFormFieldType(handle, p_annotation.handle) != p_type)) {
			return state.invalid("The annotation must be an existing AcroForm widget of the requested type.");
		}
		if (p_editable && (api.FPDFAnnot_GetFormFieldFlags(handle, p_annotation.handle) & FPDF_FORMFLAG_READONLY)) {
			return state.invalid("The PDF form field is read-only.");
		}
		focused = api.FORM_SetFocusedAnnot(handle, p_annotation.handle);
		return state.checked(focused, "Could not focus the PDF form field.") == OK;
	}

	bool PDFWriter::State::FormSession::commit() {
		if (!state.api->FORM_ForceToKillFocus(handle)) {
			state.fail(FAILED, "Could not commit the PDF form field change.");
			return false;
		}
		focused = false;
		return true;
	}

	Error PDFWriter::set_form_text(int64_t p_annotation_id, const String& p_text) {
		PDF_WRITE_OR_RETURN(forms, state->error);
		PDF_WRITE_ANNOTATION_OR_RETURN(state->error)
		State::FormSession form(*state, *annotation);
		if (form.handle == nullptr || !form.focus(*annotation, FPDF_FORMFIELD_TEXTFIELD, true)) {
			return state->error;
		}
		const String before = form_value(api, form.handle, annotation->handle);
		auto replace_text = [&](const String& p_current, const String& p_replacement) {
			if (!p_current.is_empty() && !api.FORM_SelectAllText(form.handle, form.info.page)) {
				state->fail(FAILED, "Could not select the current PDF form field text.");
				return false;
			}
			const Char16String value = p_replacement.utf16();
			api.FORM_ReplaceSelection(form.handle, form.info.page, wide(value));
			return form.commit();
		};
		if (!replace_text(before, p_text)) {
			return state->error;
		}
		const String after = form_value(api, form.handle, annotation->handle);
		if (normalized_form_text(after) != normalized_form_text(p_text)) {
			if (after != before && form.focus(*annotation, FPDF_FORMFIELD_TEXTFIELD, true)) {
				replace_text(after, before);
			}
			return state->fail(ERR_INVALID_DATA, "The PDF form field did not accept the requested text.");
		}
		return OK;
	}

	Error PDFWriter::set_form_option_selected(int64_t p_annotation_id, int64_t p_option_index, bool p_selected) {
		PDF_WRITE_OR_RETURN(forms, state->error);
		PDF_WRITE_ANNOTATION_OR_RETURN(state->error)
		State::FormSession form(*state, *annotation);
		if (form.handle == nullptr) {
			return state->error;
		}
		const int type = api.FPDFAnnot_GetFormFieldType(form.handle, annotation->handle);
		if ((type != FPDF_FORMFIELD_COMBOBOX && type != FPDF_FORMFIELD_LISTBOX) ||
			p_option_index < 0 || p_option_index >= api.FPDFAnnot_GetOptionCount(form.handle, annotation->handle)) {
			return state->fail(ERR_INVALID_PARAMETER, "A choice field and an in-bounds option index are required.");
		}
		if (!form.focus(*annotation, type, true)) {
			return state->error;
		}
		const int option = int(p_option_index);
		if (bool(api.FPDFAnnot_IsOptionSelected(form.handle, annotation->handle, option)) != p_selected) {
			if (!api.FORM_SetIndexSelected(form.handle, form.info.page, option, p_selected)) {
				return state->fail(FAILED, "Could not set the PDF form option selection.");
			}
		}
		if (!form.commit()) {
			return state->error;
		}
		return state->checked(bool(api.FPDFAnnot_IsOptionSelected(form.handle, annotation->handle, option)) == p_selected,
			"The PDF form field did not accept the requested option selection.");
	}

	Error PDFWriter::State::edit_button(Annotation& p_annotation, int p_type, bool p_checked) {
		FormSession form(*this, p_annotation);
		if (form.handle == nullptr || !form.focus(p_annotation, p_type, true)) {
			return error;
		}
		if (bool(api->FPDFAnnot_IsChecked(form.handle, p_annotation.handle)) != p_checked &&
			!api->FORM_OnChar(form.handle, form.info.page, ' ', 0)) {
			return fail(FAILED, "Could not activate the PDF form button.");
		}
		if (!form.commit()) {
			return error;
		}
		return checked(bool(api->FPDFAnnot_IsChecked(form.handle, p_annotation.handle)) == p_checked,
			"The PDF form field did not accept the requested checked state.");
	}

	Error PDFWriter::set_form_checked(int64_t p_annotation_id, bool p_checked) {
		PDF_WRITE_OR_RETURN(forms, state->error);
		PDF_WRITE_ANNOTATION_OR_RETURN(state->error)
		return state->edit_button(*annotation, FPDF_FORMFIELD_CHECKBOX, p_checked);
	}

	Error PDFWriter::select_form_radio(int64_t p_annotation_id) {
		PDF_WRITE_OR_RETURN(forms, state->error);
		PDF_WRITE_ANNOTATION_OR_RETURN(state->error)
		return state->edit_button(*annotation, FPDF_FORMFIELD_RADIOBUTTON, true);
	}

	Error PDFWriter::set_form_field_flags(int64_t p_annotation_id, int64_t p_flags) {
		PDF_WRITE_OR_RETURN(forms, state->error);
		PDF_WRITE_ANNOTATION_OR_RETURN(state->error)
		if (p_flags < 0 || p_flags > std::numeric_limits<int>::max() || api.FPDF_GetFormType(state->document) != FORMTYPE_ACRO_FORM ||
			api.FPDFAnnot_GetSubtype(annotation->handle) != FPDF_ANNOT_WIDGET) {
			return state->fail(ERR_INVALID_PARAMETER, "An existing AcroForm widget and a 32-bit flag mask are required.");
		}
		State::FormSession form(*state, *annotation);
		if (form.handle == nullptr) {
			return state->error;
		}
		return state->checked(api.FPDFAnnot_SetFormFieldFlags(form.handle, annotation->handle, int(p_flags)),
			"Could not set the PDF form field flags.");
	}
} // namespace godot
#endif

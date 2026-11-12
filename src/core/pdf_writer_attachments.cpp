#include "pdf_writer_internal.h"

#ifdef DOCRENDER_PDFIUM_ENABLED
#include "pdf_reader.h"

namespace godot {
	using namespace pdfium;

	int64_t PDFWriter::add_attachment(const String& p_name, const PackedByteArray& p_bytes) {
		PDF_WRITE_OR_RETURN(attachments, -1);
		if (p_name.is_empty() || !fits_unsigned_long(p_bytes.size())) {
			state->invalid("A PDF attachment name and supported byte count are required.");
			return -1;
		}
		const Char16String name = p_name.utf16();
		const FPDF_ATTACHMENT attachment = api.FPDFDoc_AddAttachment(state->document, wide(name));
		if (attachment == nullptr) {
			state->fail(ERR_CANT_CREATE, "Could not add the PDF attachment. Attachment names must be unique.");
			return -1;
		}
		const int count = api.FPDFDoc_GetAttachmentCount(state->document);
		for (int i = 0; i < count; ++i) {
			if (api.FPDFDoc_GetAttachment(state->document, i) != attachment) {
				continue;
			}
			if (api.FPDFAttachment_SetFile(attachment, state->document, p_bytes.ptr(), static_cast<unsigned long>(p_bytes.size()))) {
				return i;
			}
			api.FPDFDoc_DeleteAttachment(state->document, i);
			state->fail(ERR_CANT_CREATE, "Could not write the PDF attachment data.");
			return -1;
		}
		state->fail(ERR_CANT_ACQUIRE_RESOURCE, "Could not locate the newly added PDF attachment.");
		return -1;
	}

	Error PDFWriter::delete_attachment(int64_t p_index) {
		PDF_WRITE_OR_RETURN(attachments, state->error);
		if (state->attachment(p_index) == nullptr) {
			return state->error;
		}
		return state->checked(api.FPDFDoc_DeleteAttachment(state->document, int(p_index)), "Could not delete the PDF attachment.");
	}

	Error PDFWriter::set_attachment_data(int64_t p_index, const PackedByteArray& p_bytes) {
		PDF_WRITE_OR_RETURN(attachments, state->error);
		const FPDF_ATTACHMENT attachment = state->attachment(p_index);
		if (attachment == nullptr) {
			return state->error;
		}
		if (!fits_unsigned_long(p_bytes.size())) {
			return state->fail(ERR_INVALID_PARAMETER, "PDF attachment data exceeds the SDK byte limit.");
		}
		return state->checked(api.FPDFAttachment_SetFile(attachment, state->document, p_bytes.ptr(),
			static_cast<unsigned long>(p_bytes.size())), "Could not replace the PDF attachment data.");
	}

	Error PDFWriter::set_attachment_description(int64_t p_index, const String& p_description) {
		PDF_WRITE_OR_RETURN(attachments, state->error);
		const FPDF_ATTACHMENT attachment = state->attachment(p_index);
		if (attachment == nullptr) {
			return state->error;
		}
		const Char16String description = p_description.utf16();
		return state->checked(api.FPDFAttachment_SetDescription(attachment, wide(description)), "Could not set the PDF attachment description.");
	}

	Error PDFWriter::set_attachment_string(int64_t p_index, const String& p_key, const String& p_value) {
		PDF_WRITE_OR_RETURN(attachments, state->error);
		const FPDF_ATTACHMENT attachment = state->attachment(p_index);
		if (attachment == nullptr) {
			return state->error;
		}
		if (p_key.is_empty()) {
			return state->fail(ERR_INVALID_PARAMETER, "A PDF attachment property name is required.");
		}
		const CharString key = p_key.utf8();
		const Char16String value = p_value.utf16();
		return state->checked(api.FPDFAttachment_SetStringValue(attachment, key.get_data(), wide(value)),
			"Could not set the PDF attachment string property.");
	}

	Error PDFWriter::set_document_language(const String& p_language) {
		PDF_WRITE_OR_RETURN(catalog, state->error);
		const Char16String language = p_language.utf16();
		return state->checked(api.FPDFCatalog_SetLanguage(state->document, wide(language)), "Could not set the PDF document language.");
	}

	Error PDFWriter::copy_viewer_preferences(const Ref<PDFReader>& p_reader) {
		PDF_WRITE_OR_RETURN(catalog, state->error);
		const FPDF_DOCUMENT source = state->source_document(p_reader);
		if (source == nullptr) {
			return state->error;
		}
		return state->checked(api.FPDF_CopyViewerPreferences(state->document, source), "Could not copy the PDF viewer preferences.");
	}
} // namespace godot
#endif

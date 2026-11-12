#include "pdf_writer_internal.h"

#ifdef DOCRENDER_PDFIUM_ENABLED
#include <godot_cpp/variant/char_string.hpp>

#include <cmath>
#include <cstring>

namespace godot {
	using namespace pdfium;

	namespace {
		bool valid_font_data(const PackedByteArray& p_data) {
			return !p_data.is_empty() && uint64_t(p_data.size()) <= std::numeric_limits<uint32_t>::max();
		}

		bool valid_size(float p_size) {
			return std::isfinite(p_size) && p_size >= 0;
		}

		struct JpegSource : FPDF_FILEACCESS {
			const PackedByteArray& bytes;

			explicit JpegSource(const PackedByteArray& p_bytes) :
					FPDF_FILEACCESS{ static_cast<unsigned long>(p_bytes.size()), read, this }, bytes(p_bytes) {}

			static int read(void* p_source, unsigned long p_offset, unsigned char* p_buffer, unsigned long p_length) {
				const auto& source = *static_cast<JpegSource*>(p_source);
				if (p_offset > source.m_FileLen || p_length > source.m_FileLen - p_offset) {
					return 0;
				}
				std::memcpy(p_buffer, source.bytes.ptr() + p_offset, p_length);
				return 1;
			}
		};
	}

	int64_t PDFWriter::load_standard_font(const String& p_name) {
		PDF_WRITE_OR_RETURN(fonts, 0);
		const CharString name = p_name.utf8();
		return state->remember_font(api.FPDFText_LoadStandardFont(state->document, name.get_data()));
	}

	int64_t PDFWriter::load_font(const PackedByteArray& p_data, int64_t p_type, bool p_cid) {
		PDF_WRITE_OR_RETURN(fonts, 0);
		if (!valid_font_data(p_data) || (p_type != FONT_TYPE1 && p_type != FONT_TRUETYPE)) {
			state->invalid("Invalid PDF font data or font type.");
			return 0;
		}
		return state->remember_font(api.FPDFText_LoadFont(state->document, p_data.ptr(),
			uint32_t(p_data.size()), int(p_type), p_cid));
	}

	int64_t PDFWriter::load_cid_type2_font(const PackedByteArray& p_data, const String& p_to_unicode,
		const PackedByteArray& p_cid_to_gid) {
		PDF_WRITE_OR_RETURN(fonts, 0);
		if (!valid_font_data(p_data) || !valid_font_data(p_cid_to_gid) || p_cid_to_gid.size() % 2 != 0 || p_to_unicode.is_empty()) {
			state->invalid("Invalid CID font, ToUnicode CMap, or CID-to-GID map.");
			return 0;
		}
		const CharString cmap = p_to_unicode.utf8();
		return state->remember_font(api.FPDFText_LoadCidType2Font(state->document, p_data.ptr(), uint32_t(p_data.size()),
			cmap.get_data(), p_cid_to_gid.ptr(), uint32_t(p_cid_to_gid.size())));
	}

	Error PDFWriter::close_font(int64_t p_font_id) {
		PDF_WRITE_OR_RETURN(fonts, state->error);
		const auto it = state->fonts.find(p_font_id);
		if (it == state->fonts.end()) {
			return state->fail(ERR_INVALID_PARAMETER, "The PDF font ID is invalid or expired.");
		}
		api.FPDFFont_Close(it->second);
		state->fonts.erase(it);
		return OK;
	}

	int64_t PDFWriter::create_text(int64_t p_font_id, float p_font_size, const String& p_text) {
		PDF_WRITE_OR_RETURN(text, 0);
		const auto font = state->fonts.find(p_font_id);
		if (font == state->fonts.end() || !valid_size(p_font_size) || p_text.is_empty()) {
			state->invalid("An open PDF font, a finite nonnegative size, and non-empty text are required.");
			return 0;
		}
		const FPDF_PAGEOBJECT object = api.FPDFPageObj_CreateTextObj(state->document, font->second, p_font_size);
		if (object == nullptr) {
			state->fail(ERR_CANT_CREATE, "Could not create the PDF text object.");
			return 0;
		}
		const Char16String text = p_text.utf16();
		if (!api.FPDFText_SetText(object, wide(text))) {
			api.FPDFPageObj_Destroy(object);
			state->fail(ERR_INVALID_DATA, "Could not set the text with this PDF font.");
			return 0;
		}
		return state->remember_object(object);
	}

	Error PDFWriter::set_text(int64_t p_object_id, const String& p_text) {
		PDF_WRITE_OR_RETURN(text, state->error);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(FPDF_PAGEOBJ_TEXT, state->error)
		if (p_text.is_empty()) {
			return state->fail(ERR_INVALID_PARAMETER, "PDF text cannot be empty.");
		}
		const Char16String text = p_text.utf16();
		return state->finish_object(*object, api.FPDFText_SetText(object->handle, wide(text)));
	}

	Error PDFWriter::set_text_charcodes(int64_t p_object_id, const PackedInt64Array& p_charcodes) {
		PDF_WRITE_OR_RETURN(text, state->error);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(FPDF_PAGEOBJ_TEXT, state->error)
		if (p_charcodes.is_empty()) {
			return state->fail(ERR_INVALID_PARAMETER, "At least one PDF character code is required.");
		}
		std::vector<uint32_t> codes;
		codes.reserve(size_t(p_charcodes.size()));
		for (int64_t i = 0; i < p_charcodes.size(); ++i) {
			const int64_t code = p_charcodes[i];
			if (code < 0 || uint64_t(code) > std::numeric_limits<uint32_t>::max()) {
				return state->fail(ERR_INVALID_PARAMETER, "PDF character codes must be unsigned 32-bit values.");
			}
			codes.push_back(uint32_t(code));
		}
		return state->finish_object(*object, api.FPDFText_SetCharcodes(object->handle, codes.data(), codes.size()));
	}

	Error PDFWriter::set_text_positions(int64_t p_object_id, const PackedFloat32Array& p_positions) {
		PDF_WRITE_OR_RETURN(text, state->error);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(FPDF_PAGEOBJ_TEXT, state->error)
		if (p_positions.is_empty()) {
			return state->fail(ERR_INVALID_PARAMETER, "PDF character positions require at least two characters.");
		}
		for (int64_t i = 0; i < p_positions.size(); ++i) {
			if (!std::isfinite(p_positions[i])) {
				return state->fail(ERR_INVALID_PARAMETER, "PDF character positions must be finite.");
			}
		}
		return state->finish_object(*object, api.FPDFText_SetPositions(object->handle, p_positions.ptr(), size_t(p_positions.size())));
	}

	Error PDFWriter::set_text_font_size(int64_t p_object_id, float p_size) {
		PDF_WRITE_OR_RETURN(text, state->error);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(FPDF_PAGEOBJ_TEXT, state->error)
		if (!valid_size(p_size)) {
			return state->fail(ERR_INVALID_PARAMETER, "PDF font size must be finite and nonnegative.");
		}
		return state->finish_object(*object, api.FPDFTextObj_SetFontSize(object->handle, p_size));
	}

	Error PDFWriter::set_text_render_mode(int64_t p_object_id, int64_t p_mode) {
		PDF_WRITE_OR_RETURN(text, state->error);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(FPDF_PAGEOBJ_TEXT, state->error)
		if (p_mode < TEXT_FILL || p_mode > TEXT_CLIP) {
			return state->fail(ERR_INVALID_PARAMETER, "Invalid PDF text render mode.");
		}
		return state->finish_object(*object, api.FPDFTextObj_SetTextRenderMode(object->handle, FPDF_TEXT_RENDERMODE(p_mode)));
	}

	int64_t PDFWriter::create_image(const Ref<Image>& p_image) {
		PDF_WRITE_OR_RETURN(images, 0);
		const int64_t id = state->remember_object(api.FPDFPageObj_NewImageObj(state->document));
		if (id != 0 && set_image(id, p_image) != OK) {
			api.FPDFPageObj_Destroy(state->objects.at(id).handle);
			state->erase_object(id);
			return 0;
		}
		return id;
	}

	int64_t PDFWriter::create_jpeg(const PackedByteArray& p_bytes) {
		PDF_WRITE_OR_RETURN(images, 0);
		const int64_t id = state->remember_object(api.FPDFPageObj_NewImageObj(state->document));
		if (id != 0 && set_jpeg(id, p_bytes) != OK) {
			api.FPDFPageObj_Destroy(state->objects.at(id).handle);
			state->erase_object(id);
			return 0;
		}
		return id;
	}

	Error PDFWriter::set_image(int64_t p_object_id, const Ref<Image>& p_image) {
		PDF_WRITE_OR_RETURN(images, state->error);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(FPDF_PAGEOBJ_IMAGE, state->error)
		if (p_image.is_null() || !fits_bitmap(p_image->get_width(), p_image->get_height())) {
			return state->fail(ERR_INVALID_PARAMETER, "PDF image dimensions must fit within 8192 pixels per side and 64 MiB.");
		}
		Ref<Image> image = p_image;
		if (image->get_format() != Image::FORMAT_RGBA8) {
			image = p_image->duplicate();
			if (image.is_null() || (image->is_compressed() && image->decompress() != OK)) {
				return state->fail(ERR_INVALID_DATA, "Could not decompress the PDF image pixels.");
			}
			image->convert(Image::FORMAT_RGBA8);
		}
		const int width = image->get_width();
		const int height = image->get_height();
		PackedByteArray pixels = image->get_data();
		const int64_t length = int64_t(width) * height * 4;
		if (image->get_format() != Image::FORMAT_RGBA8 || pixels.size() < length) {
			return state->fail(ERR_INVALID_DATA, "Could not convert the PDF image to RGBA8.");
		}
		uint8_t* bgra = pixels.ptrw();
		for (int64_t i = 0; i < length; i += 4) {
			std::swap(bgra[i], bgra[i + 2]);
		}
		const auto bitmap = scoped(api.FPDFBitmap_CreateEx(width, height, FPDFBitmap_BGRA, bgra, width * 4), api.FPDFBitmap_Destroy);
		if (!bitmap) {
			return state->fail(ERR_CANT_CREATE, "Could not create the PDF image bitmap.");
		}
		std::vector<FPDF_PAGE> loaded;
		for (const auto& [index, page] : state->pages) {
			loaded.push_back(page.handle);
		}
		return state->finish_object(*object, api.FPDFImageObj_SetBitmap(loaded.data(), int(loaded.size()),
			object->handle, bitmap.get()));
	}

	Error PDFWriter::set_jpeg(int64_t p_object_id, const PackedByteArray& p_bytes) {
		PDF_WRITE_OR_RETURN(images, state->error);
		PDF_WRITE_EDITABLE_OBJECT_OR_RETURN(FPDF_PAGEOBJ_IMAGE, state->error)
		if (p_bytes.size() < 3 || !fits_unsigned_long(p_bytes.size()) ||
			p_bytes[0] != 0xff || p_bytes[1] != 0xd8 || p_bytes[2] != 0xff) {
			return state->fail(ERR_INVALID_PARAMETER, "Valid JPEG bytes are required.");
		}
		std::vector<FPDF_PAGE> loaded;
		for (const auto& [index, page] : state->pages) {
			loaded.push_back(page.handle);
		}
		JpegSource source(p_bytes);
		return state->finish_object(*object, api.FPDFImageObj_LoadJpegFileInline(loaded.data(), int(loaded.size()),
			object->handle, &source));
	}
} // namespace godot
#endif

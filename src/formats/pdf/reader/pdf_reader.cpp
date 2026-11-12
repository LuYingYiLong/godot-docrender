#include "pdf_reader_internal.h"

#include <algorithm>
#include <cmath>

namespace godot {
	PDFReader::PDFReader() : state(std::make_unique<State>()) {}

	PDFReader::~PDFReader() = default;

	void PDFReader::_bind_methods() {
		ClassDB::bind_method(D_METHOD("open_file", "path", "password"), &PDFReader::open_file, DEFVAL(String()));
		ClassDB::bind_method(D_METHOD("open_buffer", "bytes", "password"), &PDFReader::open_buffer, DEFVAL(String()));
		ClassDB::bind_method(D_METHOD("close"), &PDFReader::close);
		ClassDB::bind_method(D_METHOD("is_open"), &PDFReader::is_open);
		ClassDB::bind_static_method("PDFReader", D_METHOD("is_available"), &PDFReader::is_available);
		ClassDB::bind_static_method("PDFReader", D_METHOD("get_capabilities"), &PDFReader::get_capabilities);
		ClassDB::bind_method(D_METHOD("get_last_error"), &PDFReader::get_last_error);
		ClassDB::bind_method(D_METHOD("get_last_error_message"), &PDFReader::get_last_error_message);
		ClassDB::bind_method(D_METHOD("get_document_info"), &PDFReader::get_document_info);
		ClassDB::bind_method(D_METHOD("get_metadata"), &PDFReader::get_metadata);
		ClassDB::bind_method(D_METHOD("get_page_count"), &PDFReader::get_page_count);
		ClassDB::bind_method(D_METHOD("get_page_info", "page_index"), &PDFReader::get_page_info);
		ClassDB::bind_method(D_METHOD("render_page", "page_index", "dpi", "flags", "rotation", "background"), &PDFReader::render_page, DEFVAL(144), DEFVAL(RENDER_ANNOTATIONS), DEFVAL(0), DEFVAL(Color(1, 1, 1, 1)));
		ClassDB::bind_method(D_METHOD("render_region", "page_index", "region", "dpi", "flags", "rotation", "background"), &PDFReader::render_region, DEFVAL(144), DEFVAL(RENDER_ANNOTATIONS), DEFVAL(0), DEFVAL(Color(1, 1, 1, 1)));
		ClassDB::bind_method(D_METHOD("get_page_thumbnail", "page_index"), &PDFReader::get_page_thumbnail);
		ClassDB::bind_method(D_METHOD("page_to_pixel", "page_index", "position", "dpi", "rotation"), &PDFReader::page_to_pixel, DEFVAL(144), DEFVAL(0));
		ClassDB::bind_method(D_METHOD("pixel_to_page", "page_index", "position", "dpi", "rotation"), &PDFReader::pixel_to_page, DEFVAL(144), DEFVAL(0));
		ClassDB::bind_method(D_METHOD("get_page_char_count", "page_index"), &PDFReader::get_page_char_count);
		ClassDB::bind_method(D_METHOD("get_page_text", "page_index", "start", "count"), &PDFReader::get_page_text, DEFVAL(0), DEFVAL(-1));
		ClassDB::bind_method(D_METHOD("get_page_text_in_rect", "page_index", "rect"), &PDFReader::get_page_text_in_rect);
		ClassDB::bind_method(D_METHOD("get_char_info", "page_index", "char_index"), &PDFReader::get_char_info);
		ClassDB::bind_method(D_METHOD("get_text_rects", "page_index", "start", "count"), &PDFReader::get_text_rects, DEFVAL(0), DEFVAL(-1));
		ClassDB::bind_method(D_METHOD("hit_test_text", "page_index", "position", "tolerance"), &PDFReader::hit_test_text, DEFVAL(Vector2(2, 2)));
		ClassDB::bind_method(D_METHOD("find_text", "page_index", "query", "flags"), &PDFReader::find_text, DEFVAL(0));
		ClassDB::bind_method(D_METHOD("get_outline"), &PDFReader::get_outline);
		ClassDB::bind_method(D_METHOD("get_named_destinations"), &PDFReader::get_named_destinations);
		ClassDB::bind_method(D_METHOD("get_page_links", "page_index"), &PDFReader::get_page_links);
		ClassDB::bind_method(D_METHOD("get_page_web_links", "page_index"), &PDFReader::get_page_web_links);
		ClassDB::bind_method(D_METHOD("get_page_annotations", "page_index"), &PDFReader::get_page_annotations);
		ClassDB::bind_method(D_METHOD("get_form_fields", "page_index"), &PDFReader::get_form_fields);
		ClassDB::bind_method(D_METHOD("get_annotation_string", "page_index", "annotation_index", "key"), &PDFReader::get_annotation_string);
		ClassDB::bind_method(D_METHOD("get_attachments"), &PDFReader::get_attachments);
		ClassDB::bind_method(D_METHOD("get_attachment_data", "index"), &PDFReader::get_attachment_data);
		ClassDB::bind_method(D_METHOD("get_signatures"), &PDFReader::get_signatures);
		ClassDB::bind_method(D_METHOD("get_signature_contents", "index"), &PDFReader::get_signature_contents);
		ClassDB::bind_method(D_METHOD("get_page_structure", "page_index"), &PDFReader::get_page_structure);
		ClassDB::bind_method(D_METHOD("get_page_objects", "page_index"), &PDFReader::get_page_objects);
		ClassDB::bind_method(D_METHOD("get_page_object_image", "page_index", "object_path", "rendered"), &PDFReader::get_page_object_image, DEFVAL(false));
		ClassDB::bind_method(D_METHOD("get_page_object_image_data", "page_index", "object_path", "decoded"), &PDFReader::get_page_object_image_data, DEFVAL(false));
		ClassDB::bind_method(D_METHOD("get_page_object_font_data", "page_index", "object_path"), &PDFReader::get_page_object_font_data);
		ClassDB::bind_method(D_METHOD("get_javascript_actions"), &PDFReader::get_javascript_actions);
		ClassDB::bind_method(D_METHOD("get_xfa_packets"), &PDFReader::get_xfa_packets);
		BIND_ENUM_CONSTANT(RENDER_ANNOTATIONS);
		BIND_ENUM_CONSTANT(RENDER_GRAYSCALE);
		BIND_ENUM_CONSTANT(RENDER_FORM_WIDGETS);
		BIND_ENUM_CONSTANT(RENDER_PRINTING);
		BIND_ENUM_CONSTANT(RENDER_LCD_TEXT);
		BIND_ENUM_CONSTANT(RENDER_NO_NATIVE_TEXT);
		BIND_ENUM_CONSTANT(RENDER_LIMITED_IMAGE_CACHE);
		BIND_ENUM_CONSTANT(RENDER_FORCE_HALFTONE);
		BIND_ENUM_CONSTANT(RENDER_NO_SMOOTH_TEXT);
		BIND_ENUM_CONSTANT(RENDER_NO_SMOOTH_IMAGE);
		BIND_ENUM_CONSTANT(RENDER_NO_SMOOTH_PATH);
		BIND_ENUM_CONSTANT(FIND_MATCH_CASE);
		BIND_ENUM_CONSTANT(FIND_WHOLE_WORD);
		BIND_ENUM_CONSTANT(FIND_CONSECUTIVE);
	}

	Error PDFReader::State::fail(Error p_error, const String& p_message) {
		error = p_error;
		message = p_message;
		return p_error;
	}

	void PDFReader::State::clear_error() {
		error = OK;
		message = String();
	}

	bool PDFReader::is_supported() {
#ifdef DOCRENDER_PDFIUM_ENABLED
		return true;
#else
		return false;
#endif
	}

	bool PDFReader::is_available() {
#ifdef DOCRENDER_PDFIUM_ENABLED
		String message;
		return bool(pdfium::acquire(message));
#else
		return false;
#endif
	}

	PackedStringArray PDFReader::get_capabilities() {
#ifdef DOCRENDER_PDFIUM_ENABLED
		String message;
		const auto api = pdfium::acquire(message);
		if (api) {
			return api->capabilities();
		}
#endif
		return {};
	}

	void PDFReader::shutdown_backend() {
#ifdef DOCRENDER_PDFIUM_ENABLED
		pdfium::shutdown();
#endif
	}

	Error PDFReader::get_last_error() const {
		std::lock_guard guard(pdfium::mutex());
		return state->error;
	}

	String PDFReader::get_last_error_message() const {
		std::lock_guard guard(pdfium::mutex());
		return state->message;
	}

	bool PDFReader::is_open() const {
		std::lock_guard guard(pdfium::mutex());
#ifdef DOCRENDER_PDFIUM_ENABLED
		return state->document != nullptr;
#else
		return false;
#endif
	}

	void PDFReader::close() {
		std::lock_guard guard(pdfium::mutex());
#ifdef DOCRENDER_PDFIUM_ENABLED
		state->release_document();
#endif
		state->clear_error();
	}

	Error PDFReader::open_file(const String& p_path, const String& p_password) {
		if (!is_supported()) {
			std::lock_guard guard(pdfium::mutex());
			return state->fail(ERR_UNAVAILABLE, "PDFium is unavailable in this build.");
		}
		PackedByteArray bytes;
		String message;
		const Error error = pdfium::read_file(p_path, bytes, message);
		if (error != OK) {
			std::lock_guard guard(pdfium::mutex());
			return state->fail(error, message);
		}
		return open_bytes(std::move(bytes), p_password, p_path);
	}

	Error PDFReader::open_buffer(const PackedByteArray& p_bytes, const String& p_password) {
		return open_bytes(p_bytes, p_password, "the PDF buffer");
	}

	Error PDFReader::open_bytes(PackedByteArray p_bytes, const String& p_password, const String& p_source) {
		std::lock_guard guard(pdfium::mutex());
#ifdef DOCRENDER_PDFIUM_ENABLED
		String message;
		auto api = pdfium::acquire(message);
		if (!api) {
			return state->fail(ERR_UNAVAILABLE, message);
		}
		Error error = OK;
		FPDF_DOCUMENT document = pdfium::load_document(*api, p_bytes, p_password, p_source, error, message);
		if (document == nullptr) {
			return state->fail(error, message);
		}

		state->release_document();
		state->api = std::move(api);
		state->bytes = std::move(p_bytes);
		state->document = document;
		state->clear_error();
		return OK;
#else
		return state->fail(ERR_UNAVAILABLE, "PDFium is unavailable in this build.");
#endif
	}

	Ref<Image> PDFReader::render_page(int64_t p_page, int64_t p_dpi, int64_t p_flags, int64_t p_rotation, const Color& p_background) {
		return render(p_page, nullptr, p_dpi, p_flags, p_rotation, p_background);
	}

	Ref<Image> PDFReader::render_region(int64_t p_page, const Rect2i& p_region, int64_t p_dpi, int64_t p_flags, int64_t p_rotation, const Color& p_background) {
		return render(p_page, &p_region, p_dpi, p_flags, p_rotation, p_background);
	}

#ifdef DOCRENDER_PDFIUM_ENABLED
	using namespace pdfium;

	namespace {
		struct RasterGeometry {
			Vector2i size;
			int rotation = 0;
		};

		bool get_raster_geometry(const Api& p_api, FPDF_PAGE p_page, int64_t p_dpi, int64_t p_rotation,
			RasterGeometry& r_raster, String& r_message) {
			if (p_dpi < 1 || p_dpi > std::numeric_limits<int32_t>::max()) {
				r_message = "DPI must be a positive 32-bit integer.";
				return false;
			}
			if (p_rotation < 0 || p_rotation > 270 || p_rotation % 90 != 0) {
				r_message = "Rotation must be 0, 90, 180, or 270 degrees.";
				return false;
			}
			const double scale = double(p_dpi) / 72.0;
			const double width = std::ceil(p_api.FPDF_GetPageWidthF(p_page) * scale);
			const double height = std::ceil(p_api.FPDF_GetPageHeightF(p_page) * scale);
			if (!std::isfinite(width) || !std::isfinite(height) || width < 1.0 || height < 1.0) {
				r_message = "The PDF page has invalid dimensions.";
				return false;
			}
			if (width > std::numeric_limits<int32_t>::max() || height > std::numeric_limits<int32_t>::max()) {
				r_message = "The full page dimensions exceed PDFium's pixel coordinate range. Lower the DPI.";
				return false;
			}
			r_raster.rotation = int(p_rotation / 90);
			r_raster.size = r_raster.rotation % 2 == 0 ? Vector2i(int(width), int(height)) : Vector2i(int(height), int(width));
			return true;
		}

		int pdfium_render_flags(int64_t p_flags) {
			constexpr int mapping[] = {
				FPDF_ANNOT, FPDF_GRAYSCALE, 0, FPDF_PRINTING, FPDF_LCD_TEXT, FPDF_NO_NATIVETEXT,
				FPDF_RENDER_LIMITEDIMAGECACHE, FPDF_RENDER_FORCEHALFTONE, FPDF_RENDER_NO_SMOOTHTEXT,
				FPDF_RENDER_NO_SMOOTHIMAGE, FPDF_RENDER_NO_SMOOTHPATH,
			};
			int flags = 0;
			for (size_t i = 0; i < std::size(mapping); ++i) {
				if ((p_flags & (int64_t(1) << i)) != 0) {
					flags |= mapping[i];
				}
			}
			return flags;
		}
	}

	PDFReader::State::~State() {
		std::lock_guard guard(pdfium::mutex());
		release_document();
	}

	void PDFReader::State::release_document() {
		if (form != nullptr) {
			api->FPDFDOC_ExitFormFillEnvironment(form);
			form = nullptr;
		}
		if (document != nullptr) {
			api->FPDF_CloseDocument(document);
			document = nullptr;
		}
		bytes.clear();
		api.reset();
	}

	bool PDFReader::State::require(Group p_group) {
		if (document == nullptr) {
			fail(ERR_UNCONFIGURED, "No PDF document is open.");
			return false;
		}
		if (!api->has(p_group)) {
			fail(ERR_UNAVAILABLE, "This PDFium build does not support the requested capability.");
			return false;
		}
		clear_error();
		return true;
	}

	Page PDFReader::State::page(int64_t p_page) {
		const int count = api->FPDF_GetPageCount(document);
		if (p_page < 0 || p_page >= count) {
			fail(ERR_INVALID_PARAMETER, vformat("Requested page index %d, but the PDF has %d pages.", p_page, count));
			return scoped(FPDF_PAGE(nullptr), api->FPDF_ClosePage);
		}
		auto loaded = scoped(api->FPDF_LoadPage(document, int(p_page)), api->FPDF_ClosePage);
		if (!loaded) {
			fail(ERR_FILE_CORRUPT, vformat("Could not load page index %d.", p_page));
		}
		return loaded;
	}

	TextPage PDFReader::State::text_page(FPDF_PAGE p_page) {
		auto loaded = scoped(api->FPDFText_LoadPage(p_page), api->FPDFText_ClosePage);
		if (!loaded) {
			fail(ERR_FILE_CORRUPT, "Could not read text from the PDF page.");
		}
		return loaded;
	}

	bool PDFReader::State::ensure_form() {
		if (!api->has(Group::forms)) {
			fail(ERR_UNAVAILABLE, "This PDFium build does not support forms.");
			return false;
		}
		if (form != nullptr) {
			return true;
		}

		form = api->FPDFDOC_InitFormFillEnvironment(document, &form_info);
		if (form == nullptr) {
			fail(ERR_CANT_CREATE, "Could not initialize the read-only PDF form environment.");
		}
		return form != nullptr;
	}

	PDFReader::State::FormPage::FormPage(State& p_state, FPDF_PAGE p_page, int p_index) : state(p_state), page(p_page) {
		state.form_info.page = page;
		state.form_info.index = p_index;
		state.api->FORM_OnAfterLoadPage(page, state.form);
	}

	PDFReader::State::FormPage::~FormPage() {
		state.api->FORM_OnBeforeClosePage(page, state.form);
		state.form_info.page = nullptr;
		state.form_info.index = -1;
	}

	int64_t PDFReader::get_page_count() {
		std::lock_guard guard(pdfium::mutex());
		return state->require(Group::core) ? state->api->FPDF_GetPageCount(state->document) : 0;
	}

	Ref<Image> PDFReader::render(int64_t p_page, const Rect2i* p_region, int64_t p_dpi, int64_t p_flags, int64_t p_rotation,
		const Color& p_background) {
		std::lock_guard guard(pdfium::mutex());
		if (!state->require(Group::core)) {
			return {};
		}
		if (p_flags < 0 || p_flags >= int64_t(RENDER_NO_SMOOTH_PATH) * 2) {
			state->fail(ERR_INVALID_PARAMETER, "Unknown PDF rendering flags.");
			return {};
		}
		const auto page = state->page(p_page);
		if (!page) {
			return {};
		}

		const Api& api = *state->api;
		RasterGeometry raster;
		String message;
		if (!get_raster_geometry(api, page.get(), p_dpi, p_rotation, raster, message)) {
			state->fail(ERR_INVALID_PARAMETER, message);
			return {};
		}
		const Rect2i region = p_region ? *p_region : Rect2i(Vector2i(), raster.size);
		if (region.position.x < 0 || region.position.y < 0 || region.size.x < 1 || region.size.y < 1 ||
			int64_t(region.position.x) + region.size.x > raster.size.x || int64_t(region.position.y) + region.size.y > raster.size.y) {
			state->fail(ERR_INVALID_PARAMETER, "The render region must be inside the full page bitmap.");
			return {};
		}
		if (!fits_bitmap(region.size.x, region.size.y)) {
			state->fail(ERR_INVALID_PARAMETER, region.size.x > MAX_BITMAP_EDGE || region.size.y > MAX_BITMAP_EDGE ?
				"The PDF output exceeds 8192 pixels on one side. Lower the DPI or use a smaller region." :
				"The PDF output exceeds the 64 MiB bitmap limit. Lower the DPI or use a smaller region.");
			return {};
		}
		if ((p_flags & RENDER_FORM_WIDGETS) != 0 && !state->ensure_form()) {
			return {};
		}

		PackedByteArray pixels;
		const int stride = region.size.x * 4;
		const Error allocation = static_cast<Error>(pixels.resize(int64_t(stride) * region.size.y));
		if (allocation != OK) {
			state->fail(allocation, "Could not allocate PDF pixels.");
			return {};
		}
		{
			const auto bitmap = scoped(api.FPDFBitmap_CreateEx(region.size.x, region.size.y, FPDFBitmap_BGRA, pixels.ptrw(), stride),
				api.FPDFBitmap_Destroy);
			if (!bitmap || !api.FPDFBitmap_FillRect(bitmap.get(), 0, 0, region.size.x, region.size.y, FPDF_DWORD(p_background.to_argb32()))) {
				state->fail(ERR_CANT_CREATE, "Could not create the PDF bitmap.");
				return {};
			}
			const int flags = pdfium_render_flags(p_flags);
			const Vector2i origin = -region.position;
			api.FPDF_RenderPageBitmap(bitmap.get(), page.get(), origin.x, origin.y, raster.size.x, raster.size.y, raster.rotation, flags);
			if ((p_flags & RENDER_FORM_WIDGETS) != 0) {
				const State::FormPage form_page(*state, page.get(), int(p_page));
				api.FPDF_FFLDraw(state->form, bitmap.get(), page.get(), origin.x, origin.y, raster.size.x, raster.size.y, raster.rotation, flags);
			}
		}
		// The fill and both render paths share BGRA ordering, including colored
		// backgrounds. Swap red and blue once after PDFium releases the buffer.
		uint8_t* rgba = pixels.ptrw();
		for (int64_t i = 0; i < pixels.size(); i += 4) {
			std::swap(rgba[i], rgba[i + 2]);
		}
		const Ref<Image> image = Image::create_from_data(region.size.x, region.size.y, false, Image::FORMAT_RGBA8, pixels);
		if (image.is_null() || image->is_empty()) {
			state->fail(ERR_CANT_CREATE, "Could not create the PDF image.");
			return {};
		}
		return image;
	}

	Ref<Image> PDFReader::get_page_thumbnail(int64_t p_page) {
		std::lock_guard guard(pdfium::mutex());
		if (!state->require(Group::thumbnail)) {
			return {};
		}
		const auto page = state->page(p_page);
		if (!page) {
			return {};
		}
		const Api& api = *state->api;
		const auto bitmap = scoped(api.FPDFPage_GetThumbnailAsBitmap(page.get()), api.FPDFBitmap_Destroy);
		if (!bitmap) {
			return {};
		}
		const Ref<Image> image = bitmap_to_image(api, bitmap.get());
		if (image.is_null()) {
			state->fail(ERR_CANT_CREATE, "The PDF thumbnail is invalid or exceeds the bitmap limit.");
		}
		return image;
	}

	Vector2i PDFReader::page_to_pixel(int64_t p_page, const Vector2& p_position, int64_t p_dpi, int64_t p_rotation) {
		std::lock_guard guard(pdfium::mutex());
		if (!state->require(Group::core)) {
			return {};
		}
		const auto page = state->page(p_page);
		if (!page) {
			return {};
		}
		RasterGeometry raster;
		String message;
		const Api& api = *state->api;
		if (!get_raster_geometry(api, page.get(), p_dpi, p_rotation, raster, message)) {
			state->fail(ERR_INVALID_PARAMETER, message);
			return {};
		}
		int x = 0;
		int y = 0;
		if (!p_position.is_finite() || !api.FPDF_PageToDevice(page.get(), 0, 0, raster.size.x, raster.size.y, raster.rotation,
			p_position.x, p_position.y, &x, &y)) {
			state->fail(ERR_INVALID_PARAMETER, "Could not convert the PDF position to pixels.");
			return {};
		}
		return Vector2i(x, y);
	}

	Vector2 PDFReader::pixel_to_page(int64_t p_page, const Vector2i& p_position, int64_t p_dpi, int64_t p_rotation) {
		std::lock_guard guard(pdfium::mutex());
		if (!state->require(Group::core)) {
			return {};
		}
		const auto page = state->page(p_page);
		if (!page) {
			return {};
		}
		RasterGeometry raster;
		String message;
		const Api& api = *state->api;
		if (!get_raster_geometry(api, page.get(), p_dpi, p_rotation, raster, message)) {
			state->fail(ERR_INVALID_PARAMETER, message);
			return {};
		}
		double x = 0.0;
		double y = 0.0;
		if (!api.FPDF_DeviceToPage(page.get(), 0, 0, raster.size.x, raster.size.y, raster.rotation,
			p_position.x, p_position.y, &x, &y)) {
			state->fail(ERR_INVALID_PARAMETER, "Could not convert pixels to a PDF position.");
			return {};
		}
		return Vector2(x, y);
	}
#else
	PDF_READER_UNAVAILABLE(int64_t, get_page_count, (), 0)
	PDF_READER_UNAVAILABLE(Ref<Image>, render, (int64_t, const Rect2i*, int64_t, int64_t, int64_t, const Color&), {})
	PDF_READER_UNAVAILABLE(Ref<Image>, get_page_thumbnail, (int64_t), {})
	PDF_READER_UNAVAILABLE(Vector2i, page_to_pixel, (int64_t, const Vector2&, int64_t, int64_t), {})
	PDF_READER_UNAVAILABLE(Vector2, pixel_to_page, (int64_t, const Vector2i&, int64_t, int64_t), {})
#endif
} // namespace godot

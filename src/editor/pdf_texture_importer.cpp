#include "pdf_texture_importer.h"

#include "core/pdf_texture.h"

#ifdef DOCRENDER_PDFIUM_ENABLED
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/variant/char_string.hpp>

#include <windows.h>
#include <fpdfview.h>

#include <cmath>
#include <limits>
#include <type_traits>

#define DOCRENDER_PDFIUM_FUNCTIONS(X) \
	X(FPDF_InitLibrary) \
	X(FPDF_DestroyLibrary) \
	X(FPDF_LoadMemDocument64) \
	X(FPDF_GetLastError) \
	X(FPDF_CloseDocument) \
	X(FPDF_GetPageCount) \
	X(FPDF_LoadPage) \
	X(FPDF_ClosePage) \
	X(FPDF_GetPageWidthF) \
	X(FPDF_GetPageHeightF) \
	X(FPDFBitmap_CreateEx) \
	X(FPDFBitmap_Destroy) \
	X(FPDFBitmap_FillRect) \
	X(FPDF_RenderPageBitmap)
#endif

namespace godot {
	struct PDFTextureImporter::Backend {
#ifdef DOCRENDER_PDFIUM_ENABLED
		HMODULE library = nullptr;
		bool initialized = false;

#define DECLARE_PDFIUM_FUNCTION(name) decltype(&::name) name = nullptr;
		DOCRENDER_PDFIUM_FUNCTIONS(DECLARE_PDFIUM_FUNCTION)
#undef DECLARE_PDFIUM_FUNCTION

		~Backend() {
			if (initialized) {
				FPDF_DestroyLibrary();
			}
			if (library != nullptr) {
				FreeLibrary(library);
			}
		}

		Error initialize() {
			static constexpr wchar_t DLL_NAME[] = L"pdfium.dll";
			constexpr DWORD PATH_CAPACITY = 32768;
			HMODULE extension_module = nullptr;
			const DWORD module_flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
			ERR_FAIL_COND_V_MSG(!GetModuleHandleExW(module_flags, DLL_NAME, &extension_module), ERR_UNAVAILABLE,
				"PDF Texture: could not locate the DocRender extension module.");
			wchar_t extension_path[PATH_CAPACITY];
			const DWORD path_length = GetModuleFileNameW(extension_module, extension_path, PATH_CAPACITY);
			ERR_FAIL_COND_V_MSG(path_length == 0 || path_length >= PATH_CAPACITY, ERR_UNAVAILABLE,
				"PDF Texture: could not determine the DocRender extension path.");
			const String path = String(extension_path).get_base_dir().path_join(String(DLL_NAME));
			const CharWideString native_path = path.replace("/", "\\").wide_string();
			library = LoadLibraryExW(native_path.get_data(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
			if (library == nullptr) {
				const uint32_t error = static_cast<uint32_t>(GetLastError());
				ERR_PRINT(vformat("PDF Texture: could not load %s (Windows error %d). Install the DocRender PDFium DLL and reimport.", path, error));
				return ERR_UNAVAILABLE;
			}

#define LOAD_PDFIUM_FUNCTION(name) \
			name = reinterpret_cast<decltype(name)>(GetProcAddress(library, #name)); \
			ERR_FAIL_NULL_V_MSG(name, ERR_UNAVAILABLE, vformat("PDF Texture: %s does not export %s.", path, #name));
			DOCRENDER_PDFIUM_FUNCTIONS(LOAD_PDFIUM_FUNCTION)
#undef LOAD_PDFIUM_FUNCTION

			FPDF_InitLibrary();
			initialized = true;
			return OK;
		}
#endif
	};

	PDFTextureImporter::PDFTextureImporter() = default;
	PDFTextureImporter::~PDFTextureImporter() = default;

	bool PDFTextureImporter::is_supported() {
#ifdef DOCRENDER_PDFIUM_ENABLED
		return true;
#else
		return false;
#endif
	}

	String PDFTextureImporter::_get_importer_name() const {
		return "docrender.pdf_texture";
	}

	String PDFTextureImporter::_get_visible_name() const {
		return tr("PDF Texture", "Godot DocRender");
	}

	PackedStringArray PDFTextureImporter::_get_recognized_extensions() const {
		PackedStringArray extensions;
		extensions.append("pdf");
		return extensions;
	}

	TypedArray<Dictionary> PDFTextureImporter::_get_import_options(const String&, int32_t) const {
		Dictionary page;
		page["name"] = "page";
		page["default_value"] = 1;
		page["property_hint"] = PROPERTY_HINT_RANGE;
		page["hint_string"] = "1,100,1,or_greater";

		Dictionary dpi;
		dpi["name"] = "dpi";
		dpi["default_value"] = PDFTexture::DEFAULT_DPI;
		dpi["property_hint"] = PROPERTY_HINT_RANGE;
		dpi["hint_string"] = "1,600,1,or_greater,suffix:dpi";

		TypedArray<Dictionary> options;
		options.append(page);
		options.append(dpi);
		return options;
	}

	String PDFTextureImporter::_get_save_extension() const {
		return "res";
	}

	String PDFTextureImporter::_get_resource_type() const {
		return PDFTexture::get_class_static();
	}

	bool PDFTextureImporter::_can_import_threaded() const {
		return false;
	}

	Error PDFTextureImporter::_import(const String& p_source_file, const String& p_save_path, const Dictionary& p_options,
		const TypedArray<String>&, const TypedArray<String>&) const {
#ifdef DOCRENDER_PDFIUM_ENABLED
		constexpr int32_t MAX_DIMENSION = 8192;
		constexpr int64_t MAX_BITMAP_BYTES = 64 * 1024 * 1024;
		constexpr int32_t BYTES_PER_PIXEL = 4;
		constexpr double POINTS_PER_INCH = 72.0;

		const int64_t page_number = p_options.get("page", 1);
		const int64_t dpi = p_options.get("dpi", PDFTexture::DEFAULT_DPI);
		ERR_FAIL_COND_V_MSG(page_number < 1, ERR_INVALID_PARAMETER, "PDF Texture: page numbers start at 1.");
		ERR_FAIL_COND_V_MSG(dpi < 1 || dpi > std::numeric_limits<int32_t>::max(), ERR_INVALID_PARAMETER,
			"PDF Texture: DPI must be a positive 32-bit integer.");

		const Ref<FileAccess> file = FileAccess::open(p_source_file, FileAccess::READ);
		ERR_FAIL_COND_V_MSG(file.is_null(), FileAccess::get_open_error(), vformat("PDF Texture: cannot open %s.", p_source_file));
		const uint64_t length = file->get_length();
		const PackedByteArray pdf = file->get_buffer(length);
		ERR_FAIL_COND_V_MSG(pdf.is_empty() || uint64_t(pdf.size()) != length, ERR_FILE_CANT_READ,
			vformat("PDF Texture: could not read the complete PDF from %s.", p_source_file));

		if (!backend) {
			auto loader = std::make_unique<Backend>();
			const Error error = loader->initialize();
			if (error != OK) {
				return error;
			}
			backend = std::move(loader);
		}

		const std::unique_ptr<std::remove_pointer_t<FPDF_DOCUMENT>, decltype(backend->FPDF_CloseDocument)> document(
			backend->FPDF_LoadMemDocument64(pdf.ptr(), pdf.size(), nullptr), backend->FPDF_CloseDocument);
		if (!document) {
			const uint32_t error = static_cast<uint32_t>(backend->FPDF_GetLastError());
			if (error == FPDF_ERR_PASSWORD) {
				ERR_PRINT(vformat("PDF Texture: %s requires a password.", p_source_file));
				return ERR_UNAUTHORIZED;
			}
			ERR_PRINT(vformat("PDF Texture: could not read %s as a PDF (PDFium error %d).", p_source_file, error));
			return ERR_FILE_CORRUPT;
		}

		const int32_t page_count = backend->FPDF_GetPageCount(document.get());
		ERR_FAIL_COND_V_MSG(page_number > page_count, ERR_INVALID_PARAMETER,
			vformat("PDF Texture: requested page %d, but %s has %d pages.", page_number, p_source_file, page_count));
		const std::unique_ptr<std::remove_pointer_t<FPDF_PAGE>, decltype(backend->FPDF_ClosePage)> page(
			backend->FPDF_LoadPage(document.get(), static_cast<int32_t>(page_number - 1)), backend->FPDF_ClosePage);
		ERR_FAIL_COND_V_MSG(!page, ERR_FILE_CORRUPT, vformat("PDF Texture: could not load page %d of %s.", page_number, p_source_file));

		const double scale = static_cast<double>(dpi) / POINTS_PER_INCH;
		const double page_width = std::ceil(backend->FPDF_GetPageWidthF(page.get()) * scale);
		const double page_height = std::ceil(backend->FPDF_GetPageHeightF(page.get()) * scale);
		ERR_FAIL_COND_V_MSG(!std::isfinite(page_width) || !std::isfinite(page_height) || page_width < 1.0 || page_height < 1.0,
			ERR_FILE_CORRUPT, vformat("PDF Texture: page %d of %s has invalid dimensions.", page_number, p_source_file));
		ERR_FAIL_COND_V_MSG(page_width > MAX_DIMENSION || page_height > MAX_DIMENSION, ERR_INVALID_PARAMETER,
			vformat("PDF Texture: page %d of %s exceeds %d pixels on one side at %d DPI. Lower the DPI.", page_number, p_source_file, MAX_DIMENSION, dpi));
		const int32_t width = static_cast<int32_t>(page_width);
		const int32_t height = static_cast<int32_t>(page_height);
		const int32_t stride = width * BYTES_PER_PIXEL;
		const int64_t bitmap_bytes = int64_t(stride) * height;
		ERR_FAIL_COND_V_MSG(bitmap_bytes > MAX_BITMAP_BYTES, ERR_INVALID_PARAMETER,
			vformat("PDF Texture: page %d of %s exceeds the 64 MiB bitmap limit at %d DPI. Lower the DPI.", page_number, p_source_file, dpi));

		PackedByteArray pixels;
		const Error resize_error = static_cast<Error>(pixels.resize(bitmap_bytes));
		ERR_FAIL_COND_V_MSG(resize_error != OK, resize_error, vformat("PDF Texture: could not allocate pixels for %s.", p_source_file));
		{
			const std::unique_ptr<std::remove_pointer_t<FPDF_BITMAP>, decltype(backend->FPDFBitmap_Destroy)> bitmap(
				backend->FPDFBitmap_CreateEx(width, height, FPDFBitmap_BGRA, pixels.ptrw(), stride), backend->FPDFBitmap_Destroy);
			ERR_FAIL_COND_V_MSG(!bitmap, ERR_CANT_CREATE, vformat("PDF Texture: could not create a bitmap for %s.", p_source_file));
			ERR_FAIL_COND_V_MSG(!backend->FPDFBitmap_FillRect(bitmap.get(), 0, 0, width, height, 0xffffffff), ERR_CANT_CREATE,
				vformat("PDF Texture: could not initialize the bitmap for %s.", p_source_file));
			backend->FPDF_RenderPageBitmap(bitmap.get(), page.get(), 0, 0, width, height, 0, FPDF_ANNOT | FPDF_REVERSE_BYTE_ORDER);
		}

		const Ref<Image> image = Image::create_from_data(width, height, false, Image::FORMAT_RGBA8, pixels);
		ERR_FAIL_COND_V_MSG(image.is_null() || image->is_empty(), ERR_CANT_CREATE, vformat("PDF Texture: could not create an image for %s.", p_source_file));
		Ref<PDFTexture> texture;
		texture.instantiate();
		texture->set_image(image);
		texture->set_page(static_cast<int32_t>(page_number));
		texture->set_dpi(static_cast<int32_t>(dpi));
		return ResourceSaver::get_singleton()->save(texture, p_save_path + String(".res"), ResourceSaver::FLAG_COMPRESS);
#else
		return ERR_UNAVAILABLE;
#endif
	}
} // namespace godot

#ifdef DOCRENDER_PDFIUM_ENABLED
#undef DOCRENDER_PDFIUM_FUNCTIONS
#endif

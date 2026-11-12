#include "pdfium_backend.h"

#include <godot_cpp/classes/file_access.hpp>

#ifdef DOCRENDER_PDFIUM_ENABLED
#include <godot_cpp/variant/char_string.hpp>

#include <algorithm>
#endif

namespace godot::pdfium {
	std::recursive_mutex& mutex() {
		static std::recursive_mutex lock;
		return lock;
	}

	Error read_file(const String& p_path, PackedByteArray& r_bytes, String& r_message) {
		const Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
		if (file.is_null()) {
			const Error error = FileAccess::get_open_error();
			r_message = vformat("Cannot open %s.", p_path);
			return error;
		}
		const uint64_t length = file->get_length();
		if (length <= uint64_t(std::numeric_limits<int64_t>::max())) {
			r_bytes = file->get_buffer(int64_t(length));
			if (!r_bytes.is_empty() && uint64_t(r_bytes.size()) == length) {
				return OK;
			}
		}
		r_message = vformat("Could not read the complete PDF from %s.", p_path);
		return ERR_FILE_CANT_READ;
	}

#ifdef DOCRENDER_PDFIUM_ENABLED
	namespace {
		std::shared_ptr<Api>& cached_api() {
			static std::shared_ptr<Api> api;
			return api;
		}
	}

	Api::~Api() {
		std::lock_guard guard(mutex());
		if (has(Group::core)) {
			FPDF_DestroyLibrary();
		}
		if (library != nullptr) {
			FreeLibrary(library);
		}
	}

	bool Api::load(String& r_message) {
		static constexpr wchar_t DLL_NAME[] = L"pdfium.dll";
		constexpr DWORD PATH_CAPACITY = 32768;
		HMODULE extension_module = nullptr;
		const DWORD module_flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
		if (!GetModuleHandleExW(module_flags, DLL_NAME, &extension_module)) {
			r_message = "Could not locate the DocRender extension module.";
			return false;
		}
		wchar_t extension_path[PATH_CAPACITY];
		const DWORD path_length = GetModuleFileNameW(extension_module, extension_path, PATH_CAPACITY);
		if (path_length == 0 || path_length >= PATH_CAPACITY) {
			r_message = "Could not determine the DocRender extension path.";
			return false;
		}
		const String path = String(extension_path).get_base_dir().path_join(String(DLL_NAME));
		const CharWideString native_path = path.replace("/", "\\").wide_string();
		library = LoadLibraryExW(native_path.get_data(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
		if (library == nullptr) {
			r_message = vformat("Could not load %s (Windows error %d). Install the DocRender PDFium DLL.", path,
				static_cast<uint32_t>(GetLastError()));
			return false;
		}

#define PDFIUM_LOAD(symbol) \
		symbol = reinterpret_cast<decltype(symbol)>(GetProcAddress(library, #symbol)); \
		if (symbol == nullptr && missing == nullptr) { missing = #symbol; }
#define PDFIUM_LOAD_GROUP(group, symbols) \
		{ \
			const char* missing = nullptr; \
			symbols(PDFIUM_LOAD) \
			available[static_cast<size_t>(Group::group)] = missing == nullptr; \
			if (Group::group == Group::core && missing != nullptr) { \
				r_message = vformat("%s does not export %s.", path, missing); \
				return false; \
			} \
		}
		PDFIUM_GROUPS(PDFIUM_LOAD_GROUP)
#undef PDFIUM_LOAD_GROUP
#undef PDFIUM_LOAD
#define PDFIUM_LOAD_WRITER(symbol) symbol = reinterpret_cast<decltype(symbol)>(GetProcAddress(library, #symbol));
		PDFIUM_WRITER_SYMBOLS(PDFIUM_LOAD_WRITER)
#undef PDFIUM_LOAD_WRITER
#define PDFIUM_SYMBOL_LOADED(symbol) && symbol != nullptr
#define PDFIUM_WRITER_AVAILABLE(group, symbols) available[static_cast<size_t>(Group::group)] = true symbols(PDFIUM_SYMBOL_LOADED);
		PDFIUM_WRITER_GROUPS(PDFIUM_WRITER_AVAILABLE)
#undef PDFIUM_WRITER_AVAILABLE
#undef PDFIUM_SYMBOL_LOADED

		FPDF_InitLibrary();
		return true;
	}

	bool Api::has(Group p_group) const {
		return available[static_cast<size_t>(p_group)];
	}

	PackedStringArray Api::capabilities(bool p_writer) const {
		PackedStringArray result;
#define PDFIUM_CAPABILITY(group, symbols) \
		if (has(Group::group)) { result.append(#group); }
		if (p_writer) {
#define PDFIUM_WRITER_CAPABILITY(group, symbols) if (has(Group::group)) { result.append(String(#group).substr(7)); }
			PDFIUM_WRITER_GROUPS(PDFIUM_WRITER_CAPABILITY)
#undef PDFIUM_WRITER_CAPABILITY
		} else {
			PDFIUM_GROUPS(PDFIUM_CAPABILITY)
		}
#undef PDFIUM_CAPABILITY
		return result;
	}

	FPDF_DOCUMENT load_document(const Api& p_api, const PackedByteArray& p_bytes, const String& p_password,
		const String& p_source, Error& r_error, String& r_message) {
		if (p_bytes.is_empty()) {
			r_error = ERR_FILE_CORRUPT;
			r_message = vformat("No PDF data in %s.", p_source);
			return nullptr;
		}
		const CharString password = p_password.utf8();
		FPDF_DOCUMENT document = p_api.FPDF_LoadMemDocument64(p_bytes.ptr(), size_t(p_bytes.size()), password.get_data());
		if (document == nullptr) {
			const uint32_t error = static_cast<uint32_t>(p_api.FPDF_GetLastError());
			r_error = error == FPDF_ERR_PASSWORD ? ERR_UNAUTHORIZED : ERR_FILE_CORRUPT;
			if (error == FPDF_ERR_PASSWORD) {
				r_message = p_password.is_empty() ? vformat("%s requires a password.", p_source) :
					vformat("Incorrect password for %s.", p_source);
			} else {
				r_message = vformat("Could not read %s as a PDF (PDFium error %d).", p_source, error);
			}
		}
		return document;
	}

	FormInfo::FormInfo() : FPDF_FORMFILLINFO{} {
		version = 1;
		FFI_Invalidate = [](FPDF_FORMFILLINFO*, FPDF_PAGE, double, double, double, double) {};
		FFI_OutputSelectedRect = FFI_Invalidate;
		FFI_SetCursor = [](FPDF_FORMFILLINFO*, int) {};
		FFI_SetTimer = [](FPDF_FORMFILLINFO*, int, TimerCallback) { return 0; };
		FFI_KillTimer = FFI_SetCursor;
		FFI_GetLocalTime = [](FPDF_FORMFILLINFO*) { return FPDF_SYSTEMTIME{}; };
		FFI_OnChange = [](FPDF_FORMFILLINFO*) {};
		FFI_GetPage = [](FPDF_FORMFILLINFO* p_info, FPDF_DOCUMENT, int p_index) {
			const auto* info = static_cast<FormInfo*>(p_info);
			return info->index == p_index ? info->page : nullptr;
		};
		FFI_GetCurrentPage = [](FPDF_FORMFILLINFO* p_info, FPDF_DOCUMENT) {
			return static_cast<FormInfo*>(p_info)->page;
		};
		FFI_GetRotation = [](FPDF_FORMFILLINFO*, FPDF_PAGE) { return 0; };
		FFI_ExecuteNamedAction = [](FPDF_FORMFILLINFO*, FPDF_BYTESTRING) {};
		FFI_SetTextFieldFocus = [](FPDF_FORMFILLINFO*, FPDF_WIDESTRING, FPDF_DWORD, FPDF_BOOL) {};
		FFI_DoURIAction = FFI_ExecuteNamedAction;
		FFI_DoGoToAction = [](FPDF_FORMFILLINFO*, int, int, float*, int) {};
	}

	std::shared_ptr<Api> acquire(String& r_message) {
		std::lock_guard guard(mutex());
		if (!cached_api()) {
			auto api = std::make_shared<Api>();
			if (!api->load(r_message)) {
				return {};
			}
			cached_api() = std::move(api);
		}
		return cached_api();
	}

	void shutdown() {
		std::lock_guard guard(mutex());
		cached_api().reset();
	}

	String utf16(const PackedByteArray& p_buffer) {
		const auto* data = reinterpret_cast<const char16_t*>(p_buffer.ptr());
		int64_t length = p_buffer.size() / sizeof(char16_t);
		if (length > 0 && data[length - 1] == 0) {
			--length;
		}
		return length > 0 ? String::utf16(data, length) : String();
	}

	String utf8(const PackedByteArray& p_buffer) {
		int64_t length = p_buffer.size();
		if (length > 0 && p_buffer[length - 1] == 0) {
			--length;
		}
		return length > 0 ? String::utf8(reinterpret_cast<const char*>(p_buffer.ptr()), length) : String();
	}

	Rect2 rect(double p_left, double p_bottom, double p_right, double p_top) {
		return Rect2(Vector2(p_left, p_bottom), Vector2(p_right - p_left, p_top - p_bottom)).abs();
	}

	Rect2 rect(const FS_RECTF& p_rect) {
		return rect(p_rect.left, p_rect.bottom, p_rect.right, p_rect.top);
	}

	Transform2D transform(const FS_MATRIX& p_matrix) {
		return Transform2D(Vector2(p_matrix.a, p_matrix.b), Vector2(p_matrix.c, p_matrix.d), Vector2(p_matrix.e, p_matrix.f));
	}

	PackedVector2Array quad(const FS_QUADPOINTSF& p_quad) {
		PackedVector2Array points;
		points.append(Vector2(p_quad.x1, p_quad.y1));
		points.append(Vector2(p_quad.x2, p_quad.y2));
		points.append(Vector2(p_quad.x3, p_quad.y3));
		points.append(Vector2(p_quad.x4, p_quad.y4));
		return points;
	}

	bool fits_bitmap(int64_t p_width, int64_t p_height) {
		constexpr int64_t MAX_PIXELS = 64 * 1024 * 1024 / 4;
		return p_width > 0 && p_height > 0 && p_width <= MAX_BITMAP_EDGE && p_height <= MAX_BITMAP_EDGE &&
			p_width * p_height <= MAX_PIXELS;
	}

	Ref<Image> bitmap_to_image(const Api& p_api, FPDF_BITMAP p_bitmap) {
		const int width = p_api.FPDFBitmap_GetWidth(p_bitmap);
		const int height = p_api.FPDFBitmap_GetHeight(p_bitmap);
		if (!fits_bitmap(width, height)) {
			return {};
		}

		const int format = p_api.FPDFBitmap_GetFormat(p_bitmap);
		int channels;
		switch (format) {
			case FPDFBitmap_Gray: channels = 1; break;
			case FPDFBitmap_BGR: channels = 3; break;
			case FPDFBitmap_BGRx:
			case FPDFBitmap_BGRA_Premul:
			case FPDFBitmap_BGRA: channels = 4; break;
			default: return {};
		}

		const int stride = p_api.FPDFBitmap_GetStride(p_bitmap);
		const auto* input = static_cast<const uint8_t*>(p_api.FPDFBitmap_GetBuffer(p_bitmap));
		if (input == nullptr || stride < width * channels) {
			return {};
		}

		PackedByteArray pixels;
		if (pixels.resize(int64_t(width) * height * 4) != OK) {
			return {};
		}
		uint8_t* output = pixels.ptrw();
		const bool premultiplied = format == FPDFBitmap_BGRA_Premul;
		const bool has_alpha = premultiplied || format == FPDFBitmap_BGRA;
		for (int y = 0; y < height; ++y) {
			const uint8_t* row = input + int64_t(y) * stride;
			for (int x = 0; x < width; ++x) {
				if (channels == 1) {
					output[0] = output[1] = output[2] = row[0];
				} else {
					output[0] = row[2];
					output[1] = row[1];
					output[2] = row[0];
				}
				const int alpha = has_alpha ? row[3] : 255;
				output[3] = alpha;
				if (premultiplied && alpha > 0 && alpha < 255) {
					for (int channel = 0; channel < 3; ++channel) {
						output[channel] = std::min(255, (output[channel] * 255 + alpha / 2) / alpha);
					}
				}
				output += 4;
				row += channels;
			}
		}
		return Image::create_from_data(width, height, false, Image::FORMAT_RGBA8, pixels);
	}
#endif
} // namespace godot::pdfium

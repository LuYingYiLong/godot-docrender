#pragma once

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/typed_array.hpp>

#include <memory>

namespace godot {
	class PDFReader : public RefCounted {
		GDCLASS(PDFReader, RefCounted)

	public:
		enum RenderFlags {
			RENDER_ANNOTATIONS = 1 << 0,
			RENDER_GRAYSCALE = 1 << 1,
			RENDER_FORM_WIDGETS = 1 << 2,
			RENDER_PRINTING = 1 << 3,
			RENDER_LCD_TEXT = 1 << 4,
			RENDER_NO_NATIVE_TEXT = 1 << 5,
			RENDER_LIMITED_IMAGE_CACHE = 1 << 6,
			RENDER_FORCE_HALFTONE = 1 << 7,
			RENDER_NO_SMOOTH_TEXT = 1 << 8,
			RENDER_NO_SMOOTH_IMAGE = 1 << 9,
			RENDER_NO_SMOOTH_PATH = 1 << 10,
		};

		enum FindFlags {
			FIND_MATCH_CASE = 1 << 0,
			FIND_WHOLE_WORD = 1 << 1,
			FIND_CONSECUTIVE = 1 << 2,
		};

		PDFReader();
		~PDFReader() override;

		static bool is_supported();
		static void shutdown_backend();

		Error open_file(const String& p_path, const String& p_password = {});
		Error open_buffer(const PackedByteArray& p_bytes, const String& p_password = {});
		void close();
		bool is_open() const;
		static bool is_available();
		static PackedStringArray get_capabilities();
		Error get_last_error() const;
		String get_last_error_message() const;
		Dictionary get_document_info();
		Dictionary get_metadata();
		int64_t get_page_count();
		Dictionary get_page_info(int64_t p_page);
		Ref<Image> render_page(int64_t p_page, int64_t p_dpi = 144, int64_t p_flags = RENDER_ANNOTATIONS, int64_t p_rotation = 0, const Color& p_background = Color(1, 1, 1, 1));
		Ref<Image> render_region(int64_t p_page, const Rect2i& p_region, int64_t p_dpi = 144, int64_t p_flags = RENDER_ANNOTATIONS, int64_t p_rotation = 0, const Color& p_background = Color(1, 1, 1, 1));
		Ref<Image> get_page_thumbnail(int64_t p_page);
		Vector2i page_to_pixel(int64_t p_page, const Vector2& p_position, int64_t p_dpi = 144, int64_t p_rotation = 0);
		Vector2 pixel_to_page(int64_t p_page, const Vector2i& p_position, int64_t p_dpi = 144, int64_t p_rotation = 0);
		int64_t get_page_char_count(int64_t p_page);
		String get_page_text(int64_t p_page, int64_t p_start = 0, int64_t p_count = -1);
		String get_page_text_in_rect(int64_t p_page, const Rect2& p_rect);
		Dictionary get_char_info(int64_t p_page, int64_t p_index);
		TypedArray<Rect2> get_text_rects(int64_t p_page, int64_t p_start = 0, int64_t p_count = -1);
		int64_t hit_test_text(int64_t p_page, const Vector2& p_position, const Vector2& p_tolerance = Vector2(2, 2));
		TypedArray<Dictionary> find_text(int64_t p_page, const String& p_query, int64_t p_flags = 0);
		TypedArray<Dictionary> get_outline();
		TypedArray<Dictionary> get_named_destinations();
		TypedArray<Dictionary> get_page_links(int64_t p_page);
		TypedArray<Dictionary> get_page_web_links(int64_t p_page);
		TypedArray<Dictionary> get_page_annotations(int64_t p_page);
		TypedArray<Dictionary> get_form_fields(int64_t p_page);
		String get_annotation_string(int64_t p_page, int64_t p_index, const String& p_key);
		TypedArray<Dictionary> get_attachments();
		PackedByteArray get_attachment_data(int64_t p_index);
		TypedArray<Dictionary> get_signatures();
		PackedByteArray get_signature_contents(int64_t p_index);
		TypedArray<Dictionary> get_page_structure(int64_t p_page);
		TypedArray<Dictionary> get_page_objects(int64_t p_page);
		Ref<Image> get_page_object_image(int64_t p_page, const PackedInt32Array& p_path, bool p_rendered = false);
		PackedByteArray get_page_object_image_data(int64_t p_page, const PackedInt32Array& p_path, bool p_decoded = false);
		PackedByteArray get_page_object_font_data(int64_t p_page, const PackedInt32Array& p_path);
		TypedArray<Dictionary> get_javascript_actions();
		TypedArray<Dictionary> get_xfa_packets();

	protected:
		static void _bind_methods();

	private:
		friend class PDFWriter;
		struct State;
		std::unique_ptr<State> state;
		Error open_bytes(PackedByteArray p_bytes, const String& p_password, const String& p_source);

		Ref<Image> render(int64_t p_page, const Rect2i* p_region, int64_t p_dpi, int64_t p_flags,
			int64_t p_rotation, const Color& p_background);
	};
} // namespace godot

VARIANT_ENUM_CAST(godot::PDFReader::RenderFlags);
VARIANT_ENUM_CAST(godot::PDFReader::FindFlags);

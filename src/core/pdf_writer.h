#pragma once

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>

#include <memory>

namespace godot {
	class PDFReader;

	class PDFWriter : public RefCounted {
		GDCLASS(PDFWriter, RefCounted)

	public:
		enum SaveFlags {
			SAVE_FULL = 0,
			SAVE_INCREMENTAL = 1,
			SAVE_REMOVE_SECURITY = 4,
			SAVE_SUBSET_NEW_FONTS = 8,
		};

		enum PageBox {
			BOX_MEDIA,
			BOX_CROP,
			BOX_BLEED,
			BOX_TRIM,
			BOX_ART,
		};

		enum FontType {
			FONT_TYPE1 = 1,
			FONT_TRUETYPE = 2,
		};

		enum FillMode {
			FILL_NONE,
			FILL_EVEN_ODD,
			FILL_WINDING,
		};

		enum LineCap {
			CAP_BUTT,
			CAP_ROUND,
			CAP_SQUARE,
		};

		enum LineJoin {
			JOIN_MITER,
			JOIN_ROUND,
			JOIN_BEVEL,
		};

		enum TextRenderMode {
			TEXT_FILL,
			TEXT_STROKE,
			TEXT_FILL_STROKE,
			TEXT_INVISIBLE,
			TEXT_FILL_CLIP,
			TEXT_STROKE_CLIP,
			TEXT_FILL_STROKE_CLIP,
			TEXT_CLIP,
		};

		enum AnnotationSubtype {
			ANNOT_TEXT = 1,
			ANNOT_LINK = 2,
			ANNOT_FREE_TEXT = 3,
			ANNOT_LINE = 4,
			ANNOT_SQUARE = 5,
			ANNOT_CIRCLE = 6,
			ANNOT_HIGHLIGHT = 9,
			ANNOT_UNDERLINE = 10,
			ANNOT_SQUIGGLY = 11,
			ANNOT_STRIKEOUT = 12,
			ANNOT_STAMP = 13,
			ANNOT_INK = 15,
			ANNOT_POPUP = 16,
			ANNOT_FILE_ATTACHMENT = 17,
			ANNOT_WIDGET = 20,
		};

		enum AnnotationFlags {
			ANNOT_FLAG_INVISIBLE = 1,
			ANNOT_FLAG_HIDDEN = 2,
			ANNOT_FLAG_PRINT = 4,
			ANNOT_FLAG_NO_ZOOM = 8,
			ANNOT_FLAG_NO_ROTATE = 16,
			ANNOT_FLAG_NO_VIEW = 32,
			ANNOT_FLAG_READ_ONLY = 64,
			ANNOT_FLAG_LOCKED = 128,
			ANNOT_FLAG_TOGGLE_NO_VIEW = 256,
		};

		enum AppearanceMode {
			APPEARANCE_NORMAL,
			APPEARANCE_ROLLOVER,
			APPEARANCE_DOWN,
		};

		enum FlattenMode {
			FLATTEN_DISPLAY,
			FLATTEN_PRINT,
		};

		PDFWriter();
		~PDFWriter() override;

		Error create_document();
		Error open_file(const String& p_path, const String& p_password = {});
		Error open_buffer(const PackedByteArray& p_bytes, const String& p_password = {});
		void close();
		bool is_open() const;
		static bool is_available();
		static PackedStringArray get_capabilities();
		Error get_last_error() const;
		String get_last_error_message() const;
		int64_t get_page_count();
		int64_t insert_page(const Vector2& p_size_pt, int64_t p_at = -1);
		Error delete_page(int64_t p_page_index);
		Error move_pages(const PackedInt32Array& p_indices, int64_t p_at);
		Error import_pages(const Ref<PDFReader>& p_reader, const PackedInt32Array& p_indices = {}, int64_t p_at = -1);
		Error import_page_range(const Ref<PDFReader>& p_reader, const String& p_range, int64_t p_at = -1);
		Error create_n_up(const Ref<PDFReader>& p_reader, const Vector2& p_size_pt, int64_t p_columns, int64_t p_rows);
		int64_t create_page_stamp(const Ref<PDFReader>& p_reader, int64_t p_page_index);
		Error set_page_box(int64_t p_page_index, int64_t p_box, const Rect2& p_rect);
		Error set_page_rotation(int64_t p_page_index, int64_t p_degrees);
		Error transform_page(int64_t p_page_index, const Transform2D& p_transform, const Variant& p_clip = {});
		Error clip_page(int64_t p_page_index, const Rect2& p_rect);
		Error transform_page_annotations(int64_t p_page_index, const Transform2D& p_transform);
		Error generate_page_content(int64_t p_page_index);
		int64_t get_object_id(int64_t p_page_index, const PackedInt32Array& p_object_path);
		int64_t get_object_child_count(int64_t p_object_id);
		int64_t get_object_child_id(int64_t p_object_id, int64_t p_index);
		Error insert_object(int64_t p_page_index, int64_t p_object_id, int64_t p_at = -1);
		Error detach_object(int64_t p_object_id);
		Error remove_object(int64_t p_object_id);
		Error set_object_matrix(int64_t p_object_id, const Transform2D& p_transform);
		Error transform_object(int64_t p_object_id, const Transform2D& p_transform);
		Error transform_object_clip(int64_t p_object_id, const Transform2D& p_transform);
		Error set_object_active(int64_t p_object_id, bool p_active);
		Error set_object_blend_mode(int64_t p_object_id, const String& p_mode);
		Error set_object_fill_color(int64_t p_object_id, const Color& p_color);
		Error set_object_stroke_color(int64_t p_object_id, const Color& p_color);
		Error set_object_stroke_width(int64_t p_object_id, float p_width);
		Error set_object_line_cap(int64_t p_object_id, int64_t p_cap);
		Error set_object_line_join(int64_t p_object_id, int64_t p_join);
		Error set_object_dash(int64_t p_object_id, const PackedFloat32Array& p_lengths, float p_phase = 0.0f);
		int64_t create_path(const Vector2& p_start);
		int64_t create_rectangle(const Rect2& p_rect);
		Error path_move_to(int64_t p_object_id, const Vector2& p_point);
		Error path_line_to(int64_t p_object_id, const Vector2& p_point);
		Error path_bezier_to(int64_t p_object_id, const Vector2& p_control_a, const Vector2& p_control_b, const Vector2& p_end);
		Error path_close(int64_t p_object_id);
		Error set_path_draw_mode(int64_t p_object_id, int64_t p_fill_mode, bool p_stroke);
		int64_t add_object_mark(int64_t p_object_id, const String& p_name);
		Error share_object_mark(int64_t p_object_id, int64_t p_source_object_id, int64_t p_mark_index);
		Error remove_object_mark(int64_t p_object_id, int64_t p_mark_index);
		Error set_object_mark_param(int64_t p_object_id, int64_t p_mark_index, const String& p_key, const Variant& p_value);
		Error remove_object_mark_param(int64_t p_object_id, int64_t p_mark_index, const String& p_key);
		int64_t load_standard_font(const String& p_name);
		int64_t load_font(const PackedByteArray& p_data, int64_t p_type, bool p_cid = false);
		int64_t load_cid_type2_font(const PackedByteArray& p_data, const String& p_to_unicode, const PackedByteArray& p_cid_to_gid);
		Error close_font(int64_t p_font_id);
		int64_t create_text(int64_t p_font_id, float p_font_size, const String& p_text);
		Error set_text(int64_t p_object_id, const String& p_text);
		Error set_text_charcodes(int64_t p_object_id, const PackedInt64Array& p_charcodes);
		Error set_text_positions(int64_t p_object_id, const PackedFloat32Array& p_positions);
		Error set_text_font_size(int64_t p_object_id, float p_size);
		Error set_text_render_mode(int64_t p_object_id, int64_t p_mode);
		int64_t create_image(const Ref<Image>& p_image);
		int64_t create_jpeg(const PackedByteArray& p_bytes);
		Error set_image(int64_t p_object_id, const Ref<Image>& p_image);
		Error set_jpeg(int64_t p_object_id, const PackedByteArray& p_bytes);
		PackedInt32Array get_supported_annotation_subtypes();
		int64_t create_annotation(int64_t p_page_index, int64_t p_subtype);
		int64_t get_annotation_id(int64_t p_page_index, int64_t p_annotation_index);
		Error remove_annotation(int64_t p_annotation_id);
		Error set_annotation_rect(int64_t p_annotation_id, const Rect2& p_rect);
		Error set_annotation_color(int64_t p_annotation_id, const Color& p_color, bool p_interior = false);
		Error set_annotation_border(int64_t p_annotation_id, const Vector2& p_radii, float p_width);
		Error set_annotation_flags(int64_t p_annotation_id, int64_t p_flags);
		Error set_annotation_string(int64_t p_annotation_id, const String& p_key, const String& p_value);
		Error set_annotation_quad(int64_t p_annotation_id, int64_t p_index, const PackedVector2Array& p_points);
		Error append_annotation_quad(int64_t p_annotation_id, const PackedVector2Array& p_points);
		int64_t add_annotation_ink_stroke(int64_t p_annotation_id, const PackedVector2Array& p_points);
		Error clear_annotation_ink(int64_t p_annotation_id);
		Error set_annotation_uri(int64_t p_annotation_id, const String& p_uri);
		Error set_annotation_file_attachment(int64_t p_annotation_id, const String& p_name, const PackedByteArray& p_bytes);
		Error set_annotation_appearance(int64_t p_annotation_id, int64_t p_mode, const Variant& p_appearance);
		Error set_annotation_font_color(int64_t p_annotation_id, const Color& p_color);
		int64_t get_annotation_object_count(int64_t p_annotation_id);
		int64_t get_annotation_object_id(int64_t p_annotation_id, int64_t p_index);
		Error append_annotation_object(int64_t p_annotation_id, int64_t p_object_id);
		Error remove_annotation_object(int64_t p_annotation_id, int64_t p_index);
		Error set_form_text(int64_t p_annotation_id, const String& p_text);
		Error set_form_option_selected(int64_t p_annotation_id, int64_t p_option_index, bool p_selected = true);
		Error set_form_checked(int64_t p_annotation_id, bool p_checked);
		Error select_form_radio(int64_t p_annotation_id);
		Error set_form_field_flags(int64_t p_annotation_id, int64_t p_flags);
		Error flatten_page(int64_t p_page_index, int64_t p_mode = 0);
		int64_t add_attachment(const String& p_name, const PackedByteArray& p_bytes);
		Error delete_attachment(int64_t p_index);
		Error set_attachment_data(int64_t p_index, const PackedByteArray& p_bytes);
		Error set_attachment_description(int64_t p_index, const String& p_description);
		Error set_attachment_string(int64_t p_index, const String& p_key, const String& p_value);
		Error set_document_language(const String& p_language);
		Error copy_viewer_preferences(const Ref<PDFReader>& p_reader);
		PackedByteArray save_to_buffer(int64_t p_flags = 0, int64_t p_version = 0);
		Error save_to_file(const String& p_path, int64_t p_flags = 0, int64_t p_version = 0);

	protected:
		static void _bind_methods();

	private:
		struct State;
		std::unique_ptr<State> state;

		Error open_bytes(PackedByteArray p_bytes, const String& p_password, const String& p_source);
	};
} // namespace godot

VARIANT_ENUM_CAST(godot::PDFWriter::SaveFlags);
VARIANT_ENUM_CAST(godot::PDFWriter::PageBox);
VARIANT_ENUM_CAST(godot::PDFWriter::FontType);
VARIANT_ENUM_CAST(godot::PDFWriter::FillMode);
VARIANT_ENUM_CAST(godot::PDFWriter::LineCap);
VARIANT_ENUM_CAST(godot::PDFWriter::LineJoin);
VARIANT_ENUM_CAST(godot::PDFWriter::TextRenderMode);
VARIANT_ENUM_CAST(godot::PDFWriter::AnnotationSubtype);
VARIANT_ENUM_CAST(godot::PDFWriter::AnnotationFlags);
VARIANT_ENUM_CAST(godot::PDFWriter::AppearanceMode);
VARIANT_ENUM_CAST(godot::PDFWriter::FlattenMode);

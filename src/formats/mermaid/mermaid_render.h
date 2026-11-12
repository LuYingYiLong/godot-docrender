#pragma once

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/hash_set.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include <memory>
#include <optional>

class MermaidRender : public godot::RefCounted {
	GDCLASS(MermaidRender, godot::RefCounted)

public:
	static bool is_available();
	static godot::Dictionary get_runtime_catalog();

	godot::Error render(const godot::String& p_source, const godot::StringName& p_operation = "png",
		const godot::Dictionary& p_options = {}, const godot::String& p_uri = {}, int64_t p_timeout_msec = 2000);
	void clear();
	godot::PackedByteArray get_bytes() const;
	godot::String get_output_text() const;
	godot::String get_media_type() const;
	godot::Dictionary get_metadata() const;
	godot::String get_render_error() const;
	godot::Ref<godot::Image> get_image() const;
	godot::Ref<godot::Texture2D> get_texture() const;

protected:
	static void _bind_methods();

private:
	godot::PackedByteArray bytes;
	godot::String media_type;
	godot::Dictionary metadata;
	godot::String render_error;
};

class MermaidRenderCache {
public:
	static constexpr float RASTER_SCALE = 2.0f;

	struct Palette {
		bool light = false;
		std::optional<godot::Color> node_background;
		std::optional<godot::Color> node_border;
		std::optional<godot::Color> node_text;
		std::optional<godot::Color> line;

		bool operator==(const Palette& p_other) const {
			return light == p_other.light && node_background == p_other.node_background &&
				node_border == p_other.node_border && node_text == p_other.node_text && line == p_other.line;
		}
	};

	struct Result {
		godot::Ref<godot::Texture2D> texture;
		godot::String error;
	};

	MermaidRenderCache();
	~MermaidRenderCache();

	void update(const godot::HashSet<godot::String>& p_sources, const Palette& p_palette);
	const Result* get_result(const godot::String& p_source) const;
	bool has_pending() const;
	bool poll();

private:
	struct Job;

	godot::HashSet<godot::String> active_sources;
	godot::HashMap<godot::String, Result> results;
	std::unique_ptr<Job> job;
	int64_t task_id = -1;
	Palette palette;

	static void render_job(void* p_data);
};

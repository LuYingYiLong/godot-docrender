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
	static bool is_available(); // 检查当前构建是否提供 Merman 后端
	static godot::Dictionary get_runtime_catalog(); // 读取 Merman 的运行时目录及可用操作

	godot::Error render(const godot::String& p_source, const godot::StringName& p_operation = "png",
		const godot::Dictionary& p_options = {}, const godot::String& p_uri = {}, int64_t p_timeout_msec = 2000); // 按指定选项执行 Merman 操作并替换上一次结果
	void clear(); // 清空渲染结果、元数据及错误信息
	godot::PackedByteArray get_bytes() const; // 返回最近一次成功操作的原始输出字节
	godot::String get_output_text() const; // 以字符串读取 SVG、ASCII 或 JSON 等文本输出
	godot::String get_media_type() const; // 返回当前输出的媒体类型
	godot::Dictionary get_metadata() const; // 返回操作元数据或结构化错误详情的副本
	godot::String get_render_error() const; // 返回最近一次失败操作的诊断信息
	godot::Ref<godot::Image> get_image() const; // 将当前 PNG 或 JPEG 输出解码为 Image
	godot::Ref<godot::Texture2D> get_texture() const; // 从当前 PNG 或 JPEG 输出创建纹理

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

		bool operator==(const Palette& p_other) const { // 比较明暗主题和各项配色是否相同
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

	void update(const godot::HashSet<godot::String>& p_sources, const Palette& p_palette); // 按当前源码集合和配色更新缓存并调度缺失图表的渲染
	const Result* get_result(const godot::String& p_source) const; // 查找指定 Mermaid 源码的缓存纹理或错误
	bool has_pending() const; // 检查是否还有待收集的后台渲染任务
	bool poll(); // 收集已完成的后台任务，有任务完成时返回 true

private:
	struct Job;

	godot::HashSet<godot::String> active_sources;
	godot::HashMap<godot::String, Result> results;
	std::unique_ptr<Job> job;
	int64_t task_id = -1;
	Palette palette;

	static void render_job(void* p_data);
};

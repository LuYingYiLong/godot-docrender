#include "mermaid_render.h"

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/worker_thread_pool.hpp>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#ifdef MARKDOWN_LABEL_MERMAN_ENABLED
#include <godot_cpp/classes/os.hpp>

#include <merman.h>

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string_view>
#endif

using namespace godot;

namespace {
	struct NativeOutput {
		Error status = FAILED;
		std::vector<uint8_t> bytes;
		std::string media_type;
		std::string metadata;
	};

	PackedByteArray copy_bytes(const std::vector<uint8_t>& p_bytes) {
		PackedByteArray bytes;
		if (!p_bytes.empty()) {
			bytes.resize(p_bytes.size());
			std::copy(p_bytes.begin(), p_bytes.end(), bytes.ptrw());
		}
		return bytes;
	}

	Dictionary decode_metadata(const std::string& p_json) {
		const Variant value = p_json.empty() ? Variant() : JSON::parse_string(String::utf8(p_json.c_str()));
		return value.get_type() == Variant::DICTIONARY ? Dictionary(value) : Dictionary();
	}

	Ref<Image> decode_image(const PackedByteArray& p_bytes, const String& p_media_type) {
		if (p_bytes.is_empty()) {
			return {};
		}
		Ref<Image> image;
		if (p_media_type == "image/png") {
			image.instantiate();
			if (image->load_png_from_buffer(p_bytes) == OK) {
				return image;
			}
		}
		else if (p_media_type == "image/jpeg") {
			image.instantiate();
			if (image->load_jpg_from_buffer(p_bytes) == OK) {
				return image;
			}
		}
		return {};
	}

	String get_diagnostic_message(const Dictionary& p_metadata) {
		const String message = p_metadata.get("message", String());
		return message.is_empty() ? String("Could not render the Mermaid diagram.") : message;
	}

#ifdef MARKDOWN_LABEL_MERMAN_ENABLED
	MermanNativeSlice slice(std::string_view p_bytes) {
		return { MERMAN_NATIVE_STRUCT_SIZE(MermanNativeSlice),
			p_bytes.empty() ? nullptr : reinterpret_cast<const uint8_t*>(p_bytes.data()), p_bytes.size() };
	}

	MermanNativeApi discover_api() {
		MermanNativeApiRequest request{};
		request.struct_size = MERMAN_NATIVE_STRUCT_SIZE(MermanNativeApiRequest);
		request.expected_abi_version = MERMAN_NATIVE_ABI_VERSION;
		request.expected_minimum_prefix_layout_digest = slice(MERMAN_NATIVE_ABI_MINIMUM_PREFIX_LAYOUT_DIGEST);

		MermanNativeApi api{};
		api.struct_size = MERMAN_NATIVE_STRUCT_SIZE(MermanNativeApi);
		if (merman_get_native_api(&request, &api) != MERMAN_NATIVE_STATUS_OK ||
			api.struct_size < MERMAN_NATIVE_API_ENGINE_NEW_WITH_FONTS_PREFIX_SIZE ||
			!api.engine_new_with_fonts || !api.engine_try_close || !api.result_free || !api.runtime_catalog ||
			!api.operation_control_new || !api.operation_control_release || !api.execute_collect_controlled) {
			return {};
		}
		return api;
	}

	const MermanNativeApi& native_api() {
		static const MermanNativeApi api = discover_api();
		return api;
	}

	Error map_status(MermanNativeStatus p_status) {
		switch (p_status) {
		case MERMAN_NATIVE_STATUS_OK:
			return OK;
		case MERMAN_NATIVE_STATUS_INVALID_ARGUMENT:
		case MERMAN_NATIVE_STATUS_UTF8_ERROR:
		case MERMAN_NATIVE_STATUS_OPTIONS_JSON_ERROR:
			return ERR_INVALID_PARAMETER;
		case MERMAN_NATIVE_STATUS_NO_DIAGRAM:
		case MERMAN_NATIVE_STATUS_PARSE_ERROR:
			return ERR_PARSE_ERROR;
		case MERMAN_NATIVE_STATUS_UNSUPPORTED_OPERATION:
			return ERR_UNAVAILABLE;
		case MERMAN_NATIVE_STATUS_RESOURCE_LIMIT_EXCEEDED:
			return ERR_PARAMETER_RANGE_ERROR;
		case MERMAN_NATIVE_STATUS_CANCELLED:
			return ERR_TIMEOUT;
		default:
			return FAILED;
		}
	}

	MermanNativeOperationCode get_operation(std::string_view p_id) {
		static constexpr std::pair<std::string_view, MermanNativeOperationCode> operations[] = {
			{ MERMAN_NATIVE_OPERATION_ID_SVG, MERMAN_NATIVE_OPERATION_SVG },
			{ MERMAN_NATIVE_OPERATION_ID_PNG, MERMAN_NATIVE_OPERATION_PNG },
			{ MERMAN_NATIVE_OPERATION_ID_JPEG, MERMAN_NATIVE_OPERATION_JPEG },
			{ MERMAN_NATIVE_OPERATION_ID_PDF, MERMAN_NATIVE_OPERATION_PDF },
			{ MERMAN_NATIVE_OPERATION_ID_ASCII, MERMAN_NATIVE_OPERATION_ASCII },
			{ MERMAN_NATIVE_OPERATION_ID_SEMANTIC_JSON, MERMAN_NATIVE_OPERATION_SEMANTIC_JSON },
			{ MERMAN_NATIVE_OPERATION_ID_LAYOUT_JSON, MERMAN_NATIVE_OPERATION_LAYOUT_JSON },
			{ MERMAN_NATIVE_OPERATION_ID_ANALYSIS_JSON, MERMAN_NATIVE_OPERATION_ANALYSIS_JSON },
			{ MERMAN_NATIVE_OPERATION_ID_ANALYSIS_FACTS_JSON, MERMAN_NATIVE_OPERATION_ANALYSIS_FACTS_JSON },
			{ MERMAN_NATIVE_OPERATION_ID_VALIDATION_JSON, MERMAN_NATIVE_OPERATION_VALIDATION_JSON },
			{ MERMAN_NATIVE_OPERATION_ID_DOCUMENT_ANALYSIS_JSON, MERMAN_NATIVE_OPERATION_DOCUMENT_ANALYSIS_JSON },
			{ MERMAN_NATIVE_OPERATION_ID_DOCUMENT_ANALYSIS_FACTS_JSON, MERMAN_NATIVE_OPERATION_DOCUMENT_ANALYSIS_FACTS_JSON },
			{ MERMAN_NATIVE_OPERATION_ID_SVG_PLAN_JSON, MERMAN_NATIVE_OPERATION_SVG_PLAN_JSON },
			{ MERMAN_NATIVE_OPERATION_ID_EDGE_GEOMETRY_JSON, MERMAN_NATIVE_OPERATION_EDGE_GEOMETRY_JSON },
		};
		for (const auto& operation : operations) {
			if (operation.first == p_id) {
				return operation.second;
			}
		}
		return MERMAN_NATIVE_OPERATION_NONE;
	}

	struct NativeResult {
		const MermanNativeApi& api;
		MermanNativeResult value = MERMAN_NATIVE_RESULT_INIT;

		~NativeResult() { api.result_free(&value); }

		std::string get_metadata() const {
			const auto& diagnostic = value.metadata_or_error_json;
			return diagnostic.len == 0 ? std::string() :
				std::string(reinterpret_cast<const char*>(diagnostic.data), diagnostic.len);
		}

		NativeOutput get_output(MermanNativeStatus p_status) const {
			NativeOutput output;
			output.status = map_status(p_status);
			output.metadata = get_metadata();
			if (output.status == OK) {
				if (value.data.len != 0) {
					output.bytes.assign(value.data.data, value.data.data + value.data.len);
				}
				if (value.media_type.len != 0) {
					output.media_type.assign(reinterpret_cast<const char*>(value.media_type.data), value.media_type.len);
				}
			}
			return output;
		}
	};

	std::string load_font(const std::string& p_path) {
		if (p_path.empty()) {
			return {};
		}
		std::ifstream file(std::filesystem::u8path(p_path), std::ios::binary | std::ios::ate);
		const std::streamoff size = file.tellg();
		if (size <= 0 || size > 32 * 1024 * 1024) {
			return {};
		}

		std::string font(static_cast<size_t>(size), '\0');
		file.seekg(0);
		if (!file.read(font.data(), size)) {
			return {};
		}
		return font;
	}

	std::string get_font_path(const String& p_source) {
		for (int64_t index = 0; index < p_source.length(); index++) {
			if (p_source[index] > 127) {
				OS* os = OS::get_singleton();
				for (const char* family : { "Microsoft YaHei", "Noto Sans SC", "SimSun" }) {
					const String path = os->get_system_font_path(family);
					if (!path.is_empty()) {
						return path.utf8().get_data();
					}
				}
				break;
			}
		}
		return {};
	}

	void set_theme_variables(Dictionary& r_variables, const std::optional<Color>& p_color, std::initializer_list<const char*> p_names) {
		if (!p_color) {
			return;
		}
		const String value = "#" + p_color->to_html(p_color->a < 1.0f);
		for (const char* name : p_names) {
			r_variables[name] = value;
		}
	}

	std::string get_render_options(const MermaidRenderCache::Palette& p_palette) {
		Dictionary variables;
		if (!p_palette.light || p_palette.node_border) {
			variables["useGradient"] = false;
		}
		set_theme_variables(variables, p_palette.node_background, { "mainBkg", "actorBkg", "stateBkg", "requirementBackground" });
		set_theme_variables(variables, p_palette.node_border, { "nodeBorder", "actorBorder", "stateBorder", "requirementBorderColor" });
		set_theme_variables(variables, p_palette.node_text, { "nodeTextColor", "primaryTextColor", "textColor", "classText",
			"actorTextColor", "signalTextColor", "stateLabelColor", "transitionLabelColor", "requirementTextColor", "relationLabelColor" });
		set_theme_variables(variables, p_palette.line, { "lineColor", "defaultLinkColor", "arrowheadColor", "actorLineColor",
			"signalColor", "transitionColor", "relationColor" });

		Dictionary flowchart;
		flowchart["htmlLabels"] = false;

		Dictionary site_config;
		site_config["theme"] = p_palette.light ? "default" : "dark";
		site_config["fontFamily"] = "Microsoft YaHei, Noto Sans SC, SimSun, Cousine, sans-serif";
		site_config["htmlLabels"] = false;
		site_config["flowchart"] = flowchart;
		if (!variables.is_empty()) {
			site_config["themeVariables"] = variables;
		}

		Dictionary svg;
		svg["pipeline"] = "resvg-safe";
		Dictionary raster;
		raster["scale"] = MermaidRenderCache::RASTER_SCALE;

		Dictionary options;
		options["site_config"] = site_config;
		options["svg"] = svg;
		options["raster"] = raster;
		return JSON::stringify(options).utf8().get_data();
	}

	NativeOutput execute_native(MermanNativeOperationCode p_operation, std::string_view p_source, const std::string& p_font_path,
		std::string_view p_engine_options, std::string_view p_request_options, std::string_view p_uri, int64_t p_timeout_msec) {
		const MermanNativeApi& api = native_api();
		MermanNativeEngineToken engine = 0;
		{
			const std::string font = load_font(p_font_path);
			const MermanNativeSlice font_data = slice(font);
			MermanNativeFontEngineConfig config{};
			config.struct_size = MERMAN_NATIVE_STRUCT_SIZE(MermanNativeFontEngineConfig);
			config.options_json = slice(p_engine_options);
			config.fonts = font.empty() ? nullptr : &font_data;
			config.font_count = font.empty() ? 0 : 1;
			config.use_bundled_fonts = 1;
			NativeResult result{ api };
			const MermanNativeStatus status = api.engine_new_with_fonts(&config, &engine, &result.value);
			if (status != MERMAN_NATIVE_STATUS_OK) {
				return result.get_output(status);
			}
		}

		MermanNativeOperationControlToken control = 0;
		NativeResult control_result{ api };
		const MermanNativeStatus control_status = api.operation_control_new(p_timeout_msec, p_timeout_msec > 0, &control, &control_result.value);
		if (control_status != MERMAN_NATIVE_STATUS_OK) {
			api.engine_try_close(engine);
			return control_result.get_output(control_status);
		}

		MermanNativeOperationRequest request{};
		request.struct_size = MERMAN_NATIVE_STRUCT_SIZE(MermanNativeOperationRequest);
		request.operation = p_operation;
		request.source = slice(p_source);
		request.options_json = slice(p_request_options);
		request.uri = slice(p_uri);
		NativeResult result{ api };
		const MermanNativeStatus status = api.execute_collect_controlled(engine, control, &request, &result.value);
		api.operation_control_release(control);
		api.engine_try_close(engine);
		return result.get_output(status);
	}
#endif
} // namespace

struct MermaidRenderCache::Job {
	String source;
	std::string utf8;
	std::string font_path;
	std::string options;
	NativeOutput output;
	Palette palette;
};

MermaidRenderCache::MermaidRenderCache() = default;

MermaidRenderCache::~MermaidRenderCache() {
	if (has_pending()) {
		WorkerThreadPool::get_singleton()->wait_for_task_completion(task_id);
	}
}

bool MermaidRender::is_available() {
#ifdef MARKDOWN_LABEL_MERMAN_ENABLED
	return native_api().execute_collect_controlled != nullptr;
#else
	return false;
#endif
}

void MermaidRender::_bind_methods() {
	ClassDB::bind_static_method("MermaidRender", D_METHOD("is_available"), &MermaidRender::is_available);
	ClassDB::bind_static_method("MermaidRender", D_METHOD("get_runtime_catalog"), &MermaidRender::get_runtime_catalog);
	ClassDB::bind_method(D_METHOD("render", "source", "operation", "options", "uri", "timeout_msec"), &MermaidRender::render,
		DEFVAL(StringName("png")), DEFVAL(Dictionary()), DEFVAL(String()), DEFVAL(2000));
	ClassDB::bind_method(D_METHOD("clear"), &MermaidRender::clear);
	ClassDB::bind_method(D_METHOD("get_bytes"), &MermaidRender::get_bytes);
	ClassDB::bind_method(D_METHOD("get_output_text"), &MermaidRender::get_output_text);
	ClassDB::bind_method(D_METHOD("get_media_type"), &MermaidRender::get_media_type);
	ClassDB::bind_method(D_METHOD("get_metadata"), &MermaidRender::get_metadata);
	ClassDB::bind_method(D_METHOD("get_render_error"), &MermaidRender::get_render_error);
	ClassDB::bind_method(D_METHOD("get_image"), &MermaidRender::get_image);
	ClassDB::bind_method(D_METHOD("get_texture"), &MermaidRender::get_texture);
}

Dictionary MermaidRender::get_runtime_catalog() {
#ifdef MARKDOWN_LABEL_MERMAN_ENABLED
	if (is_available()) {
		const MermanNativeApi& api = native_api();
		NativeResult result{ api };
		if (api.runtime_catalog(&result.value) == MERMAN_NATIVE_STATUS_OK) {
			return decode_metadata(result.get_metadata());
		}
	}
#endif
	return {};
}

Error MermaidRender::render(const String& p_source, const StringName& p_operation, const Dictionary& p_options, const String& p_uri, int64_t p_timeout_msec) {
	clear();
	if (p_timeout_msec < 0) {
		render_error = "The Mermaid render timeout must be zero or greater.";
		return ERR_INVALID_PARAMETER;
	}
	if (!is_available()) {
		render_error = "This build does not include the Merman rendering backend.";
		return ERR_UNAVAILABLE;
	}

#ifdef MARKDOWN_LABEL_MERMAN_ENABLED
	const MermanNativeOperationCode operation = get_operation(String(p_operation).utf8().get_data());
	if (operation == MERMAN_NATIVE_OPERATION_NONE) {
		render_error = "Unknown Merman operation: " + String(p_operation);
		return ERR_INVALID_PARAMETER;
	}

	const CharString source = p_source.utf8();
	const CharString options = JSON::stringify(p_options).utf8();
	const CharString uri = p_uri.utf8();
	const NativeOutput output = execute_native(operation, { source.get_data(), static_cast<size_t>(source.length()) },
		get_font_path(p_source), options.get_data(), {}, { uri.get_data(), static_cast<size_t>(uri.length()) }, p_timeout_msec);
	metadata = decode_metadata(output.metadata);
	if (output.status == OK) {
		bytes = copy_bytes(output.bytes);
		media_type = String::utf8(output.media_type.c_str());
	}
	else {
		render_error = get_diagnostic_message(metadata);
	}
	return output.status;
#else
	(void)p_source;
	(void)p_operation;
	(void)p_options;
	(void)p_uri;
	return ERR_UNAVAILABLE;
#endif
}

void MermaidRender::clear() {
	bytes.clear();
	media_type = String();
	metadata.clear();
	render_error = String();
}

PackedByteArray MermaidRender::get_bytes() const {
	return bytes;
}

String MermaidRender::get_output_text() const {
	if (media_type == "image/svg+xml" || media_type.begins_with("text/") || media_type.begins_with("application/json")) {
		return bytes.get_string_from_utf8();
	}
	return {};
}

String MermaidRender::get_media_type() const {
	return media_type;
}

Dictionary MermaidRender::get_metadata() const {
	return metadata.duplicate(true);
}

String MermaidRender::get_render_error() const {
	return render_error;
}

Ref<Image> MermaidRender::get_image() const {
	return decode_image(bytes, media_type);
}

Ref<Texture2D> MermaidRender::get_texture() const {
	const Ref<Image> image = get_image();
	return image.is_valid() ? Ref<Texture2D>(ImageTexture::create_from_image(image)) : Ref<Texture2D>();
}

void MermaidRenderCache::update(const HashSet<String>& p_sources, const Palette& p_palette) {
	active_sources = p_sources;

	if (!(palette == p_palette) || active_sources.is_empty()) {
		results.clear();
		palette = p_palette;
	}
	else {
		HashMap<String, Result> retained;
		for (const String& source : active_sources) {
			if (const Result* result = results.getptr(source)) {
				retained.insert(source, *result);
			}
		}
		results = std::move(retained);
	}

#ifdef MARKDOWN_LABEL_MERMAN_ENABLED
	if (MermaidRender::is_available() && !has_pending()) {
		for (const String& source : active_sources) {
			if (results.has(source)) {
				continue;
			}
			const CharString utf8 = source.utf8();
			job = std::make_unique<Job>();
			job->source = source;
			job->utf8.assign(utf8.get_data(), utf8.length());
			job->font_path = get_font_path(source);
			job->options = get_render_options(palette);
			job->palette = palette;
			task_id = WorkerThreadPool::get_singleton()->add_native_task(&MermaidRenderCache::render_job, job.get(), false, "Render Mermaid diagram");
			break;
		}
	}
#endif
}

const MermaidRenderCache::Result* MermaidRenderCache::get_result(const String& p_source) const {
	return results.getptr(p_source);
}

bool MermaidRenderCache::has_pending() const {
	return task_id >= 0;
}

bool MermaidRenderCache::poll() {
	if (!has_pending()) {
		return false;
	}
	WorkerThreadPool* pool = WorkerThreadPool::get_singleton();
	if (!pool->is_task_completed(task_id)) {
		return false;
	}
	pool->wait_for_task_completion(task_id);
	task_id = -1;

	if (job->palette == palette && active_sources.has(job->source)) {
		Result result;
		const Ref<Image> image = decode_image(copy_bytes(job->output.bytes), String::utf8(job->output.media_type.c_str()));
		if (image.is_valid()) {
			result.texture = ImageTexture::create_from_image(image);
		}
		if (result.texture.is_null()) {
			result.error = get_diagnostic_message(decode_metadata(job->output.metadata));
		}
		results.insert(job->source, result);
	}
	job.reset();
	return true;
}

void MermaidRenderCache::render_job(void* p_data) {
#ifdef MARKDOWN_LABEL_MERMAN_ENABLED
	Job& work = *static_cast<Job*>(p_data);
	if (work.utf8.size() <= 65536) {
		constexpr std::string_view engine_options = R"({"resources":{"profile":"interactive","limits":{"max_source_bytes":65536,"max_raster_pixels":4194304}}})";
		work.output = execute_native(MERMAN_NATIVE_OPERATION_PNG, work.utf8, work.font_path, engine_options, work.options, {}, 2000);
	}
#else
	(void)p_data;
#endif
}

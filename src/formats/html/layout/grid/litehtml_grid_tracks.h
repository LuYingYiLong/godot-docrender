#pragma once

#include "litehtml_grid_placement.h"

#include <litehtml/css_length.h>

#include <array>
#include <optional>

namespace litehtml {

enum class grid_breadth_kind { automatic, length, fraction };

struct grid_breadth {
	grid_breadth_kind kind = grid_breadth_kind::automatic;
	css_length length;
	double fraction = 0;
};

struct grid_track {
	grid_breadth minimum;
	grid_breadth maximum;
};

using grid_track_list = std::vector<grid_track>;

struct grid_track_segment {
	grid_track_list tracks;
	grid_line_indices names;
};

struct grid_auto_repeat {
	grid_track_segment pattern;
	bool fit;
};

struct grid_track_template {
	grid_track_segment prefix;
	std::optional<grid_auto_repeat> repeat;
	grid_track_segment suffix;
};

struct grid_area_template {
	std::size_t rows = 0;
	std::size_t columns = 0;
	std::unordered_map<std::string, grid_item_placement> areas;
};

struct expanded_grid_tracks : grid_track_segment {
	std::size_t explicit_count = 0;
	std::optional<grid_track_range> fit_range;
};

// Count gutters in the same way for sizing contributions and positioning.
struct grid_track_gutters {
	std::size_t track_count;
	std::vector<std::size_t> active_before;

	std::size_t active_count() const { return active_before.empty() ? track_count : active_before.back(); }
	std::size_t count_inside(grid_track_range range) const;
	bool after(std::size_t track) const;
};

struct grid_track_contribution {
	grid_track_range range;
	pixel_t minimum = 0;
	pixel_t preferred = 0;
};

struct grid_track_sizes {
	std::vector<pixel_t> tracks;
	pixel_t minimum = 0;
	pixel_t extent = 0;
};

bool parse_grid_tracks(const css_token_vector& tokens, grid_track_template& tracks);
bool parse_grid_auto_tracks(const css_token_vector& tokens, grid_track_list& tracks);
bool parse_grid_auto_flow(const css_token_vector& tokens, grid_auto_flow& flow);
bool parse_grid_template_areas(const css_token_vector& tokens, grid_area_template& areas);
expanded_grid_tracks expand_grid_tracks(const grid_track_template& tracks, const css_length& gap,
	std::optional<pixel_t> definite, std::optional<pixel_t> minimum, std::optional<pixel_t> maximum);
void apply_grid_template_areas(const grid_area_template& areas, expanded_grid_tracks& rows, expanded_grid_tracks& columns);
grid_track_gutters finish_grid_tracks(expanded_grid_tracks& axis, const grid_track_list& pattern, std::size_t offset, std::size_t count,
	const std::vector<grid_item_placement>& items, grid_track_range grid_item_placement::* dimension);
bool parse_grid_line(const css_token_vector& tokens, grid_line& line);
bool parse_grid_axis_placement(const css_token_vector& tokens, grid_axis_placement& placement);
bool parse_grid_area(const css_token_vector& tokens, std::array<grid_line, 4>& lines);
bool grid_tracks_use_percentage(const grid_track_list& tracks);
bool grid_range_needs_content(const grid_track_list& tracks, grid_track_range range, bool definite);
std::optional<pixel_t> fixed_grid_span(const grid_track_list& tracks, grid_track_range range,
	const grid_track_gutters& gutters, const css_length& gap, std::optional<pixel_t> available);
bool grid_range_has_auto_minimum(const grid_track_list& tracks, grid_track_range range, bool definite);
grid_track_sizes size_grid_tracks(const grid_track_list& tracks,
	const std::vector<grid_track_contribution>& contributions, const grid_track_gutters& gutters, pixel_t gap,
	std::optional<pixel_t> available, bool stretch);

}

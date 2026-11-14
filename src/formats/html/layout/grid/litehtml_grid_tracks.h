#pragma once

#include "litehtml_grid_placement.h"

#include <litehtml/css_length.h>

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

struct grid_auto_repeat {
	std::size_t insertion;
	grid_track_list pattern;
	bool fit;
};

struct grid_track_template {
	grid_track_list tracks;
	std::optional<grid_auto_repeat> repeat;
};

struct expanded_grid_tracks {
	grid_track_list tracks;
	std::optional<grid_track_range> fit_range;
};

// Count gutters in the same way for sizing contributions and positioning.
struct grid_track_gutters {
	std::size_t track_count;
	std::vector<std::size_t> active_before;

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
expanded_grid_tracks expand_grid_tracks(const grid_track_template& tracks, const css_length& gap,
	std::optional<pixel_t> definite, std::optional<pixel_t> minimum, std::optional<pixel_t> maximum);
grid_track_gutters finish_grid_tracks(expanded_grid_tracks& axis, std::size_t offset, std::size_t count,
	const std::vector<grid_item_placement>& items, grid_track_range grid_item_placement::* dimension);
bool parse_grid_line(const css_token_vector& tokens, grid_line& line);
bool parse_grid_axis_placement(const css_token_vector& tokens, grid_axis_placement& placement);
bool grid_tracks_use_percentage(const grid_track_list& tracks);
bool grid_range_needs_content(const grid_track_list& tracks, grid_track_range range, bool definite);
bool grid_range_has_auto_minimum(const grid_track_list& tracks, grid_track_range range, bool definite);
grid_track_sizes size_grid_tracks(const grid_track_list& tracks,
	const std::vector<grid_track_contribution>& contributions, const grid_track_gutters& gutters, pixel_t gap,
	std::optional<pixel_t> available);

}

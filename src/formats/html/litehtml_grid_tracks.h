#pragma once

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

struct grid_track_contribution {
	pixel_t minimum = 0;
	pixel_t preferred = 0;
};

struct grid_track_sizes {
	std::vector<pixel_t> tracks;
	pixel_t minimum = 0;
	pixel_t extent = 0;
};

bool parse_grid_tracks(const css_token_vector& tokens, grid_track_list& tracks);
bool grid_tracks_use_percentage(const grid_track_list& tracks);
bool grid_track_needs_content(const grid_track& track, bool definite);
grid_track_sizes size_grid_tracks(const grid_track_list& tracks,
	const std::vector<grid_track_contribution>& contributions, pixel_t gap,
	std::optional<pixel_t> available);

}

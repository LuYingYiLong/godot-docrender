#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace litehtml {
	inline constexpr int grid_track_limit = 10000;

	struct grid_line {
		enum class kind { automatic, number, span };
		kind type = kind::automatic;
		std::int64_t value = 0;
	};

	struct grid_axis_placement {
		grid_line start;
		grid_line end;
	};

	struct grid_item_lines {
		grid_axis_placement rows;
		grid_axis_placement columns;
	};

	struct grid_track_range {
		// Zero-based, with the end excluded.
		std::size_t start;
		std::size_t end;
	};

	struct grid_item_placement {
		grid_track_range rows;
		grid_track_range columns;
	};

	struct grid_placement {
		std::vector<grid_item_placement> items;
		std::size_t row_count;
		std::size_t column_count;
		std::size_t row_offset = 0;
		std::size_t column_offset = 0;
	};

	grid_placement place_grid_items(const std::vector<grid_item_lines>& items, std::size_t explicit_rows, std::size_t explicit_columns);
}
